/*
 * Copyright (c) 2002, Adam Dunkels.
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution. 
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgement:
 *        This product includes software developed by Adam Dunkels. 
 * 4. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.  
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.  
 *
 * This file is part of the Contiki desktop environment
 *
 * $Id: simpletelnet.c,v 1.7 2003/08/24 22:41:31 adamdunkels Exp $
 *
 */

#include "petsciiconv.h"
#include "uip_main.h"
#include "uip.h"
#include "ctk.h"
#include "dispatcher.h"
#include "resolv.h"
#include "telnet.h"
#include "simpletelnet.h"
#include "loader.h"

/* Telnet window */
static struct ctk_window telnetwindow;

static struct ctk_label telnethostlabel =
  {CTK_LABEL(1, 0, 4, 1, "Host")};
static char telnethost[25];
static struct ctk_textentry telnethosttextentry =
  {CTK_TEXTENTRY(0, 1, 24, 1, telnethost, 24)};

static struct ctk_label telnetportlabel =
  {CTK_LABEL(31, 0, 4, 1, "Port")};
static char telnetport[6];
static struct ctk_textentry telnetporttextentry =
  {CTK_TEXTENTRY(30, 1, 5, 1, telnetport, 5)};

static struct ctk_button telnetconnectbutton =
  {CTK_BUTTON(2, 3, 7, "Connect")};
static struct ctk_button telnetdisconnectbutton =
  {CTK_BUTTON(25, 3, 10, "Disconnect")};

static char telnetline[31];
static struct ctk_textentry telnetlinetextentry =
  {CTK_TEXTENTRY(0, 5, 30, 1, telnetline, 30)};


static struct ctk_button telnetsendbutton =
  {CTK_BUTTON(32, 5, 4, "Send")};

static struct ctk_label telnetstatus =
  {CTK_LABEL(0, 19, 38, 1, "")};

static struct ctk_separator telnetsep1 =
  {CTK_SEPARATOR(0, 7, 38)};

static struct ctk_separator telnetsep2 =
  {CTK_SEPARATOR(0, 18, 38)};

static char telnettext[38*10];
static struct ctk_label telnettextarea =
  {CTK_LABEL(0, 8, 38, 10, telnettext)};

static struct telnet_state ts_appstate;

#define ISO_NL       0x0a
#define ISO_CR       0x0d

static DISPATCHER_SIGHANDLER(simpletelnet_sighandler, s, data);
static struct dispatcher_proc p =
  {DISPATCHER_PROC("Simple telnet", NULL, simpletelnet_sighandler,
		   telnet_app)};
static ek_id_t id;

/*-----------------------------------------------------------------------------------*/
LOADER_INIT_FUNC(simpletelnet_init, arg)
{
  if(id == EK_ID_NONE) {
    id = dispatcher_start(&p);

    /* Create Telnet window. */
    ctk_window_new(&telnetwindow, 38, 20, "Simple telnet");
    
    CTK_WIDGET_ADD(&telnetwindow, &telnethostlabel);
    CTK_WIDGET_ADD(&telnetwindow, &telnetportlabel);
    CTK_WIDGET_ADD(&telnetwindow, &telnethosttextentry);
    CTK_WIDGET_ADD(&telnetwindow, &telnetporttextentry);
    CTK_WIDGET_ADD(&telnetwindow, &telnetconnectbutton);
    CTK_WIDGET_ADD(&telnetwindow, &telnetdisconnectbutton);
    CTK_WIDGET_ADD(&telnetwindow, &telnetlinetextentry);
    CTK_WIDGET_ADD(&telnetwindow, &telnetsendbutton);
    
    CTK_WIDGET_ADD(&telnetwindow, &telnetsep1);
    CTK_WIDGET_ADD(&telnetwindow, &telnettextarea);
    
    CTK_WIDGET_ADD(&telnetwindow, &telnetsep2);
    CTK_WIDGET_ADD(&telnetwindow, &telnetstatus);

    CTK_WIDGET_FOCUS(&telnetwindow, &telnethosttextentry);
       
    /* Attach as a listener to the CTK button press signal. */
    dispatcher_listen(ctk_signal_button_activate);
    dispatcher_listen(ctk_signal_window_close);
    dispatcher_listen(resolv_signal_found);
  }
  
  ctk_window_open(&telnetwindow);

}
/*-----------------------------------------------------------------------------------*/
static void
scrollup(void)
{
  unsigned char i;
  for(i = 1; i < 10; ++i) {
    memcpy(&telnettext[(i - 1) * 38], &telnettext[i * 38], 38);
  }
  memset(&telnettext[9 * 38], 0, 38);
}
/*-----------------------------------------------------------------------------------*/
static void
add_text(char *text)
{
  unsigned char i;
  unsigned int len;
  
  len = strlen(text);

  i = 0;
  while(len > 0) {
    if(*text == '\n') {
      scrollup();
      i = 0;
    } else if(*text == '\r') {
      i = 0;
    } else {
      telnettext[9 * 38 + i] = *text;
      ++i;
      if(i == 38) {
	scrollup();
	i = 0;
      }
    }
    ++text;
    --len;
  }
  
  /*  if(strlen(text) > 37) {
      memcpy(&telnettext[9 * 38], text, 37);
      } else {
      memcpy(&telnettext[9 * 38], text, strlen(text));
      }
  */
}
/*-----------------------------------------------------------------------------------*/
static void
show(char *text)
{
  add_text(text);
  add_text("\n");
  ctk_label_set_text(&telnetstatus, text);
  ctk_window_redraw(&telnetwindow);
}
/*-----------------------------------------------------------------------------------*/
static void
connect(void)
{
  u16_t addr[2], *addrptr;
  u16_t port;
  char *cptr;
  struct uip_conn *conn;

  /* Find the first space character in host and put a zero there
     to end the string. */
  for(cptr = telnethost; *cptr != ' ' && *cptr != 0; ++cptr);
  *cptr = 0;

  addrptr = &addr[0];  
  if(uip_main_ipaddrconv(telnethost, (unsigned char *)addr) == 0) {
    addrptr = resolv_lookup(telnethost);
    if(addrptr == NULL) {
      resolv_query(telnethost);
      show("Resolving host...");
      return;
    }
  }

  port = 0;
  for(cptr = telnetport; *cptr != ' ' && *cptr != 0; ++cptr) {
    if(*cptr < '0' || *cptr > '9') {
      show("Port number error");
      return;
    }
    port = 10 * port + *cptr - '0';
  }


  conn = uip_connect(addrptr, port);
  if(conn == NULL) {
    show("Out of memory error");
    return;
  }

  dispatcher_markconn(conn, &ts_appstate);

  show("Connecting...");

}
/*-----------------------------------------------------------------------------------*/
static
DISPATCHER_SIGHANDLER(simpletelnet_sighandler, s, data)
{
  struct ctk_widget *w;
  char *ptr;
  DISPATCHER_SIGHANDLER_ARGS(s, data);
  
  if(s == ctk_signal_button_activate) {
    
    w = (struct ctk_widget *)data;
    if(w == (struct ctk_widget *)&telnetsendbutton) {
      petsciiconv_toascii(telnetline, sizeof(telnetline));
      ptr = telnetline + strlen(telnetline);
      *ptr++ = ISO_CR;     
      *ptr++ = ISO_NL;
      if(telnet_send(&ts_appstate, telnetline, ptr - telnetline)) {
	/* Could not send. */
	ctk_label_set_text(&telnetstatus, "Could not send");
	ctk_window_redraw(&telnetwindow);
	/*      } else {*/
	/* Could send */
      }
    } else if(w == (struct ctk_widget *)&telnetdisconnectbutton) {
      telnet_close(&ts_appstate);
      show("Closing...");
    } else if(w == (struct ctk_widget *)&telnetconnectbutton) {
      connect();
      ctk_window_redraw(&telnetwindow);
    }
  } else if(s == resolv_signal_found) {
    if(strcmp(data, telnethost) == 0) {
      if(resolv_lookup(telnethost) != NULL) {
	connect();
      } else {
	show("Host not found");
      }
    }
  } else if(s == ctk_signal_window_close) {
    dispatcher_exit(&p);
    id = 0;
    LOADER_UNLOAD();
  }
}
/*-----------------------------------------------------------------------------------*/
void
telnet_connected(struct telnet_state *s)
{  
  show("Connected");
}
void
telnet_closed(struct telnet_state *s)
{
  show("Connection closed");
}
void
telnet_sent(struct telnet_state *s)
{
  petsciiconv_topetscii(telnetline, sizeof(telnetline));
  add_text(telnetline);
  memset(telnetline, 0, sizeof(telnetline));
  ctk_window_redraw(&telnetwindow);
}
void
telnet_aborted(struct telnet_state *s)
{
  show("Connection reset by peer");
}
void
telnet_timedout(struct telnet_state *s)
{
  show("Connection timed out");
}
void
telnet_newdata(struct telnet_state *s, char *data, u16_t len)
{
  petsciiconv_topetscii(data, len);
  data[len] = 0;
  add_text(data);
  ctk_window_redraw(&telnetwindow);
}

const char gateway_rcs[] = "$Id: gateway.c,v 1.21 2007/07/28 12:30:03 fabiankeil Exp $";
/*********************************************************************
 *
 * File        :  $Source: /cvsroot/ijbswa/current/gateway.c,v $
 *
 * Purpose     :  Contains functions to connect to a server, possibly
 *                using a "forwarder" (i.e. HTTP proxy and/or a SOCKS4
 *                proxy).
 *
 * Copyright   :  Written by and Copyright (C) 2001-2007 the SourceForge
 *                Privoxy team. http://www.privoxy.org/
 *
 *                Based on the Internet Junkbuster originally written
 *                by and Copyright (C) 1997 Anonymous Coders and
 *                Junkbusters Corporation.  http://www.junkbusters.com
 *
 *                This program is free software; you can redistribute it
 *                and/or modify it under the terms of the GNU General
 *                Public License as published by the Free Software
 *                Foundation; either version 2 of the License, or (at
 *                your option) any later version.
 *
 *                This program is distributed in the hope that it will
 *                be useful, but WITHOUT ANY WARRANTY; without even the
 *                implied warranty of MERCHANTABILITY or FITNESS FOR A
 *                PARTICULAR PURPOSE.  See the GNU General Public
 *                License for more details.
 *
 *                The GNU General Public License should be included with
 *                this file.  If not, you can view it at
 *                http://www.gnu.org/copyleft/gpl.html
 *                or write to the Free Software Foundation, Inc., 59
 *                Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Revisions   :
 *    $Log: gateway.c,v $
 *    Revision 1.21  2007/07/28 12:30:03  fabiankeil
 *    Modified patch from Song Weijia (#1762559) to
 *    fix socks requests on big-endian platforms.
 *
 *    Revision 1.20  2007/05/14 10:23:48  fabiankeil
 *    - Use strlcpy() instead of strcpy().
 *    - Use the same buffer for socks requests and socks responses.
 *    - Fix bogus warning about web_server_addr being used uninitialized.
 *
 *    Revision 1.19  2007/01/25 14:09:45  fabiankeil
 *    - Save errors in socks4_connect() to csp->error_message.
 *    - Silence some gcc43 warnings, hopefully the right way.
 *
 *    Revision 1.18  2006/07/18 14:48:46  david__schmidt
 *    Reorganizing the repository: swapping out what was HEAD (the old 3.1 branch)
 *    with what was really the latest development (the v_3_0_branch branch)
 *
 *    Revision 1.16  2002/05/12 21:36:29  jongfoster
 *    Correcting function comments
 *
 *    Revision 1.15  2002/03/26 22:29:54  swa
 *    we have a new homepage!
 *
 *    Revision 1.14  2002/03/24 13:25:43  swa
 *    name change related issues
 *
 *    Revision 1.13  2002/03/13 00:29:59  jongfoster
 *    Killing warnings
 *
 *    Revision 1.12  2002/03/09 20:03:52  jongfoster
 *    - Making various functions return int rather than size_t.
 *      (Undoing a recent change).  Since size_t is unsigned on
 *      Windows, functions like read_socket that return -1 on
 *      error cannot return a size_t.
 *
 *      THIS WAS A MAJOR BUG - it caused frequent, unpredictable
 *      crashes, and also frequently caused JB to jump to 100%
 *      CPU and stay there.  (Because it thought it had just
 *      read ((unsigned)-1) == 4Gb of data...)
 *
 *    - The signature of write_socket has changed, it now simply
 *      returns success=0/failure=nonzero.
 *
 *    - Trying to get rid of a few warnings --with-debug on
 *      Windows, I've introduced a new type "jb_socket".  This is
 *      used for the socket file descriptors.  On Windows, this
 *      is SOCKET (a typedef for unsigned).  Everywhere else, it's
 *      an int.  The error value can't be -1 any more, so it's
 *      now JB_INVALID_SOCKET (which is -1 on UNIX, and in
 *      Windows it maps to the #define INVALID_SOCKET.)
 *
 *    - The signature of bind_port has changed.
 *
 *    Revision 1.11  2002/03/08 17:46:04  jongfoster
 *    Fixing int/size_t warnings
 *
 *    Revision 1.10  2002/03/07 03:50:19  oes
 *     - Improved handling of failed DNS lookups
 *     - Fixed compiler warnings
 *
 *    Revision 1.9  2001/10/25 03:40:48  david__schmidt
 *    Change in porting tactics: OS/2's EMX porting layer doesn't allow multiple
 *    threads to call select() simultaneously.  So, it's time to do a real, live,
 *    native OS/2 port.  See defines for __EMX__ (the porting layer) vs. __OS2__
 *    (native). Both versions will work, but using __OS2__ offers multi-threading.
 *
 *    Revision 1.8  2001/09/13 20:10:12  jongfoster
 *    Fixing missing #include under Windows
 *
 *    Revision 1.7  2001/09/12 17:58:26  steudten
 *
 *    add #include <string.h>
 *
 *    Revision 1.6  2001/09/10 10:41:16  oes
 *    Added #include in.h
 *
 *    Revision 1.5  2001/07/29 18:47:57  jongfoster
 *    Adding missing #include project.h
 *
 *    Revision 1.4  2001/07/24 12:47:06  oes
 *    Applied BeOS support update by Eugenia
 *
 *    Revision 1.3  2001/06/09 10:55:28  jongfoster
 *    Changing BUFSIZ ==> BUFFER_SIZE
 *
 *    Revision 1.2  2001/06/07 23:11:38  jongfoster
 *    Removing gateways[] list - no longer used.
 *    Replacing function pointer in struct gateway with a directly
 *    called function forwarded_connect(), which can do the common
 *    task of deciding whether to connect to the web server or HTTP
 *    proxy.
 *    Replacing struct gateway with struct forward_spec
 *    Fixing bug with SOCKS4A and HTTP proxy server in combination.
 *    It was a bug which led to the connection being made to the web
 *    server rather than the HTTP proxy, and also a buffer overrun.
 *
 *    Revision 1.1.1.1  2001/05/15 13:58:54  oes
 *    Initial import of version 2.9.3 source tree
 *
 *
 *********************************************************************/


#include "config.h"

#include <stdio.h>
#include <sys/types.h>

#ifndef _WIN32
#include <netinet/in.h>
#endif

#include <errno.h>
#include <string.h>
#include "assert.h"

#ifdef _WIN32
#include <winsock2.h>
#endif /* def _WIN32 */

#ifdef __BEOS__
#include <netdb.h>
#endif /* def __BEOS__ */

#ifdef __OS2__
#include <utils.h>
#endif /* def __OS2__ */

#include "project.h"
#include "jcc.h"
#include "errlog.h"
#include "jbsockets.h"
#include "gateway.h"
#include "miscutil.h"

const char gateway_h_rcs[] = GATEWAY_H_VERSION;

static jb_socket socks4_connect(const struct forward_spec * fwd,
                                const char * target_host,
                                int target_port,
                                struct client_state *csp);


#define SOCKS_REQUEST_GRANTED          90
#define SOCKS_REQUEST_REJECT           91
#define SOCKS_REQUEST_IDENT_FAILED     92
#define SOCKS_REQUEST_IDENT_CONFLICT   93

/* structure of a socks client operation */
struct socks_op {
   unsigned char vn;          /* socks version number */
   unsigned char cd;          /* command code */
   unsigned char dstport[2];  /* destination port */
   unsigned char dstip[4];    /* destination address */
   char userid;               /* first byte of userid */
   char padding[3];           /* make sure sizeof(struct socks_op) is endian-independent. */
   /* more bytes of the userid follow, terminated by a NULL */
};

/* structure of a socks server reply */
struct socks_reply {
   unsigned char vn;          /* socks version number */
   unsigned char cd;          /* command code */
   unsigned char dstport[2];  /* destination port */
   unsigned char dstip[4];    /* destination address */
};

static const char socks_userid[] = "anonymous";


/*********************************************************************
 *
 * Function    :  forwarded_connect
 *
 * Description :  Connect to a specified web server, possibly via
 *                a HTTP proxy and/or a SOCKS proxy.
 *
 * Parameters  :
 *          1  :  fwd = the proxies to use when connecting.
 *          2  :  http = the http request and apropos headers
 *          3  :  csp = Current client state (buffers, headers, etc...)
 *
 * Returns     :  JB_INVALID_SOCKET => failure, else it is the socket file descriptor.
 *
 *********************************************************************/
jb_socket forwarded_connect(const struct forward_spec * fwd,
                            struct http_request *http,
                            struct client_state *csp)
{
   const char * dest_host;
   int dest_port;

   /* Figure out if we need to connect to the web server or a HTTP proxy. */
   if (fwd->forward_host)
   {
      /* HTTP proxy */
      dest_host = fwd->forward_host;
      dest_port = fwd->forward_port;
   }
   else
   {
      /* Web server */
      dest_host = http->host;
      dest_port = http->port;
   }

   /* Connect, maybe using a SOCKS proxy */
   switch (fwd->type)
   {
      case SOCKS_NONE:
         return (connect_to(dest_host, dest_port, csp));

      case SOCKS_4:
      case SOCKS_4A:
         return (socks4_connect(fwd, dest_host, dest_port, csp));

      default:
         /* Should never get here */
         log_error(LOG_LEVEL_FATAL, "SOCKS4 impossible internal error - bad SOCKS type.");
         errno = EINVAL;
         return(JB_INVALID_SOCKET);
   }
}


/*********************************************************************
 *
 * Function    :  socks4_connect
 *
 * Description :  Connect to the SOCKS server, and connect through
 *                it to the specified server.   This handles
 *                all the SOCKS negotiation, and returns a file
 *                descriptor for a socket which can be treated as a
 *                normal (non-SOCKS) socket.
 *
 *                Logged error messages are saved to csp->error_message
 *                and later reused by error_response() for the CGI
 *                message. strdup allocation failures are handled there.
 *
 * Parameters  :
 *          1  :  fwd = Specifies the SOCKS proxy to use.
 *          2  :  target_host = The final server to connect to.
 *          3  :  target_port = The final port to connect to.
 *          4  :  csp = Current client state (buffers, headers, etc...)
 *
 * Returns     :  JB_INVALID_SOCKET => failure, else a socket file descriptor.
 *
 *********************************************************************/
static jb_socket socks4_connect(const struct forward_spec * fwd,
                                const char * target_host,
                                int target_port,
                                struct client_state *csp)
{
   int web_server_addr;
   char buf[BUFFER_SIZE];
   struct socks_op    *c = (struct socks_op    *)buf;
   struct socks_reply *s = (struct socks_reply *)buf;
   size_t n;
   size_t csiz;
   jb_socket sfd;
   int err = 0;
   char *errstr = NULL;

   if ((fwd->gateway_host == NULL) || (*fwd->gateway_host == '\0'))
   {
      /* XXX: Shouldn't the config file parser prevent this? */
      errstr = "NULL gateway host specified.";
      err = 1;
   }

   if (fwd->gateway_port <= 0)
   {
      errstr = "invalid gateway port specified.";
      err = 1;
   }

   if (err)
   {
      log_error(LOG_LEVEL_CONNECT, "socks4_connect: %s", errstr);
      csp->error_message = strdup(errstr); 
      errno = EINVAL;
      return(JB_INVALID_SOCKET);
   }

   /* build a socks request for connection to the web server */

   strlcpy(&(c->userid), socks_userid, sizeof(buf) - sizeof(struct socks_op));

   csiz = sizeof(*c) + sizeof(socks_userid) - sizeof(c->userid) - sizeof(c->padding);

   switch (fwd->type)
   {
      case SOCKS_4:
         web_server_addr = htonl(resolve_hostname_to_ip(target_host));
         if (web_server_addr == INADDR_NONE)
         {
            errstr = "could not resolve target host";
            log_error(LOG_LEVEL_CONNECT, "socks4_connect: %s %s", errstr, target_host);
            err = 1;
         }
         break;
      case SOCKS_4A:
         web_server_addr = 0x00000001;
         n = csiz + strlen(target_host) + 1;
         if (n > sizeof(buf))
         {
            errno = EINVAL;
            errstr = "buffer cbuf too small.";
            log_error(LOG_LEVEL_CONNECT, "socks4_connect: %s", errstr);
            err = 1;
         }
         else
         {
            strlcpy(buf + csiz, target_host, sizeof(buf) - sizeof(struct socks_op) - csiz);
            /*
             * What we forward to the socks4a server should have the
             * size of socks_op, plus the length of the userid plus
             * its \0 byte (which we don't have to add because the
             * first byte of the userid is counted twice as it's also
             * part of sock_op) minus the padding bytes (which are part
             * of the userid as well), plus the length of the target_host
             * (which is stored csiz bytes after the beginning of the buffer),
             * plus another \0 byte.
             */
            assert(n == sizeof(struct socks_op) + strlen(&(c->userid)) - sizeof(c->padding) + strlen(buf + csiz) + 1);
            csiz = n;
         }
         break;
      default:
         /* Should never get here */
         log_error(LOG_LEVEL_FATAL,
            "socks4_connect: SOCKS4 impossible internal error - bad SOCKS type.");
         /* Not reached */
         return(JB_INVALID_SOCKET);
   }

   if (err)
   {
      csp->error_message = strdup(errstr);
      return(JB_INVALID_SOCKET);
   }

   c->vn          = 4;
   c->cd          = 1;
   c->dstport[0]  = (unsigned char)((target_port       >> 8  ) & 0xff);
   c->dstport[1]  = (unsigned char)((target_port             ) & 0xff);
   c->dstip[0]    = (unsigned char)((web_server_addr   >> 24 ) & 0xff);
   c->dstip[1]    = (unsigned char)((web_server_addr   >> 16 ) & 0xff);
   c->dstip[2]    = (unsigned char)((web_server_addr   >>  8 ) & 0xff);
   c->dstip[3]    = (unsigned char)((web_server_addr         ) & 0xff);

   /* pass the request to the socks server */
   sfd = connect_to(fwd->gateway_host, fwd->gateway_port, csp);

   if (sfd == JB_INVALID_SOCKET)
   {
      /*
       * XXX: connect_to should fill in the exact reason.
       * Most likely resolving the IP of the forwarder failed.
       */
      errstr = "connect_to failed: see logfile for details";
      err = 1;
   }
   else if (write_socket(sfd, (char *)c, csiz))
   {
      errstr = "SOCKS4 negotiation write failed.";
      log_error(LOG_LEVEL_CONNECT, "socks4_connect: %s", errstr);
      err = 1;
      close_socket(sfd);
   }
   else if (read_socket(sfd, buf, sizeof(buf)) != sizeof(*s))
   {
      errstr = "SOCKS4 negotiation read failed.";
      log_error(LOG_LEVEL_CONNECT, "socks4_connect: %s", errstr);
      err = 1;
      close_socket(sfd);
   }

   if (err)
   {
      csp->error_message = strdup(errstr);      
      return(JB_INVALID_SOCKET);
   }

   switch (s->cd)
   {
      case SOCKS_REQUEST_GRANTED:
         return(sfd);
         break;
      case SOCKS_REQUEST_REJECT:
         errstr = "SOCKS request rejected or failed.";
         errno = EINVAL;
         break;
      case SOCKS_REQUEST_IDENT_FAILED:
         errstr = "SOCKS request rejected because "
            "SOCKS server cannot connect to identd on the client.";
         errno = EACCES;
         break;
      case SOCKS_REQUEST_IDENT_CONFLICT:
         errstr = "SOCKS request rejected because "
            "the client program and identd report "
            "different user-ids.";
         errno = EACCES;
         break;
      default:
         errno = ENOENT;
         snprintf(buf, sizeof(buf),
            "SOCKS request rejected for reason code %d.", s->cd);
         errstr = buf;
   }

   log_error(LOG_LEVEL_CONNECT, "socks4_connect: %s", errstr);
   csp->error_message = strdup(errstr);
   close_socket(sfd);

   return(JB_INVALID_SOCKET);

}


/*
  Local Variables:
  tab-width: 3
  end:
*/

/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/ossocket.h"

#ifdef PROVIDES_SOCKET_API

#include <stdio.h>

const char* hstrerror(int err) {
  switch (err) {
    case HOST_NOT_FOUND:
      return "The specified host is unknown.";
    case NO_ADDRESS:
      return "The requested name is valid but does not have an IP address.";
    case NO_RECOVERY:
      return "A nonrecoverable name server error occurred.";
    case TRY_AGAIN:
      return "A temporary error occurred on an authoritative name server. "
             "Try again later.";
    case NETDB_INTERNAL:
      return "Internal error in gethostbyname.";
  }

  static char rtn[128];
  snprintf(rtn, sizeof(rtn), "Unknown error in gethostbyname: %d.", err);
  return rtn;
}

#endif  /* PROVIDES_SOCKET_API */

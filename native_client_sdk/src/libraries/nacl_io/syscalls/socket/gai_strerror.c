/* Copyright 2014 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/ossocket.h"

#ifdef PROVIDES_SOCKET_API

#include <stdio.h>

#if !defined(NACL_GLIBC_OLD)
#if !defined(_NEWLIB_VERSION)
const
#endif
char* gai_strerror(int errcode) {
  switch (errcode) {
    case EAI_BADFLAGS: return "Invalid value for `ai_flags' field.";
    case EAI_NONAME: return "NAME or SERVICE is unknown.";
    case EAI_AGAIN: return "Temporary failure in name resolution.";
    case EAI_FAIL: return "Non-recoverable failure in name res.";
    case EAI_FAMILY: return "`ai_family' not supported.";
    case EAI_SOCKTYPE: return "`ai_socktype' not supported.";
    case EAI_SERVICE: return "SERVICE not supported for `ai_socktype'.";
    case EAI_MEMORY: return "Memory allocation failure.";
    case EAI_SYSTEM: return "System error returned in `errno'.";
    case EAI_OVERFLOW: return "Argument buffer overflow.";
  }

  static char unknown_msg[128];
  sprintf(unknown_msg, "Unknown error in getaddrinfo: %d.", errcode);
  return unknown_msg;
}
#endif

#endif // PROVIDES_SOCKET_API

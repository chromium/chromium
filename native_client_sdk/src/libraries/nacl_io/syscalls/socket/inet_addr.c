// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/ossocket.h"

#if defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__)

#include <string.h>

in_addr_t inet_addr(const char* addr) {
   struct in_addr rtn = { 0 };
   int ret = inet_aton(addr, &rtn);
   // inet_ntoa returns zero if addr is not valid
   if (ret == 0)
     return INADDR_NONE;
   return rtn.s_addr;
}

#endif  // defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__)

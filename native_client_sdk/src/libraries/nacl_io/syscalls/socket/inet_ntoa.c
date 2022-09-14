// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/ossocket.h"

#if defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__) && !defined(__BIONIC__) && !defined(__APPLE__)

static uint8_t GetByte(const void* addr, int byte) {
  const char* buf = (const char*)addr;
  return (uint8_t)buf[byte];
}

char* inet_ntoa(struct in_addr in) {
  static char addr[INET_ADDRSTRLEN];
  snprintf(addr, INET_ADDRSTRLEN, "%u.%u.%u.%u",
           GetByte(&in, 0), GetByte(&in, 1),
           GetByte(&in, 2), GetByte(&in, 3));
  return addr;
}

#endif  // defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__) ...

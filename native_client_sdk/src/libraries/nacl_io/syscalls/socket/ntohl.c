/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/ossocket.h"

#if defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__) && !defined(__BIONIC__) && !defined(__APPLE__)

#include <string.h>

inline uint32_t ntohl(uint32_t networklong) {
  uint8_t input[4];
  memcpy(input, &networklong, 4);

  return ((((uint32_t) input[0]) << 24) |
          (((uint32_t) input[1]) << 16) |
          (((uint32_t) input[2]) << 8) |
          ((uint32_t) input[3]));
}

#endif  /* defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__) ... */

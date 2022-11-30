/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/ossocket.h"

#if defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__) && !defined(__BIONIC__) && !defined(__APPLE__)

#include <string.h>

inline uint16_t ntohs(uint16_t networkshort) {
  uint8_t input[2];
  memcpy(input, &networkshort, 2);

  return ((((uint32_t) input[0]) << 8) |
          ((uint32_t) input[1]));
}

#endif  /* defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__) ... */

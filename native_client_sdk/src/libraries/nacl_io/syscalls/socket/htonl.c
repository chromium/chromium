/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/ossocket.h"

#if defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__) && !defined(__BIONIC__) && !defined(__APPLE__)

#include <string.h>

inline uint32_t htonl(uint32_t hostlong) {
  uint8_t result_bytes[4];
  result_bytes[0] = (uint8_t) ((hostlong >> 24) & 0xFF);
  result_bytes[1] = (uint8_t) ((hostlong >> 16) & 0xFF);
  result_bytes[2] = (uint8_t) ((hostlong >> 8) & 0xFF);
  result_bytes[3] = (uint8_t) (hostlong & 0xFF);

  uint32_t result;
  memcpy(&result, result_bytes, 4);
  return result;
}

#endif  /* defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__) ... */

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/ossocket.h"
#if defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__) && !defined(__BIONIC__)

#include <errno.h>
#include <string.h>

#include <iostream>
#include <sstream>
#include <string>

#include "sdk_util/macros.h"

EXTERN_C_BEGIN

const char* inet_ntop(int af, const void* src, char* dst, socklen_t size) {
  if (AF_INET == af) {
    if (size < INET_ADDRSTRLEN) {
      errno = ENOSPC;
      return NULL;
    }
    struct in_addr in;
    memcpy(&in, src, sizeof(in));
    char* result = inet_ntoa(in);
    memcpy(dst, result, strlen(result) + 1);
    return dst;
  }

  if (AF_INET6 == af) {
    if (size < INET6_ADDRSTRLEN) {
      errno = ENOSPC;
      return NULL;
    }

    // Convert to an array of 8 host order shorts
    const uint16_t* tuples = static_cast<const uint16_t*>(src);
    uint16_t host_tuples[8];
    int zero_run_start = -1;
    int zero_run_end = -1;
    for (int i = 0; i < 8; i++) {
      host_tuples[i] = ntohs(tuples[i]);
      if (host_tuples[i] == 0) {
        if (zero_run_start == -1)
          zero_run_start = i;
      } else if (zero_run_start != -1 && zero_run_end == -1) {
        zero_run_end = i;
      }
    }

    if (zero_run_start != -1) {
      if (zero_run_end == -1)
        zero_run_end = 8;
      if (zero_run_end - zero_run_start < 2) {
        zero_run_start = -1;
        zero_run_end = -1;
      }
    }

    // Mimick glibc's behaviour here and allow ipv4 address to be specified
    // as either ::A.B.C.D or ::ffff:A.B.C.D.
    if (zero_run_start == 0 &&
        (zero_run_end == 6 ||
         (zero_run_end == 5 && host_tuples[zero_run_end] == 0xffff))) {
      if (zero_run_end == 5) {
        strcpy(dst, "::ffff:");
      } else {
        strcpy(dst, "::");
      }
      inet_ntop(AF_INET, host_tuples + 6, dst + strlen(dst), INET_ADDRSTRLEN);
    } else {
      std::stringstream output;
      for (int i = 0; i < 8; i++) {
        if (i == zero_run_start) {
          output << "::";
          continue;
        }
        if (i > zero_run_start && i < zero_run_end)
          continue;
        output << std::hex << host_tuples[i];
        if (i < 7 && i + 1 != zero_run_start)
          output << ":";
      }
      memcpy(dst, output.str().c_str(), output.str().size() + 1);
    }
    return dst;
  }

  errno = EAFNOSUPPORT;
  return NULL;
}

EXTERN_C_END

#endif  // defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__) ...

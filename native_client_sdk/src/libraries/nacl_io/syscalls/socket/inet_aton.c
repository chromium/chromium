// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nacl_io/ossocket.h"

#if defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__)

#include <stdio.h>
#include <string.h>
#include <limits.h>

int inet_aton(const char *cp, struct in_addr *inp) {
  unsigned int p1 = 0, p2 = 0, p3 = 0, p4 = 0;
  int consumed = 0;
  int matched = sscanf(cp, "%u.%u.%u.%u%n", &p1, &p2, &p3, &p4,
                       &consumed);

  if (strlen(cp) == consumed && matched == 4) {
    if ((p1 | p2 | p3 | p4) <= UCHAR_MAX) {
      in_addr_t host_order_addr = (p1 << 24) | (p2 << 16) | (p3 << 8) | p4;
      inp->s_addr = htonl(host_order_addr);
      return 1;
    }
  }

  matched = sscanf(cp, "%u.%u.%u%n", &p1, &p2, &p3, &consumed);
  if (strlen(cp) == consumed && matched == 3) {
    if ((p1 | p2) <= UCHAR_MAX && p3 <= USHRT_MAX) {
      in_addr_t host_order_addr = (p1 << 24) | (p2 << 16) | p3;
      inp->s_addr = htonl(host_order_addr);
      return 1;
    }
  }

  matched = sscanf(cp, "%u.%u%n", &p1, &p2, &consumed);
  if (strlen(cp) == consumed && matched == 2) {
    if (p1 <= UCHAR_MAX && p1 <= 1 << 24) {
      in_addr_t host_order_addr = (p1 << 24) | p2;
      inp->s_addr = htonl(host_order_addr);
      return 1;
    }
  }

  matched = sscanf(cp, "%u%n", &p1, &consumed);
  if (strlen(cp) == consumed && matched == 1) {
    inp->s_addr = htonl(p1);
    return 1;
  }

  // Failure
  return 0;
}

#endif  // defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__)

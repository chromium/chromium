/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "nacl_io/ossocket.h"
#if defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__) && !defined(__BIONIC__)

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "sdk_util/macros.h"

enum {
  kIpv4AddressSize = sizeof(in_addr_t),
  kIpv6AddressSize = sizeof(struct in6_addr),
};

/* Helper function for inet_pton() for IPv4 addresses. */
static int inet_pton_v4(const char* src, void* dst) {
  const char* pos = src;
  uint8_t result[kIpv4AddressSize] = {0};

  int i;
  for (i = 0; i < kIpv4AddressSize; ++i) {
    /* strtol() won't treat whitespace characters in the beginning as an error,
     * so check to ensure this is started with digit before passing to strtol().
     */
    if (isspace((int)(*pos)))
      return 0;
    char* end_pos;
    unsigned long value = strtoul(pos, &end_pos, 10);
    if (value > 255 || pos == end_pos)
      return 0;
    result[i] = (unsigned char)value;
    pos = end_pos;

    if (i < (kIpv4AddressSize - 1)) {
      if (*pos != '.')
        return 0;
      ++pos;
    }
  }
  if (*pos != '\0')
    return 0;
  memcpy(dst, result, sizeof(result));
  return 1;
}

/* Helper function for inet_pton() for IPv6 addresses. */
int inet_pton_v6(const char* src, void* dst) {
  /* strtol() skips 0x in from of a number, while it's not allowed in IPv6
   * addresses. Check that there is no 'x' in the string. */
  const char* pos = src;
  while (*pos != '\0') {
    if (*pos == 'x')
      return 0;
    pos++;
  }
  pos = src;

  uint8_t result[kIpv6AddressSize];
  memset(&result, 0, sizeof(result));
  int double_colon_pos = -1;
  int result_pos = 0;

  if (*pos == ':') {
    if (*(pos + 1) != ':')
      return 0;
    pos += 2;
    double_colon_pos = 0;
  }

  while (*pos != '\0') {
    /* strtol() won't treat whitespace characters in the beginning as an error,
     * so check to ensure this is started with digit before passing to strtol().
     */
    if (isspace((int)(*pos)))
      return 0;
    char* end_pos;
    unsigned long word = strtoul(pos, &end_pos, 16);
    if (word > 0xffff || pos == end_pos)
      return 0;

    if (*end_pos == '.')  {
      if (result_pos + kIpv4AddressSize > kIpv6AddressSize)
        return 0;
      /* Parse rest of address as IPv4 address. */
      if (!inet_pton_v4(pos, result + result_pos))
        return 0;
      result_pos += 4;
      break;
    }

    if (result_pos > kIpv6AddressSize - 2)
      return 0;
    result[result_pos] = (word & 0xFF00) >> 8;
    result[result_pos + 1] = word & 0xFF;
    result_pos += 2;

    if (*end_pos == '\0')
      break;

    if (*end_pos != ':')
      return 0;

    pos = end_pos + 1;
    if (*pos == ':') {
      if (double_colon_pos != -1)
        return 0;
      double_colon_pos = result_pos;
      ++pos;
    }
  }

  /* Finally move the data to the end in case the address contained '::'. */
  if (result_pos < kIpv6AddressSize) {
    if (double_colon_pos == -1)
      return 0;
    int move_size = result_pos - double_colon_pos;
    int gap_size = kIpv6AddressSize - result_pos;
    memmove(result + kIpv6AddressSize - move_size,
            result + double_colon_pos, move_size);
    memset(result + double_colon_pos, 0, gap_size);
  }

  /* Finally copy the result to the output buffer. */
  memcpy(dst, result, sizeof(result));

  return 1;
}

int inet_pton(int af, const char *src, void *dst) {
  if (!src || !dst) {
    return 0;
  }
  if (af == AF_INET) {
    return inet_pton_v4(src, dst);
  } else if (af == AF_INET6) {
    return inet_pton_v6(src, dst);
  }
  errno = EAFNOSUPPORT;
  return -1;
}

#endif  /* defined(PROVIDES_SOCKET_API) && !defined(__GLIBC__) ... */

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_address_util.h"

#if BUILDFLAG(IS_WIN)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif

#include <stdint.h>

#include "base/check.h"
#include "base/containers/span.h"
#include "net/base/ip_address.h"

namespace net {

in_addr ToInAddr(const IPAddress& ip_address) {
  static_assert(sizeof(in_addr) == IPAddress::kIPv4AddressSize,
                "Address size mismatch");

  auto span = ip_address.bytes().span();
  in_addr ret;
  // This CHECKs size the span's size and the size of an in_addr aren't the
  // same.
  base::byte_span_from_ref(ret).copy_from(span);
  return ret;
}

in6_addr ToIn6Addr(const IPAddress& ip_address) {
  static_assert(sizeof(in6_addr) == IPAddress::kIPv6AddressSize,
                "Address size mismatch");

  auto span = ip_address.bytes().span();
  in6_addr ret;
  // This CHECKs size the span's size and the size of an in6_addr aren't the
  // same.
  base::byte_span_from_ref(ret).copy_from(span);
  return ret;
}

}  // namespace net

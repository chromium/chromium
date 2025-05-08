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

#include <array>

#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

TEST(IpAddressUtil, ToInAddr) {
  auto raw_ip_address = std::to_array<uint8_t>({1, 2, 3, 4});
  IPAddress ip_address(raw_ip_address);
  in_addr addr = ToInAddr(ip_address);

  // Since the fields within in_addr vary by platform (but not the struct size),
  // have to convert it to a common type to perform a comparison.
  auto addr_as_span = base::byte_span_from_ref(addr);
  EXPECT_EQ(addr_as_span, raw_ip_address);
}

TEST(IpAddressUtil, ToIn6Addr) {
  auto raw_ip_address = std::to_array<uint8_t>(
      {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16});
  IPAddress ip_address(raw_ip_address);
  in6_addr addr = ToIn6Addr(ip_address);

  // Since the fields within in6_addr vary by platform (but not the struct
  // size), have to convert it to a common type to perform a comparison.
  auto addr_as_span = base::byte_span_from_ref(addr);
  EXPECT_EQ(addr_as_span, raw_ip_address);
}

}  // namespace net

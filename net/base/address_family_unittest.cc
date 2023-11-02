// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/address_family.h"

#include "net/base/ip_address.h"
#include "net/base/sys_addrinfo.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

TEST(AddressFamilyTest, GetAddressFamily) {
  IPAddress address;
  EXPECT_EQ(ADDRESS_FAMILY_UNSPECIFIED, GetAddressFamily(address));
  EXPECT_TRUE(address.AssignFromIPLiteral("192.168.0.1"));
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, GetAddressFamily(address));
  EXPECT_TRUE(address.AssignFromIPLiteral("1:abcd::3:4:ff"));
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, GetAddressFamily(address));
}

TEST(AddressFamilyTest, ToAddressFamily) {
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, ToAddressFamily(AF_INET));
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, ToAddressFamily(AF_INET6));
  EXPECT_EQ(ADDRESS_FAMILY_UNSPECIFIED, ToAddressFamily(AF_UNSPEC));
}

}  // namespace
}  // namespace net

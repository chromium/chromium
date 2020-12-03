// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ip_address_space_util.h"

#include "net/base/ip_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

using mojom::IPAddressSpace;
using net::IPAddress;
using net::IPAddressBytes;

IPAddress PublicIPv4Address() {
  return IPAddress(64, 233, 160, 0);
}

IPAddress PrivateIPv4Address() {
  return IPAddress(192, 168, 1, 1);
}

TEST(IPAddressSpaceTest, IPAddressToIPAddressSpacev4) {
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress()), IPAddressSpace::kUnknown);

  EXPECT_EQ(IPAddressToIPAddressSpace(PublicIPv4Address()),
            IPAddressSpace::kPublic);

  EXPECT_EQ(IPAddressToIPAddressSpace(PrivateIPv4Address()),
            IPAddressSpace::kPrivate);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(10, 1, 1, 1)),
            IPAddressSpace::kPrivate);

  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress::IPv4Localhost()),
            IPAddressSpace::kLocal);
}

IPAddressBytes IPv6BytesWithPrefix(uint8_t prefix) {
  IPAddressBytes bytes;
  bytes.Resize(IPAddress::kIPv6AddressSize);
  bytes.data()[0] = prefix;
  return bytes;
}

TEST(IPAddressSpaceTest, IPAddressToAddressSpacev6) {
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(IPv6BytesWithPrefix(42))),
            IPAddressSpace::kPublic);

  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(IPv6BytesWithPrefix(0xfd))),
            IPAddressSpace::kPrivate);

  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress::IPv6Localhost()),
            IPAddressSpace::kLocal);
}

TEST(IPAddressSpaceTest, IsLessPublicAddressSpaceThanLocal) {
  EXPECT_FALSE(
      IsLessPublicAddressSpace(IPAddressSpace::kLocal, IPAddressSpace::kLocal));

  EXPECT_TRUE(IsLessPublicAddressSpace(IPAddressSpace::kLocal,
                                       IPAddressSpace::kPrivate));
  EXPECT_TRUE(IsLessPublicAddressSpace(IPAddressSpace::kLocal,
                                       IPAddressSpace::kPublic));
  EXPECT_TRUE(IsLessPublicAddressSpace(IPAddressSpace::kLocal,
                                       IPAddressSpace::kUnknown));
}

TEST(IPAddressSpaceTest, IsLessPublicAddressSpaceThanPrivate) {
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPrivate,
                                        IPAddressSpace::kLocal));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPrivate,
                                        IPAddressSpace::kPrivate));

  EXPECT_TRUE(IsLessPublicAddressSpace(IPAddressSpace::kPrivate,
                                       IPAddressSpace::kPublic));
  EXPECT_TRUE(IsLessPublicAddressSpace(IPAddressSpace::kPrivate,
                                       IPAddressSpace::kUnknown));
}

TEST(IPAddressSpaceTest, IsLessPublicAddressSpaceThanPublic) {
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPublic,
                                        IPAddressSpace::kLocal));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPublic,
                                        IPAddressSpace::kPrivate));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPublic,
                                        IPAddressSpace::kPublic));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPublic,
                                        IPAddressSpace::kUnknown));
}

TEST(IPAddressSpaceTest, IsLessPublicAddressSpaceThanUnknown) {
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kUnknown,
                                        IPAddressSpace::kLocal));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kUnknown,
                                        IPAddressSpace::kPrivate));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kUnknown,
                                        IPAddressSpace::kPublic));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kUnknown,
                                        IPAddressSpace::kUnknown));
}

}  // namespace
}  // namespace network

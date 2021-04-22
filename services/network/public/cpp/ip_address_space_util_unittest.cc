// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ip_address_space_util.h"

#include "base/command_line.h"
#include "net/base/ip_address.h"
#include "services/network/public/cpp/network_switches.h"
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

void ParseURLSafeIPAddressTo(base::StringPiece str, IPAddress& address) {
  // ASSERT can only be used in functions returning void, hence the existence of
  // this helper function.
  ASSERT_TRUE(net::ParseURLHostnameToAddress(str, &address))
      << "Invalid IP address literal: " << str;
}

// Wrapper around net::ParseURLHostnameToAddress for easier use in tests.
IPAddress ParseURLSafeIPAddress(base::StringPiece str) {
  IPAddress address;
  ParseURLSafeIPAddressTo(str, address);
  return address;
}

TEST(IPAddressSpaceTest, IPAddressToIPAddressSpaceV4) {
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

TEST(IPAddressSpaceTest, IPAddressToAddressSpaceV6) {
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(IPv6BytesWithPrefix(42))),
            IPAddressSpace::kPublic);

  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(IPv6BytesWithPrefix(0xfd))),
            IPAddressSpace::kPrivate);

  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress::IPv6Localhost()),
            IPAddressSpace::kLocal);
}

// Verifies that IPv4-mapped IPv6 addresses belong to the address space of the
// mapped IPv4 address.
TEST(IPAddressSpaceTest, IPAddressToAddressSpaceIPv4MappedIPv6) {
  EXPECT_EQ(IPAddressToIPAddressSpace(
                net::ConvertIPv4ToIPv4MappedIPv6(PublicIPv4Address())),
            IPAddressSpace::kPublic);

  EXPECT_EQ(IPAddressToIPAddressSpace(
                net::ConvertIPv4ToIPv4MappedIPv6(PrivateIPv4Address())),
            IPAddressSpace::kPrivate);

  // TODO(https://crbug.com/1201170): Expect kLocal here.
  EXPECT_EQ(IPAddressToIPAddressSpace(
                net::ConvertIPv4ToIPv4MappedIPv6(IPAddress::IPv4Localhost())),
            IPAddressSpace::kPrivate);
}

// Verifies that the `ip-address-space-overrides` switch can be present and
// empty, in which case it is ignored.
TEST(IPAddressSpaceTest, IPAddressToAddressSpaceOverrideEmpty) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides, "");

  // Check a single address, to make sure things do not crash.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress::IPv6Localhost()),
            IPAddressSpace::kLocal);
}

// Verifies that a single IPv4 address space can be overridden.
TEST(IPAddressSpaceTest, IPAddressToAddressSpaceOverrideSingle) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides,
                                 "127.1.0.1/24=public");

  // 1 bit lower than the lower bound.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 0, 255, 255)),
            IPAddressSpace::kLocal);

  // Lower and upper bound.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 1, 0, 0)),
            IPAddressSpace::kPublic);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 1, 0, 255)),
            IPAddressSpace::kPublic);

  // 1 bit higher than the upper bound.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 1, 1, 0)),
            IPAddressSpace::kLocal);
}

// Verifies that multiple IPv4 address spaces can be overridden.
TEST(IPAddressSpaceTest, IPAddressToAddressSpaceOverrideMultiple) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides,
                                 "127.1.0.1/16=public,127.2.0.1/16=private");

  // First override block.

  // 1 bit lower than the lower bound.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 0, 255, 255)),
            IPAddressSpace::kLocal);

  // Lower and upper bound.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 1, 0, 0)),
            IPAddressSpace::kPublic);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 1, 255, 255)),
            IPAddressSpace::kPublic);

  // Second override block (contiguous with first block).

  // Lower and upper bound.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 2, 0, 0)),
            IPAddressSpace::kPrivate);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 2, 255, 255)),
            IPAddressSpace::kPrivate);

  // 1 bit higher than the upper bound.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 3, 0, 0)),
            IPAddressSpace::kLocal);
}

// Verifies that invalid entries in the command-line switch comma-separated list
// are simply ignored, and that subsequent entries are still applied.
TEST(IPAddressSpaceTest, IPAddressToAddressSpaceOverrideInvalid) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(
      switches::kIpAddressSpaceOverrides,
      "127.1.0.1/16=public,potato,,127.2.0.1/16=private");

  // First valid override block applies.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 1, 0, 0)),
            IPAddressSpace::kPublic);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 1, 255, 255)),
            IPAddressSpace::kPublic);

  // Second valid override block applies, despite preceding garbage.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 2, 0, 0)),
            IPAddressSpace::kPrivate);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 2, 255, 255)),
            IPAddressSpace::kPrivate);
}

// Verifies that command-line overrides that overlap with previously-given
// overrides are ignored. In other words, the first matching override is
// applied.
TEST(IPAddressSpaceTest, IPAddressToAddressSpaceOverrideOverlap) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides,
                                 "127.1.0.1/24=public,127.1.0.1/16=private");

  // The first matching override block applies.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 1, 0, 0)),
            IPAddressSpace::kPublic);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 1, 0, 255)),
            IPAddressSpace::kPublic);

  // Same here, but the first block no longer matches.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 1, 1, 0)),
            IPAddressSpace::kPrivate);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 1, 255, 255)),
            IPAddressSpace::kPrivate);
}

// Verifies that invalid IP addresses are not subject to overrides.
TEST(IPAddressSpaceTest, IPAddressToAddressSpaceOverrideInvalidAddress) {
  // 0.0.0.0/0 is not a valid CIDR block apparently. We cover the entirety of
  // IPv4 addresses using two /1 blocks instead.
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides,
                                 "0.0.0.0/1=local,128.0.0.0/1=local");

  // Check that the override *does not apply* to an invalid IP address.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress()), IPAddressSpace::kUnknown);

  // Check that the override applies to all valid IPv4 addresses.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(0, 0, 0, 0)),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(PublicIPv4Address()),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(255, 255, 255, 255)),
            IPAddressSpace::kLocal);
}

// Verifies that command-line overrides can specify IPv6 subnets.
TEST(IPAddressSpaceTest, IPAddressToAddressSpaceOverrideV6) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides,
                                 "2001::/16=local,2020::/24=private");

  // First override block.

  // 1 bit lower than lower bound.
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseURLSafeIPAddress(
                "[2000:ffff:ffff:ffff:ffff:ffff:ffff:ffff]")),
            IPAddressSpace::kPublic);

  // Lower and upper bound.
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseURLSafeIPAddress("[2001::]")),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseURLSafeIPAddress(
                "[2001:ffff:ffff:ffff:ffff:ffff:ffff:ffff]")),
            IPAddressSpace::kLocal);

  // 1 bit higher than upper bound.
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseURLSafeIPAddress("[2002::]")),
            IPAddressSpace::kPublic);

  // Second override block.

  // 1 bit lower than lower bound.
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseURLSafeIPAddress(
                "[2019:ffff:ffff:ffff:ffff:ffff:ffff:ffff]")),
            IPAddressSpace::kPublic);

  // Lower and upper bound.
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseURLSafeIPAddress("[2020::]")),
            IPAddressSpace::kPrivate);
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseURLSafeIPAddress(
                "[2020:00ff:ffff:ffff:ffff:ffff:ffff:ffff]")),
            IPAddressSpace::kPrivate);

  // 1 bit higher than upper bound.
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseURLSafeIPAddress("[2020:100::]")),
            IPAddressSpace::kPublic);

  // IPv4 addresses are unaffected.
  EXPECT_EQ(IPAddressToIPAddressSpace(PrivateIPv4Address()),
            IPAddressSpace::kPrivate);
  EXPECT_EQ(IPAddressToIPAddressSpace(PublicIPv4Address()),
            IPAddressSpace::kPublic);
}

// Verifies that IPv4-mapped IPv6 addresses are overridden as though they were
// the mapped IPv4 address instead. This is a quirk of the implementation that
// we test for completeness - it is not particularly useful.
TEST(IPAddressSpaceTest, IPAddressToAddressSpaceOverrideIPv4MappedIPv6) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides,
                                 "127.1.0.0/16=public");

  // Check that the override applies to all valid IPv4 addresses.
  EXPECT_EQ(IPAddressToIPAddressSpace(
                net::ConvertIPv4ToIPv4MappedIPv6(IPAddress(127, 1, 0, 0))),
            IPAddressSpace::kPublic);

  // TODO(https://crbug.com/1201170): Expect kLocal here.
  EXPECT_EQ(IPAddressToIPAddressSpace(
                net::ConvertIPv4ToIPv4MappedIPv6(IPAddress(127, 2, 0, 0))),
            IPAddressSpace::kPrivate);
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

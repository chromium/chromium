// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/base/ip_address.h"

#include <optional>
#include <vector>

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::Optional;

namespace net {

namespace {

// Helper to stringize an IP address (used to define expectations).
std::string DumpIPAddress(const IPAddress& v) {
  std::string out;
  for (size_t i = 0; i < v.bytes().size(); ++i) {
    if (i != 0)
      out.append(",");
    out.append(base::NumberToString(v.bytes()[i]));
  }
  return out;
}

TEST(IPAddressBytesTest, ConstructEmpty) {
  IPAddressBytes bytes;
  ASSERT_EQ(0u, bytes.size());
}

TEST(IPAddressBytesTest, ConstructIPv4) {
  uint8_t data[] = {192, 168, 1, 1};
  IPAddressBytes bytes(data);
  ASSERT_EQ(std::size(data), bytes.size());
  size_t i = 0;
  for (uint8_t byte : bytes)
    EXPECT_EQ(data[i++], byte);
  ASSERT_EQ(std::size(data), i);
}

TEST(IPAddressBytesTest, ConstructIPv6) {
  uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  IPAddressBytes bytes(data);
  ASSERT_EQ(std::size(data), bytes.size());
  size_t i = 0;
  for (uint8_t byte : bytes)
    EXPECT_EQ(data[i++], byte);
  ASSERT_EQ(std::size(data), i);
}

TEST(IPAddressBytesTest, Assign) {
  uint8_t data[] = {192, 168, 1, 1};
  IPAddressBytes copy;
  copy.Assign(data);
  EXPECT_EQ(IPAddressBytes(data), copy);
}

TEST(IPAddressTest, ConstructIPv4) {
  EXPECT_EQ("127.0.0.1", IPAddress::IPv4Localhost().ToString());

  IPAddress ipv4_ctor(192, 168, 1, 1);
  EXPECT_EQ("192.168.1.1", ipv4_ctor.ToString());
}

TEST(IPAddressTest, IsIPVersion) {
  uint8_t addr1[4] = {192, 168, 0, 1};
  IPAddress ip_address1(addr1);
  EXPECT_TRUE(ip_address1.IsIPv4());
  EXPECT_FALSE(ip_address1.IsIPv6());

  uint8_t addr2[16] = {0xFE, 0xDC, 0xBA, 0x98};
  IPAddress ip_address2(addr2);
  EXPECT_TRUE(ip_address2.IsIPv6());
  EXPECT_FALSE(ip_address2.IsIPv4());

  IPAddress ip_address3;
  EXPECT_FALSE(ip_address3.IsIPv6());
  EXPECT_FALSE(ip_address3.IsIPv4());
}

TEST(IPAddressTest, IsValid) {
  uint8_t addr1[4] = {192, 168, 0, 1};
  IPAddress ip_address1(addr1);
  EXPECT_TRUE(ip_address1.IsValid());
  EXPECT_FALSE(ip_address1.empty());

  uint8_t addr2[16] = {0xFE, 0xDC, 0xBA, 0x98};
  IPAddress ip_address2(addr2);
  EXPECT_TRUE(ip_address2.IsValid());
  EXPECT_FALSE(ip_address2.empty());

  uint8_t addr3[5] = {0xFE, 0xDC, 0xBA, 0x98};
  IPAddress ip_address3(addr3);
  EXPECT_FALSE(ip_address3.IsValid());
  EXPECT_FALSE(ip_address3.empty());

  IPAddress ip_address4;
  EXPECT_FALSE(ip_address4.IsValid());
  EXPECT_TRUE(ip_address4.empty());
}

enum IPAddressReservedResult : bool { NOT_RESERVED = false, RESERVED = true };

// Tests for the reserved IPv4 ranges and the (unreserved) blocks in between.
// The reserved ranges are tested by checking the first and last address of each
// range. The unreserved blocks are tested similarly. These tests cover the
// entire IPv4 address range, as well as this range mapped to IPv6.
TEST(IPAddressTest, IsPubliclyRoutableIPv4) {
  struct {
    const char* const address;
    IPAddressReservedResult is_reserved;
  } tests[] = {// 0.0.0.0/8
               {"0.0.0.0", RESERVED},
               {"0.255.255.255", RESERVED},
               // Unreserved block(s)
               {"1.0.0.0", NOT_RESERVED},
               {"9.255.255.255", NOT_RESERVED},
               // 10.0.0.0/8
               {"10.0.0.0", RESERVED},
               {"10.255.255.255", RESERVED},
               // Unreserved block(s)
               {"11.0.0.0", NOT_RESERVED},
               {"100.63.255.255", NOT_RESERVED},
               // 100.64.0.0/10
               {"100.64.0.0", RESERVED},
               {"100.127.255.255", RESERVED},
               // Unreserved block(s)
               {"100.128.0.0", NOT_RESERVED},
               {"126.255.255.255", NOT_RESERVED},
               // 127.0.0.0/8
               {"127.0.0.0", RESERVED},
               {"127.255.255.255", RESERVED},
               // Unreserved block(s)
               {"128.0.0.0", NOT_RESERVED},
               {"169.253.255.255", NOT_RESERVED},
               // 169.254.0.0/16
               {"169.254.0.0", RESERVED},
               {"169.254.255.255", RESERVED},
               // Unreserved block(s)
               {"169.255.0.0", NOT_RESERVED},
               {"172.15.255.255", NOT_RESERVED},
               // 172.16.0.0/12
               {"172.16.0.0", RESERVED},
               {"172.31.255.255", RESERVED},
               // Unreserved block(s)
               {"172.32.0.0", NOT_RESERVED},
               {"191.255.255.255", NOT_RESERVED},
               // 192.0.0.0/24 (including sub ranges)
               {"192.0.0.0", RESERVED},
               {"192.0.0.255", RESERVED},
               // Unreserved block(s)
               {"192.0.1.0", NOT_RESERVED},
               {"192.0.1.255", NOT_RESERVED},
               // 192.0.2.0/24
               {"192.0.2.0", RESERVED},
               {"192.0.2.255", RESERVED},
               // Unreserved block(s)
               {"192.0.3.0", NOT_RESERVED},
               {"192.31.195.255", NOT_RESERVED},
               // 192.31.196.0/24
               {"192.31.196.0", NOT_RESERVED},
               {"192.31.196.255", NOT_RESERVED},
               // Unreserved block(s)
               {"192.32.197.0", NOT_RESERVED},
               {"192.52.192.255", NOT_RESERVED},
               // 192.52.193.0/24
               {"192.52.193.0", NOT_RESERVED},
               {"192.52.193.255", NOT_RESERVED},
               // Unreserved block(s)
               {"192.52.194.0", NOT_RESERVED},
               {"192.88.98.255", NOT_RESERVED},
               // 192.88.99.0/24
               {"192.88.99.0", RESERVED},
               {"192.88.99.255", RESERVED},
               // Unreserved block(s)
               {"192.88.100.0", NOT_RESERVED},
               {"192.167.255.255", NOT_RESERVED},
               // 192.168.0.0/16
               {"192.168.0.0", RESERVED},
               {"192.168.255.255", RESERVED},
               // Unreserved block(s)
               {"192.169.0.0", NOT_RESERVED},
               {"192.175.47.255", NOT_RESERVED},
               // 192.175.48.0/24
               {"192.175.48.0", NOT_RESERVED},
               {"192.175.48.255", NOT_RESERVED},
               // Unreserved block(s)
               {"192.175.49.0", NOT_RESERVED},
               {"198.17.255.255", NOT_RESERVED},
               // 198.18.0.0/15
               {"198.18.0.0", RESERVED},
               {"198.19.255.255", RESERVED},
               // Unreserved block(s)
               {"198.20.0.0", NOT_RESERVED},
               {"198.51.99.255", NOT_RESERVED},
               // 198.51.100.0/24
               {"198.51.100.0", RESERVED},
               {"198.51.100.255", RESERVED},
               // Unreserved block(s)
               {"198.51.101.0", NOT_RESERVED},
               {"203.0.112.255", NOT_RESERVED},
               // 203.0.113.0/24
               {"203.0.113.0", RESERVED},
               {"203.0.113.255", RESERVED},
               // Unreserved block(s)
               {"203.0.114.0", NOT_RESERVED},
               {"223.255.255.255", NOT_RESERVED},
               // 224.0.0.0/8 - 255.0.0.0/8
               {"224.0.0.0", RESERVED},
               {"255.255.255.255", RESERVED}};

  for (const auto& test : tests) {
    IPAddress address;
    EXPECT_TRUE(address.AssignFromIPLiteral(test.address));
    ASSERT_TRUE(address.IsValid());
    EXPECT_EQ(!test.is_reserved, address.IsPubliclyRoutable());

    // Check these IPv4 addresses when mapped to IPv6. This verifies we're
    // properly unpacking mapped addresses.
    IPAddress mapped_address = ConvertIPv4ToIPv4MappedIPv6(address);
    EXPECT_EQ(!test.is_reserved, mapped_address.IsPubliclyRoutable());
  }
}

// Tests for the reserved IPv6 ranges and the (unreserved) blocks in between.
// The reserved ranges are tested by checking the first and last address of each
// range. The unreserved blocks are tested similarly. These tests cover the
// entire IPv6 address range.
TEST(IPAddressTest, IsPubliclyRoutableIPv6) {
  struct {
    const char* const address;
    IPAddressReservedResult is_reserved;
  } tests[] = {// 0000::/8.
               // Skip testing ::ffff:/96 explicitly since it was tested
               // in IsPubliclyRoutableIPv4
               {"0:0:0:0:0:0:0:0", RESERVED},
               {"ff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // 0100::/8
               {"100:0:0:0:0:0:0:0", RESERVED},
               {"1ff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // 0200::/7
               {"200:0:0:0:0:0:0:0", RESERVED},
               {"3ff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // 0400::/6
               {"400:0:0:0:0:0:0:0", RESERVED},
               {"7ff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // 0800::/5
               {"800:0:0:0:0:0:0:0", RESERVED},
               {"fff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // 1000::/4
               {"1000:0:0:0:0:0:0:0", RESERVED},
               {"1fff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // 2000::/3 (Global Unicast)
               {"2000:0:0:0:0:0:0:0", NOT_RESERVED},
               {"3fff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", NOT_RESERVED},
               // 4000::/3
               {"4000:0:0:0:0:0:0:0", RESERVED},
               {"5fff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // 6000::/3
               {"6000:0:0:0:0:0:0:0", RESERVED},
               {"7fff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // 8000::/3
               {"8000:0:0:0:0:0:0:0", RESERVED},
               {"9fff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // c000::/3
               {"c000:0:0:0:0:0:0:0", RESERVED},
               {"dfff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // e000::/4
               {"e000:0:0:0:0:0:0:0", RESERVED},
               {"efff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // f000::/5
               {"f000:0:0:0:0:0:0:0", RESERVED},
               {"f7ff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // f800::/6
               {"f800:0:0:0:0:0:0:0", RESERVED},
               {"fbff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // fc00::/7
               {"fc00:0:0:0:0:0:0:0", RESERVED},
               {"fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // fe00::/9
               {"fe00:0:0:0:0:0:0:0", RESERVED},
               {"fe7f:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // fe80::/10
               {"fe80:0:0:0:0:0:0:0", RESERVED},
               {"febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // fec0::/10
               {"fec0:0:0:0:0:0:0:0", RESERVED},
               {"feff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", RESERVED},
               // ff00::/8 (Multicast)
               {"ff00:0:0:0:0:0:0:0", NOT_RESERVED},
               {"ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", NOT_RESERVED}};

  IPAddress address;
  for (const auto& test : tests) {
    EXPECT_TRUE(address.AssignFromIPLiteral(test.address));
    EXPECT_EQ(!test.is_reserved, address.IsPubliclyRoutable());
  }
}

TEST(IPAddressTest, IsZero) {
  uint8_t address1[4] = {};
  IPAddress zero_ipv4_address(address1);
  EXPECT_TRUE(zero_ipv4_address.IsZero());

  uint8_t address2[4] = {10};
  IPAddress non_zero_ipv4_address(address2);
  EXPECT_FALSE(non_zero_ipv4_address.IsZero());

  uint8_t address3[16] = {};
  IPAddress zero_ipv6_address(address3);
  EXPECT_TRUE(zero_ipv6_address.IsZero());

  uint8_t address4[16] = {10};
  IPAddress non_zero_ipv6_address(address4);
  EXPECT_FALSE(non_zero_ipv6_address.IsZero());

  IPAddress empty_address;
  EXPECT_FALSE(empty_address.IsZero());
}

TEST(IPAddressTest, IsIPv4Mapped) {
  IPAddress ipv4_address(192, 168, 0, 1);
  EXPECT_FALSE(ipv4_address.IsIPv4MappedIPv6());
  IPAddress ipv6_address(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1);
  EXPECT_FALSE(ipv6_address.IsIPv4MappedIPv6());
  IPAddress mapped_address(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 1, 1, 0, 1);
  EXPECT_TRUE(mapped_address.IsIPv4MappedIPv6());
}

TEST(IPAddressTest, AllZeros) {
  EXPECT_TRUE(IPAddress::AllZeros(0).empty());

  EXPECT_EQ(3u, IPAddress::AllZeros(3).size());
  EXPECT_TRUE(IPAddress::AllZeros(3).IsZero());

  EXPECT_EQ("0.0.0.0", IPAddress::IPv4AllZeros().ToString());
  EXPECT_EQ("::", IPAddress::IPv6AllZeros().ToString());
}

TEST(IPAddressTest, ToString) {
  EXPECT_EQ("0.0.0.0", IPAddress::IPv4AllZeros().ToString());

  IPAddress address(192, 168, 0, 1);
  EXPECT_EQ("192.168.0.1", address.ToString());

  IPAddress address2(0xFE, 0xDC, 0xBA, 0x98, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                     0);
  EXPECT_EQ("fedc:ba98::", address2.ToString());

  // ToString() shouldn't crash on invalid addresses.
  uint8_t addr4[2];
  IPAddress address4(addr4);
  EXPECT_EQ("", address4.ToString());

  IPAddress address5;
  EXPECT_EQ("", address5.ToString());
}

TEST(IPAddressTest, IPAddressToStringWithPort) {
  EXPECT_EQ("0.0.0.0:3",
            IPAddressToStringWithPort(IPAddress::IPv4AllZeros(), 3));

  IPAddress address1(192, 168, 0, 1);
  EXPECT_EQ("192.168.0.1:99", IPAddressToStringWithPort(address1, 99));

  IPAddress address2(0xFE, 0xDC, 0xBA, 0x98, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                     0);
  EXPECT_EQ("[fedc:ba98::]:8080", IPAddressToStringWithPort(address2, 8080));

  // IPAddressToStringWithPort() shouldn't crash on invalid addresses.
  uint8_t addr3[2];
  EXPECT_EQ("", IPAddressToStringWithPort(IPAddress(addr3), 8080));
}

TEST(IPAddressTest, IPAddressToPackedString) {
  IPAddress ipv4_address;
  EXPECT_TRUE(ipv4_address.AssignFromIPLiteral("4.31.198.44"));
  std::string expected_ipv4_address("\x04\x1f\xc6\x2c", 4);
  EXPECT_EQ(expected_ipv4_address, IPAddressToPackedString(ipv4_address));

  IPAddress ipv6_address;
  EXPECT_TRUE(ipv6_address.AssignFromIPLiteral("2001:0700:0300:1800::000f"));
  std::string expected_ipv6_address(
      "\x20\x01\x07\x00\x03\x00\x18\x00"
      "\x00\x00\x00\x00\x00\x00\x00\x0f",
      16);
  EXPECT_EQ(expected_ipv6_address, IPAddressToPackedString(ipv6_address));
}

// Test that invalid IP literals fail to parse.
TEST(IPAddressTest, AssignFromIPLiteral_FailParse) {
  IPAddress address;

  EXPECT_FALSE(address.AssignFromIPLiteral("bad value"));
  EXPECT_FALSE(address.AssignFromIPLiteral("bad:value"));
  EXPECT_FALSE(address.AssignFromIPLiteral(std::string()));
  EXPECT_FALSE(address.AssignFromIPLiteral("192.168.0.1:30"));
  EXPECT_FALSE(address.AssignFromIPLiteral("  192.168.0.1  "));
  EXPECT_FALSE(address.AssignFromIPLiteral("[::1]"));
}

// Test that a failure calling AssignFromIPLiteral() has the sideffect of
// clearing the current value.
TEST(IPAddressTest, AssignFromIPLiteral_ResetOnFailure) {
  IPAddress address = IPAddress::IPv6Localhost();

  EXPECT_TRUE(address.IsValid());
  EXPECT_FALSE(address.empty());

  EXPECT_FALSE(address.AssignFromIPLiteral("bad value"));

  EXPECT_FALSE(address.IsValid());
  EXPECT_TRUE(address.empty());
}

// Test parsing an IPv4 literal.
TEST(IPAddressTest, AssignFromIPLiteral_IPv4) {
  IPAddress address;
  EXPECT_TRUE(address.AssignFromIPLiteral("192.168.0.1"));
  EXPECT_EQ("192,168,0,1", DumpIPAddress(address));
  EXPECT_EQ("192.168.0.1", address.ToString());
}

// Test parsing an IPv6 literal.
TEST(IPAddressTest, AssignFromIPLiteral_IPv6) {
  IPAddress address;
  EXPECT_TRUE(address.AssignFromIPLiteral("1:abcd::3:4:ff"));
  EXPECT_EQ("0,1,171,205,0,0,0,0,0,0,0,3,0,4,0,255", DumpIPAddress(address));
  EXPECT_EQ("1:abcd::3:4:ff", address.ToString());
}

TEST(IPAddressTest, IsIPv4MappedIPv6) {
  IPAddress ipv4_address(192, 168, 0, 1);
  EXPECT_FALSE(ipv4_address.IsIPv4MappedIPv6());
  IPAddress ipv6_address = IPAddress::IPv6Localhost();
  EXPECT_FALSE(ipv6_address.IsIPv4MappedIPv6());
  IPAddress mapped_address(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 1, 1, 0, 1);
  EXPECT_TRUE(mapped_address.IsIPv4MappedIPv6());
}

TEST(IPAddressTest, IsEqual) {
  IPAddress ip_address1;
  EXPECT_TRUE(ip_address1.AssignFromIPLiteral("127.0.0.1"));
  IPAddress ip_address2;
  EXPECT_TRUE(ip_address2.AssignFromIPLiteral("2001:db8:0::42"));
  IPAddress ip_address3;
  EXPECT_TRUE(ip_address3.AssignFromIPLiteral("127.0.0.1"));

  EXPECT_FALSE(ip_address1 == ip_address2);
  EXPECT_TRUE(ip_address1 == ip_address3);
}

TEST(IPAddressTest, LessThan) {
  // IPv4 vs IPv6
  IPAddress ip_address1;
  EXPECT_TRUE(ip_address1.AssignFromIPLiteral("127.0.0.1"));
  IPAddress ip_address2;
  EXPECT_TRUE(ip_address2.AssignFromIPLiteral("2001:db8:0::42"));
  EXPECT_TRUE(ip_address1 < ip_address2);
  EXPECT_FALSE(ip_address2 < ip_address1);

  // Compare equivalent addresses.
  IPAddress ip_address3;
  EXPECT_TRUE(ip_address3.AssignFromIPLiteral("127.0.0.1"));
  EXPECT_FALSE(ip_address1 < ip_address3);
  EXPECT_FALSE(ip_address3 < ip_address1);

  IPAddress ip_address4;
  EXPECT_TRUE(ip_address4.AssignFromIPLiteral("128.0.0.0"));
  EXPECT_TRUE(ip_address1 < ip_address4);
  EXPECT_FALSE(ip_address4 < ip_address1);
}

// Test mapping an IPv4 address to an IPv6 address.
TEST(IPAddressTest, ConvertIPv4ToIPv4MappedIPv6) {
  IPAddress ipv4_address(192, 168, 0, 1);
  IPAddress ipv6_address = ConvertIPv4ToIPv4MappedIPv6(ipv4_address);

  // ::ffff:192.168.0.1
  EXPECT_EQ("0,0,0,0,0,0,0,0,0,0,255,255,192,168,0,1",
            DumpIPAddress(ipv6_address));
  EXPECT_EQ("::ffff:c0a8:1", ipv6_address.ToString());
}

// Test reversal of a IPv6 address mapping.
TEST(IPAddressTest, ConvertIPv4MappedIPv6ToIPv4) {
  IPAddress ipv4mapped_address;
  EXPECT_TRUE(ipv4mapped_address.AssignFromIPLiteral("::ffff:c0a8:1"));

  IPAddress expected(192, 168, 0, 1);

  IPAddress result = ConvertIPv4MappedIPv6ToIPv4(ipv4mapped_address);
  EXPECT_EQ(expected, result);
}

TEST(IPAddressTest, IPAddressMatchesPrefix) {
  struct {
    const char* const cidr_literal;
    size_t prefix_length_in_bits;
    const char* const ip_literal;
    bool expected_to_match;
  } tests[] = {
      // IPv4 prefix with IPv4 inputs.
      {"10.10.1.32", 27, "10.10.1.44", true},
      {"10.10.1.32", 27, "10.10.1.90", false},
      {"10.10.1.32", 27, "10.10.1.90", false},

      // IPv6 prefix with IPv6 inputs.
      {"2001:db8::", 32, "2001:DB8:3:4::5", true},
      {"2001:db8::", 32, "2001:c8::", false},

      // IPv6 prefix with IPv4 inputs.
      {"2001:db8::", 33, "192.168.0.1", false},
      {"::ffff:192.168.0.1", 112, "192.168.33.77", true},

      // IPv4 prefix with IPv6 inputs.
      {"10.11.33.44", 16, "::ffff:0a0b:89", true},
      {"10.11.33.44", 16, "::ffff:10.12.33.44", false},
  };
  for (const auto& test : tests) {
    SCOPED_TRACE(
        base::StringPrintf("%s, %s", test.cidr_literal, test.ip_literal));

    IPAddress ip_address;
    EXPECT_TRUE(ip_address.AssignFromIPLiteral(test.ip_literal));

    IPAddress ip_prefix;
    EXPECT_TRUE(ip_prefix.AssignFromIPLiteral(test.cidr_literal));

    EXPECT_EQ(test.expected_to_match,
              IPAddressMatchesPrefix(ip_address, ip_prefix,
                                     test.prefix_length_in_bits));
  }
}

// Test parsing invalid CIDR notation literals.
TEST(IPAddressTest, ParseCIDRBlock_Invalid) {
  const char* const bad_literals[] = {"foobar",
                                      "",
                                      "192.168.0.1",
                                      "::1",
                                      "/",
                                      "/1",
                                      "1",
                                      "192.168.1.1/-1",
                                      "192.168.1.1/33",
                                      "::1/-3",
                                      "a::3/129",
                                      "::1/x",
                                      "192.168.0.1//11",
                                      "192.168.1.1/+1",
                                      "192.168.1.1/ +1",
                                      "192.168.1.1/"};

  for (auto* bad_literal : bad_literals) {
    IPAddress ip_address;
    size_t prefix_length_in_bits;

    EXPECT_FALSE(
        ParseCIDRBlock(bad_literal, &ip_address, &prefix_length_in_bits));
  }
}

// Test parsing a valid CIDR notation literal.
TEST(IPAddressTest, ParseCIDRBlock_Valid) {
  IPAddress ip_address;
  size_t prefix_length_in_bits;

  EXPECT_TRUE(
      ParseCIDRBlock("192.168.0.1/11", &ip_address, &prefix_length_in_bits));

  EXPECT_EQ("192,168,0,1", DumpIPAddress(ip_address));
  EXPECT_EQ(11u, prefix_length_in_bits);

  EXPECT_TRUE(ParseCIDRBlock("::ffff:192.168.0.1/112", &ip_address,
                             &prefix_length_in_bits));

  EXPECT_EQ("0,0,0,0,0,0,0,0,0,0,255,255,192,168,0,1",
            DumpIPAddress(ip_address));
  EXPECT_EQ(112u, prefix_length_in_bits);
}

TEST(IPAddressTest, ParseURLHostnameToAddress_FailParse) {
  IPAddress address;
  EXPECT_FALSE(ParseURLHostnameToAddress("bad value", &address));
  EXPECT_FALSE(ParseURLHostnameToAddress("bad:value", &address));
  EXPECT_FALSE(ParseURLHostnameToAddress(std::string(), &address));
  EXPECT_FALSE(ParseURLHostnameToAddress("192.168.0.1:30", &address));
  EXPECT_FALSE(ParseURLHostnameToAddress("  192.168.0.1  ", &address));
  EXPECT_FALSE(ParseURLHostnameToAddress("::1", &address));
  EXPECT_FALSE(ParseURLHostnameToAddress("[192.169.0.1]", &address));
}

TEST(IPAddressTest, ParseURLHostnameToAddress_IPv4) {
  IPAddress address;
  EXPECT_TRUE(ParseURLHostnameToAddress("192.168.0.1", &address));
  EXPECT_EQ("192,168,0,1", DumpIPAddress(address));
  EXPECT_EQ("192.168.0.1", address.ToString());
}

TEST(IPAddressTest, ParseURLHostnameToAddress_IPv6) {
  IPAddress address;
  EXPECT_TRUE(ParseURLHostnameToAddress("[1:abcd::3:4:ff]", &address));
  EXPECT_EQ("0,1,171,205,0,0,0,0,0,0,0,3,0,4,0,255", DumpIPAddress(address));
  EXPECT_EQ("1:abcd::3:4:ff", address.ToString());
}

TEST(IPAddressTest, IPAddressStartsWith) {
  IPAddress ipv4_address(192, 168, 10, 5);

  uint8_t ipv4_prefix1[] = {192, 168, 10};
  EXPECT_TRUE(IPAddressStartsWith(ipv4_address, ipv4_prefix1));

  uint8_t ipv4_prefix3[] = {192, 168, 10, 5};
  EXPECT_TRUE(IPAddressStartsWith(ipv4_address, ipv4_prefix3));

  uint8_t ipv4_prefix2[] = {192, 168, 10, 10};
  EXPECT_FALSE(IPAddressStartsWith(ipv4_address, ipv4_prefix2));

  // Prefix is longer than the address.
  uint8_t ipv4_prefix4[] = {192, 168, 10, 10, 0};
  EXPECT_FALSE(IPAddressStartsWith(ipv4_address, ipv4_prefix4));

  IPAddress ipv6_address;
  EXPECT_TRUE(ipv6_address.AssignFromIPLiteral("2a00:1450:400c:c09::64"));

  uint8_t ipv6_prefix1[] = {42, 0, 20, 80, 64, 12, 12, 9};
  EXPECT_TRUE(IPAddressStartsWith(ipv6_address, ipv6_prefix1));

  uint8_t ipv6_prefix2[] = {41, 0, 20, 80, 64, 12, 12, 9,
                            0,  0, 0,  0,  0,  0,  100};
  EXPECT_FALSE(IPAddressStartsWith(ipv6_address, ipv6_prefix2));

  uint8_t ipv6_prefix3[] = {42, 0, 20, 80, 64, 12, 12, 9,
                            0,  0, 0,  0,  0,  0,  0,  100};
  EXPECT_TRUE(IPAddressStartsWith(ipv6_address, ipv6_prefix3));

  uint8_t ipv6_prefix4[] = {42, 0, 20, 80, 64, 12, 12, 9,
                            0,  0, 0,  0,  0,  0,  0,  0};
  EXPECT_FALSE(IPAddressStartsWith(ipv6_address, ipv6_prefix4));

  // Prefix is longer than the address.
  uint8_t ipv6_prefix5[] = {42, 0, 20, 80, 64, 12, 12, 9, 0,
                            0,  0, 0,  0,  0,  0,  0,  10};
  EXPECT_FALSE(IPAddressStartsWith(ipv6_address, ipv6_prefix5));
}

TEST(IPAddressTest, IsLinkLocal) {
  const char* kPositive[] = {
      "169.254.0.0",
      "169.254.100.1",
      "169.254.100.1",
      "::ffff:169.254.0.0",
      "::ffff:169.254.100.1",
      "fe80::1",
      "fe81::1",
  };

  for (const char* literal : kPositive) {
    IPAddress ip_address;
    ASSERT_TRUE(ip_address.AssignFromIPLiteral(literal));
    EXPECT_TRUE(ip_address.IsLinkLocal()) << literal;
  }

  const char* kNegative[] = {
      "170.254.0.0",        "169.255.0.0",        "::169.254.0.0",
      "::fffe:169.254.0.0", "::ffff:169.255.0.0", "fec0::1",
  };

  for (const char* literal : kNegative) {
    IPAddress ip_address;
    ASSERT_TRUE(ip_address.AssignFromIPLiteral(literal));
    EXPECT_FALSE(ip_address.IsLinkLocal()) << literal;
  }
}

TEST(IPAddressTest, IsUniqueLocalIPv6) {
  const char* kPositive[] = {
      "fc00::1",
      "fc80::1",
      "fd00::1",
  };

  for (const char* literal : kPositive) {
    IPAddress ip_address;
    ASSERT_TRUE(ip_address.AssignFromIPLiteral(literal));
    EXPECT_TRUE(ip_address.IsUniqueLocalIPv6()) << literal;
  }

  const char* kNegative[] = {
      "fe00::1",
      "ff00::1",
      "252.0.0.1",
  };

  for (const char* literal : kNegative) {
    IPAddress ip_address;
    ASSERT_TRUE(ip_address.AssignFromIPLiteral(literal));
    EXPECT_FALSE(ip_address.IsUniqueLocalIPv6()) << literal;
  }
}

// Tests extraction of the NAT64 translation prefix.
TEST(IPAddressTest, ExtractPref64FromIpv4onlyArpaAAAA) {
  // Well Known Prefix 64:ff9b::/96.
  IPAddress ipv6_address_WKP_0(0, 100, 255, 155, 0, 0, 0, 0, 0, 0, 0, 0, 192, 0,
                               0, 170);
  IPAddress ipv6_address_WKP_1(0, 100, 255, 155, 0, 0, 0, 0, 0, 0, 0, 0, 192, 0,
                               0, 171);
  Dns64PrefixLength pref64_length_WKP_0 =
      ExtractPref64FromIpv4onlyArpaAAAA(ipv6_address_WKP_0);
  Dns64PrefixLength pref64_length_WKP_1 =
      ExtractPref64FromIpv4onlyArpaAAAA(ipv6_address_WKP_1);
  EXPECT_EQ(Dns64PrefixLength::k96bit, pref64_length_WKP_0);
  EXPECT_EQ(Dns64PrefixLength::k96bit, pref64_length_WKP_1);

  // Prefix length 96
  IPAddress ipv6_address_96_0(32, 1, 13, 184, 1, 34, 3, 68, 0, 0, 0, 0, 192, 0,
                              0, 170);
  IPAddress ipv6_address_96_1(32, 1, 13, 184, 1, 34, 3, 68, 0, 0, 0, 0, 192, 0,
                              0, 171);
  Dns64PrefixLength pref64_length_96_0 =
      ExtractPref64FromIpv4onlyArpaAAAA(ipv6_address_96_0);
  Dns64PrefixLength pref64_length_96_1 =
      ExtractPref64FromIpv4onlyArpaAAAA(ipv6_address_96_1);
  EXPECT_EQ(Dns64PrefixLength::k96bit, pref64_length_96_0);
  EXPECT_EQ(Dns64PrefixLength::k96bit, pref64_length_96_1);

  // Prefix length 64
  IPAddress ipv6_address_64_0(32, 1, 13, 184, 1, 34, 3, 68, 0, 192, 0, 0, 170,
                              0, 0, 0);
  IPAddress ipv6_address_64_1(32, 1, 13, 184, 1, 34, 3, 68, 0, 192, 0, 0, 171,
                              0, 0, 0);
  Dns64PrefixLength pref64_length_64_0 =
      ExtractPref64FromIpv4onlyArpaAAAA(ipv6_address_64_0);
  Dns64PrefixLength pref64_length_64_1 =
      ExtractPref64FromIpv4onlyArpaAAAA(ipv6_address_64_1);
  EXPECT_EQ(Dns64PrefixLength::k64bit, pref64_length_64_0);
  EXPECT_EQ(Dns64PrefixLength::k64bit, pref64_length_64_1);

  // Prefix length 56
  IPAddress ipv6_address_56_0(32, 1, 13, 184, 1, 34, 3, 192, 0, 0, 0, 170, 0, 0,
                              0, 0);
  IPAddress ipv6_address_56_1(32, 1, 13, 184, 1, 34, 3, 192, 0, 0, 0, 171, 0, 0,
                              0, 0);
  Dns64PrefixLength pref64_length_56_0 =
      ExtractPref64FromIpv4onlyArpaAAAA(ipv6_address_56_0);
  Dns64PrefixLength pref64_length_56_1 =
      ExtractPref64FromIpv4onlyArpaAAAA(ipv6_address_56_1);
  EXPECT_EQ(Dns64PrefixLength::k56bit, pref64_length_56_0);
  EXPECT_EQ(Dns64PrefixLength::k56bit, pref64_length_56_1);

  // Prefix length 48
  IPAddress ipv6_address_48_0(32, 1, 13, 184, 1, 34, 192, 0, 0, 0, 170, 0, 0, 0,
                              0, 0);
  IPAddress ipv6_address_48_1(32, 1, 13, 184, 1, 34, 192, 0, 0, 0, 171, 0, 0, 0,
                              0, 0);
  Dns64PrefixLength pref64_length_48_0 =
      ExtractPref64FromIpv4onlyArpaAAAA(ipv6_address_48_0);
  Dns64PrefixLength pref64_length_48_1 =
      ExtractPref64FromIpv4onlyArpaAAAA(ipv6_address_48_1);
  EXPECT_EQ(Dns64PrefixLength::k48bit, pref64_length_48_0);
  EXPECT_EQ(Dns64PrefixLength::k48bit, pref64_length_48_1);

  // Prefix length 40
  IPAddress ipv6_address_40_0(32, 1, 13, 184, 1, 192, 0, 0, 0, 170, 0, 0, 0, 0,
                              0, 0);
  IPAddress ipv6_address_40_1(32, 1, 13, 184, 1, 192, 0, 0, 0, 171, 0, 0, 0, 0,
                              0, 0);
  Dns64PrefixLength pref64_length_40_0 =
      ExtractPref64FromIpv4onlyArpaAAAA(ipv6_address_40_0);
  Dns64PrefixLength pref64_length_40_1 =
      ExtractPref64FromIpv4onlyArpaAAAA(ipv6_address_40_1);
  EXPECT_EQ(Dns64PrefixLength::k40bit, pref64_length_40_0);
  EXPECT_EQ(Dns64PrefixLength::k40bit, pref64_length_40_1);

  // Prefix length 32
  IPAddress ipv6_address_32_0(32, 1, 13, 184, 192, 0, 0, 170, 0, 0, 0, 0, 0, 0,
                              0, 0);
  IPAddress ipv6_address_32_1(32, 1, 13, 184, 192, 0, 0, 171, 0, 0, 0, 0, 0, 0,
                              0, 0);
  Dns64PrefixLength pref64_length_32_0 =
      ExtractPref64FromIpv4onlyArpaAAAA(ipv6_address_32_0);
  Dns64PrefixLength pref64_length_32_1 =
      ExtractPref64FromIpv4onlyArpaAAAA(ipv6_address_32_1);
  EXPECT_EQ(Dns64PrefixLength::k32bit, pref64_length_32_0);
  EXPECT_EQ(Dns64PrefixLength::k32bit, pref64_length_32_1);
}

// Tests mapping an IPv4 address to an IPv6 address.
TEST(IPAddressTest, ConvertIPv4ToIPv4EmbeddedIPv6) {
  IPAddress ipv4_address(192, 0, 2, 33);

  // Well Known Prefix 64:ff9b::/96.
  IPAddress ipv6_address_WKP(0, 100, 255, 155, 0, 0, 0, 0, 0, 0, 0, 0, 192, 0,
                             0, 170);
  IPAddress converted_ipv6_address_WKP = ConvertIPv4ToIPv4EmbeddedIPv6(
      ipv4_address, ipv6_address_WKP, Dns64PrefixLength::k96bit);
  EXPECT_EQ("0,100,255,155,0,0,0,0,0,0,0,0,192,0,2,33",
            DumpIPAddress(converted_ipv6_address_WKP));
  EXPECT_EQ("64:ff9b::c000:221", converted_ipv6_address_WKP.ToString());

  // Prefix length 96
  IPAddress ipv6_address_96(32, 1, 13, 184, 1, 34, 3, 68, 0, 0, 0, 0, 0, 0, 0,
                            0);
  IPAddress converted_ipv6_address_96 = ConvertIPv4ToIPv4EmbeddedIPv6(
      ipv4_address, ipv6_address_96, Dns64PrefixLength::k96bit);
  EXPECT_EQ("32,1,13,184,1,34,3,68,0,0,0,0,192,0,2,33",
            DumpIPAddress(converted_ipv6_address_96));
  EXPECT_EQ("2001:db8:122:344::c000:221", converted_ipv6_address_96.ToString());

  // Prefix length 64
  IPAddress ipv6_address_64(32, 1, 13, 184, 1, 34, 3, 68, 0, 0, 0, 0, 0, 0, 0,
                            0);
  IPAddress converted_ipv6_address_64 = ConvertIPv4ToIPv4EmbeddedIPv6(
      ipv4_address, ipv6_address_64, Dns64PrefixLength::k64bit);
  EXPECT_EQ("32,1,13,184,1,34,3,68,0,192,0,2,33,0,0,0",
            DumpIPAddress(converted_ipv6_address_64));
  EXPECT_EQ("2001:db8:122:344:c0:2:2100:0",
            converted_ipv6_address_64.ToString());

  // Prefix length 56
  IPAddress ipv6_address_56(32, 1, 13, 184, 1, 34, 3, 0, 0, 0, 0, 0, 0, 0, 0,
                            0);
  IPAddress converted_ipv6_address_56 = ConvertIPv4ToIPv4EmbeddedIPv6(
      ipv4_address, ipv6_address_56, Dns64PrefixLength::k56bit);
  EXPECT_EQ("32,1,13,184,1,34,3,192,0,0,2,33,0,0,0,0",
            DumpIPAddress(converted_ipv6_address_56));
  EXPECT_EQ("2001:db8:122:3c0:0:221::", converted_ipv6_address_56.ToString());

  // Prefix length 48
  IPAddress ipv6_address_48(32, 1, 13, 184, 1, 34, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                            0);
  IPAddress converted_ipv6_address_48 = ConvertIPv4ToIPv4EmbeddedIPv6(
      ipv4_address, ipv6_address_48, Dns64PrefixLength::k48bit);
  EXPECT_EQ("32,1,13,184,1,34,192,0,0,2,33,0,0,0,0,0",
            DumpIPAddress(converted_ipv6_address_48));
  EXPECT_EQ("2001:db8:122:c000:2:2100::", converted_ipv6_address_48.ToString());

  // Prefix length 40
  IPAddress ipv6_address_40(32, 1, 13, 184, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  IPAddress converted_ipv6_address_40 = ConvertIPv4ToIPv4EmbeddedIPv6(
      ipv4_address, ipv6_address_40, Dns64PrefixLength::k40bit);
  EXPECT_EQ("32,1,13,184,1,192,0,2,0,33,0,0,0,0,0,0",
            DumpIPAddress(converted_ipv6_address_40));
  EXPECT_EQ("2001:db8:1c0:2:21::", converted_ipv6_address_40.ToString());

  // Prefix length 32
  IPAddress ipv6_address_32(32, 1, 13, 184, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
  IPAddress converted_ipv6_address_32 = ConvertIPv4ToIPv4EmbeddedIPv6(
      ipv4_address, ipv6_address_32, Dns64PrefixLength::k32bit);
  EXPECT_EQ("32,1,13,184,192,0,2,33,0,0,0,0,0,0,0,0",
            DumpIPAddress(converted_ipv6_address_32));
  EXPECT_EQ("2001:db8:c000:221::", converted_ipv6_address_32.ToString());
}

TEST(IPAddressTest, RoundtripAddressThroughValue) {
  IPAddress address(1, 2, 3, 4);
  ASSERT_TRUE(address.IsValid());

  base::Value value = address.ToValue();
  EXPECT_THAT(IPAddress::FromValue(value), Optional(address));
}

TEST(IPAddressTest, FromGarbageValue) {
  base::Value value(123);
  EXPECT_FALSE(IPAddress::FromValue(value).has_value());
}

TEST(IPAddressTest, FromInvalidValue) {
  base::Value value("1.2.3.4.5");
  EXPECT_FALSE(IPAddress::FromValue(value).has_value());
}

TEST(IPAddressTest, IPv4Mask) {
  IPAddress mask;
  EXPECT_FALSE(
      IPAddress::CreateIPv4Mask(&mask, IPAddress::kIPv6AddressSize * 8));
  EXPECT_FALSE(
      IPAddress::CreateIPv4Mask(&mask, (IPAddress::kIPv4AddressSize + 1) * 8));
  EXPECT_FALSE(
      IPAddress::CreateIPv4Mask(&mask, IPAddress::kIPv4AddressSize * 8 + 1));
  EXPECT_TRUE(
      IPAddress::CreateIPv4Mask(&mask, IPAddress::kIPv4AddressSize * 8));
  EXPECT_EQ("255.255.255.255", mask.ToString());
  EXPECT_TRUE(IPAddress::CreateIPv4Mask(&mask, 31));
  EXPECT_EQ("255.255.255.254", mask.ToString());
  EXPECT_TRUE(IPAddress::CreateIPv4Mask(&mask, 24));
  EXPECT_EQ("255.255.255.0", mask.ToString());
  EXPECT_TRUE(IPAddress::CreateIPv4Mask(&mask, 23));
  EXPECT_EQ("255.255.254.0", mask.ToString());
  EXPECT_TRUE(IPAddress::CreateIPv4Mask(&mask, 18));
  EXPECT_EQ("255.255.192.0", mask.ToString());
  EXPECT_TRUE(IPAddress::CreateIPv4Mask(&mask, 16));
  EXPECT_EQ("255.255.0.0", mask.ToString());
  EXPECT_TRUE(IPAddress::CreateIPv4Mask(&mask, 8));
  EXPECT_EQ("255.0.0.0", mask.ToString());
  EXPECT_TRUE(IPAddress::CreateIPv4Mask(&mask, 1));
  EXPECT_EQ("128.0.0.0", mask.ToString());
  EXPECT_TRUE(IPAddress::CreateIPv4Mask(&mask, 0));
  EXPECT_EQ("0.0.0.0", mask.ToString());
}

TEST(IPAddressTest, IPv6Mask) {
  IPAddress mask;
  EXPECT_FALSE(
      IPAddress::CreateIPv6Mask(&mask, (IPAddress::kIPv6AddressSize * 8) + 1));
  EXPECT_TRUE(
      IPAddress::CreateIPv6Mask(&mask, IPAddress::kIPv6AddressSize * 8));
  EXPECT_EQ("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff", mask.ToString());
  EXPECT_TRUE(IPAddress::CreateIPv6Mask(&mask, 112));
  EXPECT_EQ("ffff:ffff:ffff:ffff:ffff:ffff:ffff:0", mask.ToString());
  EXPECT_TRUE(IPAddress::CreateIPv6Mask(&mask, 32));
  EXPECT_EQ("ffff:ffff::", mask.ToString());
  EXPECT_TRUE(IPAddress::CreateIPv6Mask(&mask, 1));
  EXPECT_EQ("8000::", mask.ToString());
  EXPECT_TRUE(IPAddress::CreateIPv6Mask(&mask, 0));
  EXPECT_EQ("::", mask.ToString());
}

}  // anonymous namespace

}  // namespace net

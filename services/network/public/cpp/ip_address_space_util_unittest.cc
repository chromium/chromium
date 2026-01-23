// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/ip_address_space_util.h"

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/transport_info.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {
namespace {

using mojom::ContentSecurityPolicy;
using mojom::IPAddressSpace;
using mojom::ParsedHeaders;
using mojom::URLResponseHead;
using net::IPAddress;
using net::IPAddressBytes;
using net::IPEndPoint;
using net::TransportInfo;
using net::TransportType;

IPAddress PublicIPv4Address() {
  return IPAddress(64, 233, 160, 0);
}

IPAddress PrivateIPv4Address() {
  return IPAddress(192, 168, 1, 1);
}

TransportInfo DirectTransport(const IPEndPoint& endpoint) {
  TransportInfo result;
  result.type = TransportType::kDirect;
  result.endpoint = endpoint;
  return result;
}

TransportInfo ProxiedTransport(const IPEndPoint& endpoint) {
  TransportInfo result;
  result.type = TransportType::kProxied;
  result.endpoint = endpoint;
  return result;
}

TransportInfo MakeTransport(TransportType type, const IPEndPoint& endpoint) {
  TransportInfo result;
  result.type = type;
  result.endpoint = endpoint;
  return result;
}

IPAddressSpace IPEndPointToIPAddressSpace(const IPEndPoint& endpoint) {
  return TransportInfoToIPAddressSpace(DirectTransport(endpoint));
}

// Helper for tests that do not care about overrides.
IPAddressSpace IPAddressToIPAddressSpace(const IPAddress& address) {
  return IPEndPointToIPAddressSpace(IPEndPoint(address, 80));
}

// Verifies that the address space of an invalid IP address is `unknown`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceInvalid) {
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress()), IPAddressSpace::kUnknown);
}

// Verifies that the address space of a regular IP address is `public`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV4Public) {
  EXPECT_EQ(IPAddressToIPAddressSpace(PublicIPv4Address()),
            IPAddressSpace::kPublic);
}

// Verifies that the address space of IP addresses belonging to any of the
// three "Private Use" address blocks defined in RFC 1918 is `local`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV4PrivateUse) {
  EXPECT_EQ(IPAddressToIPAddressSpace(PrivateIPv4Address()),
            IPAddressSpace::kLocal);

  // 10.0.0.0/8

  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(9, 255, 255, 255)),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(10, 0, 0, 0)),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(10, 255, 255, 255)),
            IPAddressSpace::kLocal);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(11, 0, 0, 0)),
            IPAddressSpace::kPublic);

  // 172.16.0.0/12

  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(172, 15, 255, 255)),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(172, 16, 0, 0)),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(172, 31, 255, 255)),
            IPAddressSpace::kLocal);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(172, 32, 0, 0)),
            IPAddressSpace::kPublic);

  // 192.168.0.0/16

  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(192, 167, 255, 255)),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(192, 168, 0, 0)),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(192, 168, 255, 255)),
            IPAddressSpace::kLocal);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(169, 169, 0, 0)),
            IPAddressSpace::kPublic);
}

// Verifies that the address space of IP addresses belonging to the "Link-local"
// 169.254.0.0/16 block are `local`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV4LinkLocal) {
  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(169, 253, 255, 255)),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(169, 254, 0, 0)),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(169, 254, 255, 255)),
            IPAddressSpace::kLocal);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(169, 255, 0, 0)),
            IPAddressSpace::kPublic);
}

// Verifies that the address space of IPv4 localhost and the rest of the
// 127.0.0.0/8 block is `loopback`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV4Localhost) {
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress::IPv4Localhost()),
            IPAddressSpace::kLoopback);

  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(126, 255, 255, 255)),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 0, 0, 0)),
            IPAddressSpace::kLoopback);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 255, 255, 255)),
            IPAddressSpace::kLoopback);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(128, 0, 0, 0)),
            IPAddressSpace::kPublic);
}

IPAddress ParseIPAddress(std::string_view str) {
  IPAddress address;
  EXPECT_TRUE(address.AssignFromIPLiteral(str))
      << "Failed to parse IP address: " << str;
  return address;
}

// Verifies that the address space of a regular IPv6 address is `public`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV6Public) {
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("42::")),
            IPAddressSpace::kPublic);
}

// Verifies that the address space of IPv6 addresses in the "Unique-local"
// (fc00::/7) address block is `local`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV6UniqueLocal) {
  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(
                ParseIPAddress("fbff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("fc00::")),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(
                ParseIPAddress("fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")),
            IPAddressSpace::kLocal);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("fe00::")),
            IPAddressSpace::kPublic);
}

// Verifies that the address space of IPv6 addresses in the "Link-local unicast"
// (fe80::/10) address block is `local`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV6LinkLocalUnicast) {
  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(
                ParseIPAddress("fe7f:ffff:ffff:ffff:ffff:ffff:ffff:ffff")),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("fe80::")),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(
                ParseIPAddress("febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff")),
            IPAddressSpace::kLocal);

  // Upper bound (exclusive).
  // fec0:: is Site Local which is mapped to kLocal (see test below).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("fec0::")),
            IPAddressSpace::kLocal);
}

// Verifies that the address space of IPv6 localhost (::1/128) is `loopback`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV6Localhost) {
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress::IPv6Localhost()),
            IPAddressSpace::kLoopback);

  // Lower bound (exclusive).
  // ::0 is the unspecified address which is mapped to kLoopback (see test
  // below).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("::0")),
            IPAddressSpace::kLoopback);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("::2")),
            IPAddressSpace::kPublic);
}

// Verifies that the address space of IPv6 null address (::/128) is `loopback`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV6Null) {
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("::")),
            IPAddressSpace::kLoopback);
}

// Verifies that the address space of IP addresses belonging to the
// "Carrier Grade NAT" 100.64.0.0/10 block are `local`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV4CarrierGradeNat) {
  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(100, 63, 255, 255)),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(100, 64, 0, 0)),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(100, 127, 255, 255)),
            IPAddressSpace::kLocal);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(100, 128, 0, 0)),
            IPAddressSpace::kPublic);
}

// Verifies that the address space of IPv6 addresses in the "Documentation"
// (2001:db8::/32 and 3fff::/20) address blocks are `local`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV6Documentation) {
  // 2001:db8::/32
  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(
                ParseIPAddress("2001:db7:ffff:ffff:ffff:ffff:ffff:ffff")),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("2001:db8::")),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(
                ParseIPAddress("2001:db8:ffff:ffff:ffff:ffff:ffff:ffff")),
            IPAddressSpace::kLocal);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("2001:db9::")),
            IPAddressSpace::kPublic);

  // 3fff::/20
  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(
                ParseIPAddress("3ffe:ffff:ffff:ffff:ffff:ffff:ffff:ffff")),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("3fff::")),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(
                ParseIPAddress("3fff:0fff:ffff:ffff:ffff:ffff:ffff:ffff")),
            IPAddressSpace::kLocal);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("3fff:ffff::")),
            IPAddressSpace::kPublic);
}

// Verifies that the address space of IPv6 addresses in the "Site-local unicast"
// (fec0::/10) address block is `local`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV6SiteLocalUnicast) {
  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("fec0:0:0:0:0:0:0:0")),
            IPAddressSpace::kLocal);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("fec0::")),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(
                ParseIPAddress("feff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")),
            IPAddressSpace::kLocal);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("ff00::")),
            IPAddressSpace::kPublic);
}

// Verifies that IPv4-mapped IPv6 addresses belong to the address space of the
// mapped IPv4 address.
TEST(IPAddressSpaceTest, IPEndPointToAddressSpaceIPv4MappedIPv6) {
  EXPECT_EQ(IPAddressToIPAddressSpace(
                net::ConvertIPv4ToIPv4MappedIPv6(PublicIPv4Address())),
            IPAddressSpace::kPublic);

  EXPECT_EQ(IPAddressToIPAddressSpace(
                net::ConvertIPv4ToIPv4MappedIPv6(PrivateIPv4Address())),
            IPAddressSpace::kLocal);

  EXPECT_EQ(IPAddressToIPAddressSpace(
                net::ConvertIPv4ToIPv4MappedIPv6(IPAddress::IPv4Localhost())),
            IPAddressSpace::kLoopback);
}

// Verifies that 0.0.0.0/8 is mapped to non-public address spaces.
TEST(IPAddressSpaceTest, IPEndPointToAddressSpaceNullIP) {
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(0, 0, 0, 0)),
            IPAddressSpace::kLoopback);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(0, 0, 0, 4)),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(0, 255, 255, 255)),
            IPAddressSpace::kLocal);
}

class IPAddressSpaceOverridesTest : public testing::Test {
 public:
  void SetUp() override {
    network::IPAddressSpaceOverrides::GetInstance().ResetForTesting();
  }
};

// Verifies that overrides can be present and empty, in which case it is
// ignored.
TEST_F(IPAddressSpaceOverridesTest, IPEndPointToAddressSpaceOverrideEmpty) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());

  // Check a single address, to make sure things do not crash.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress::IPv6Localhost()),
            IPAddressSpace::kLoopback);
}

// Verifies that a single IPv4 endpoints can be overridden.
TEST_F(IPAddressSpaceOverridesTest, IPEndPointToAddressSpaceOverrideSingle) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "127.0.0.1:80=public", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());

  // Wrong IP address.
  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(127, 0, 0, 0), 80)),
            IPAddressSpace::kLoopback);

  // Wrong port.
  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(127, 0, 0, 1), 81)),
            IPAddressSpace::kLoopback);

  // Exact match.
  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(127, 0, 0, 1), 80)),
            IPAddressSpace::kPublic);
}

// Verifies that multiple IPv4 endpoints can be overridden.
TEST_F(IPAddressSpaceOverridesTest, IPEndPointToAddressSpaceOverrideMultiple) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "10.2.3.4:80=public,8.8.8.8:8888=local", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());

  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(10, 2, 3, 4), 80)),
            IPAddressSpace::kPublic);

  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(8, 8, 8, 8), 8888)),
            IPAddressSpace::kLocal);
}

// Verifies that a port of 0 will apply to all ports
TEST_F(IPAddressSpaceOverridesTest,
       IPEndPointToAddressSpaceOverrideWildcardPort) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "10.2.3.4:0=public,[2001::]:0=loopback", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());
  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(10, 2, 3, 4), 80)),
            IPAddressSpace::kPublic);

  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(10, 2, 3, 4), 200)),
            IPAddressSpace::kPublic);

  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("2001::"), 2000)),
      IPAddressSpace::kLoopback);

  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("2001::"), 2001)),
      IPAddressSpace::kLoopback);
}

// Verifies that private and local are both the kLocal address space
TEST_F(IPAddressSpaceOverridesTest,
       IPEndPointToAddressSpaceOverrideLocalPrivateSame) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "10.2.3.4:80=private,8.8.8.8:8888=local", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());

  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(10, 2, 3, 4), 80)),
            IPAddressSpace::kLocal);

  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(8, 8, 8, 8), 8888)),
            IPAddressSpace::kLocal);
}

// Verifies that invalid entries in the override comma-separated list
// are simply ignored, and that subsequent entries are still applied.
TEST_F(IPAddressSpaceOverridesTest, IPEndPointToAddressSpaceOverrideInvalid) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      ","                      // Empty.
      "1.2.3.4:80foo=public,"  // Invalid port.
      "1.2.3.4:65536=local,"   // Port out of range.
      "1.2.3.4:80=potato,"     // Invalid address space.
      "1.2.3.4:=public,"       // Missing port.
      "1.2.3.4=public,"        // Missing colon, port.
      "1.2.3.4:80=,"           // Missing address space.
      "1.2.3.4:80,"            // Missing equal, address space.
      "1:80=public,"           // Surprisingly, "1:80" parses as "0.0.0.1:80"
      "1.2.3.4:80=local",
      &rejected_patterns);
  EXPECT_THAT(rejected_patterns,
              testing::UnorderedElementsAre(
                  "1.2.3.4:80foo=public",  // Invalid port.
                  "1.2.3.4:65536=local",   // Port out of range.
                  "1.2.3.4:80=potato",     // Invalid address space.
                  "1.2.3.4:=public",       // Missing port.
                  "1.2.3.4=public",        // Missing colon, port.
                  "1.2.3.4:80=",           // Missing address space.
                  "1.2.3.4:80"));          // Missing equal, address space.

  // Valid override applies, despite preceding garbage.
  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(1, 2, 3, 4), 80)),
            IPAddressSpace::kLocal);
}

// Verifies that overrides that overlap with previously-given overrides are
// ignored. In other words, the first matching override is applied.
TEST_F(IPAddressSpaceOverridesTest, IPEndPointToAddressSpaceOverrideOverlap) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "8.8.8.8:80=loopback,8.8.8.8:80=local", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());

  // The first matching override applies.
  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(8, 8, 8, 8), 80)),
            IPAddressSpace::kLoopback);
}

// Verifies that invalid IP addresses are not subject to overrides.
TEST_F(IPAddressSpaceOverridesTest,
       IPEndPointToAddressSpaceOverrideInvalidAddress) {
  // 0.0.0.0:80 should not really match the invalid IP address, but it is still
  // the most likely to match.
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "0.0.0.0:80=loopback", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());

  // Check that the override *does not apply* to an invalid IP address.
  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(), 80)),
            IPAddressSpace::kUnknown);
}

// Verifies that overrides can specify IPv6 addresses.
TEST_F(IPAddressSpaceOverridesTest, IPEndPointToAddressSpaceOverrideV6) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "[2001::]:2001=loopback,[2020::1]:1234=local", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());

  // First override.

  // Wrong address.
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("2001::1"), 2001)),
      IPAddressSpace::kPublic);

  // Wrong port.
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("2001::"), 2000)),
      IPAddressSpace::kPublic);

  // Exact match.
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("2001::"), 2001)),
      IPAddressSpace::kLoopback);

  // Second override block.

  // Wrong address.
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("2020::"), 1234)),
      IPAddressSpace::kPublic);

  // Wrong port.
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("2020::1"), 1235)),
      IPAddressSpace::kPublic);

  // Exact match.
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("2020::1"), 1234)),
      IPAddressSpace::kLocal);
}

TEST(IPAddressSpaceTest, TransportInfoToIPAddressSpaceProxiedIsUnknown) {
  EXPECT_EQ(TransportInfoToIPAddressSpace(
                ProxiedTransport(IPEndPoint(IPAddress(1, 2, 3, 4), 80))),
            IPAddressSpace::kUnknown);
}

TEST_F(IPAddressSpaceOverridesTest,
       TransportInfoToIPAddressSpaceProxiedIgnoresOverrides) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "127.0.0.1:80=public", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());

  EXPECT_EQ(TransportInfoToIPAddressSpace(
                ProxiedTransport(IPEndPoint(IPAddress(127, 0, 0, 1), 80))),
            IPAddressSpace::kUnknown);
}

TEST(IPAddressSpaceTest,
     TransportInfoToIPAddressSpaceCachedFromProxyIsUnknown) {
  EXPECT_EQ(TransportInfoToIPAddressSpace(
                MakeTransport(TransportType::kCachedFromProxy,
                              IPEndPoint(IPAddress(1, 2, 3, 4), 80))),
            IPAddressSpace::kUnknown);
}

TEST_F(IPAddressSpaceOverridesTest,
       TransportInfoToIPAddressSpaceCachedFromProxyIgnoresOverrides) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "127.0.0.1:80=public", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());

  EXPECT_EQ(TransportInfoToIPAddressSpace(
                MakeTransport(TransportType::kCachedFromProxy,
                              IPEndPoint(IPAddress(127, 0, 0, 1), 80))),
            IPAddressSpace::kUnknown);
}

// Verifies that IPv4-mapped IPv6 addresses are not overridden as though they
// were the mapped IPv4 address instead.
TEST_F(IPAddressSpaceOverridesTest,
       IPEndPointToAddressSpaceOverrideIPv4MappedIPv6) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "127.0.0.1:80=public", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());

  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(
                net::ConvertIPv4ToIPv4MappedIPv6(IPAddress(127, 0, 0, 1)), 80)),
            IPAddressSpace::kLoopback);
}

// Basic CIDR override test.
TEST_F(IPAddressSpaceOverridesTest, CIDROverride) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "10.0.0.1/8=loopback", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());
  // 10.*.*.* address matches
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("10.64.2.4"), 2001)),
      IPAddressSpace::kLoopback);

  // and the port doesn't matter
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("10.64.2.4"), 1002)),
      IPAddressSpace::kLoopback);

  // and these don't match.
  EXPECT_EQ(IPEndPointToIPAddressSpace(
                IPEndPoint(ParseIPAddress("192.168.0.1"), 2001)),
            IPAddressSpace::kLocal);
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("11.64.2.4"), 1002)),
      IPAddressSpace::kPublic);
}

// Check that the 0.0.0.0/0 CIDR will override every IPv4 address.
TEST_F(IPAddressSpaceOverridesTest, CIDROverrideAll) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "0.0.0.0/0=public", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("10.64.2.4"), 2001)),
      IPAddressSpace::kPublic);
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("10.64.2.4"), 1002)),
      IPAddressSpace::kPublic);
  EXPECT_EQ(IPEndPointToIPAddressSpace(
                IPEndPoint(ParseIPAddress("192.168.0.1"), 2001)),
            IPAddressSpace::kPublic);
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("11.64.2.4"), 1002)),
      IPAddressSpace::kPublic);
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("127.0.0.1"), 1002)),
      IPAddressSpace::kPublic);
}

TEST_F(IPAddressSpaceOverridesTest, CIDROverrideV6) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "[2001::]/16=loopback,[2020::1]/16=local", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());

  // First override.
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("2001::1"), 2001)),
      IPAddressSpace::kLoopback);
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("2001::"), 2001)),
      IPAddressSpace::kLoopback);

  // Second override.
  EXPECT_EQ(IPEndPointToIPAddressSpace(
                IPEndPoint(ParseIPAddress("2020:1234::"), 1234)),
            IPAddressSpace::kLocal);
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("2020::1"), 1234)),
      IPAddressSpace::kLocal);

  // Neither
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("2002::1"), 2001)),
      IPAddressSpace::kPublic);
}

// Check that the [::]/0 CIDR will override every IPv6 address.
TEST_F(IPAddressSpaceOverridesTest, CIDRV6OverrideAll) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "[::]/0=public", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("fc00::4"), 2001)),
      IPAddressSpace::kPublic);
  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("::"), 1002)),
            IPAddressSpace::kPublic);
  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("::1"), 2001)),
            IPAddressSpace::kPublic);
  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(ParseIPAddress("11.64.2.4"), 1002)),
      IPAddressSpace::kPublic);
  EXPECT_EQ(IPEndPointToIPAddressSpace(
                IPEndPoint(ParseIPAddress("::ffff:5:1"), 1002)),
            IPAddressSpace::kPublic);
}

// Verifies that IPv4-mapped IPv6 addresses are not overridden as though they
// were the mapped IPv4 address instead.
TEST_F(IPAddressSpaceOverridesTest, CIDROverrideIPv4MappedIPv6) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "10.0.0.16/8=public", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());

  EXPECT_EQ(
      IPEndPointToIPAddressSpace(IPEndPoint(
          net::ConvertIPv4ToIPv4MappedIPv6(IPAddress(10, 64, 10, 1)), 80)),
      IPAddressSpace::kLocal);
}

TEST(IPAddressSpaceTest, IsLessPublicAddressSpaceThanLoopback) {
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kLoopback,
                                        IPAddressSpace::kLoopback));

  EXPECT_TRUE(IsLessPublicAddressSpace(IPAddressSpace::kLoopback,
                                       IPAddressSpace::kLocal));
  EXPECT_TRUE(IsLessPublicAddressSpace(IPAddressSpace::kLoopback,
                                       IPAddressSpace::kPublic));
  EXPECT_TRUE(IsLessPublicAddressSpace(IPAddressSpace::kLoopback,
                                       IPAddressSpace::kUnknown));

  EXPECT_FALSE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kLoopback,
                                           IPAddressSpace::kLoopback));
  EXPECT_FALSE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kLoopback,
                                           IPAddressSpace::kLocal));

  EXPECT_TRUE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kLoopback,
                                          IPAddressSpace::kPublic));
  EXPECT_TRUE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kLoopback,
                                          IPAddressSpace::kUnknown));
}

TEST(IPAddressSpaceTest, IsLessPublicAddressSpaceThanLocal) {
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kLocal,
                                        IPAddressSpace::kLoopback));
  EXPECT_FALSE(
      IsLessPublicAddressSpace(IPAddressSpace::kLocal, IPAddressSpace::kLocal));

  EXPECT_TRUE(IsLessPublicAddressSpace(IPAddressSpace::kLocal,
                                       IPAddressSpace::kPublic));
  EXPECT_TRUE(IsLessPublicAddressSpace(IPAddressSpace::kLocal,
                                       IPAddressSpace::kUnknown));

  EXPECT_FALSE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kLocal,
                                           IPAddressSpace::kLoopback));
  EXPECT_FALSE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kLocal,
                                           IPAddressSpace::kLocal));

  EXPECT_TRUE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kLocal,
                                          IPAddressSpace::kPublic));
  EXPECT_TRUE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kLocal,
                                          IPAddressSpace::kUnknown));
}

TEST(IPAddressSpaceTest, IsLessPublicAddressSpaceThanPublic) {
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPublic,
                                        IPAddressSpace::kLoopback));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPublic,
                                        IPAddressSpace::kLocal));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPublic,
                                        IPAddressSpace::kPublic));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kPublic,
                                        IPAddressSpace::kUnknown));

  EXPECT_FALSE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kPublic,
                                           IPAddressSpace::kLoopback));
  EXPECT_FALSE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kPublic,
                                           IPAddressSpace::kLocal));
  EXPECT_FALSE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kPublic,
                                           IPAddressSpace::kPublic));
  EXPECT_FALSE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kPublic,
                                           IPAddressSpace::kUnknown));
}

TEST(IPAddressSpaceTest, IsLessPublicAddressSpaceThanUnknown) {
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kUnknown,
                                        IPAddressSpace::kLoopback));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kUnknown,
                                        IPAddressSpace::kLocal));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kUnknown,
                                        IPAddressSpace::kPublic));
  EXPECT_FALSE(IsLessPublicAddressSpace(IPAddressSpace::kUnknown,
                                        IPAddressSpace::kUnknown));

  EXPECT_FALSE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kUnknown,
                                           IPAddressSpace::kLoopback));
  EXPECT_FALSE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kUnknown,
                                           IPAddressSpace::kLocal));
  EXPECT_FALSE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kUnknown,
                                           IPAddressSpace::kPublic));
  EXPECT_FALSE(IsLessPublicAddressSpaceLNA(IPAddressSpace::kUnknown,
                                           IPAddressSpace::kUnknown));
}

TEST(IPAddressSpaceUtilTest, CalculateClientAddressSpaceFileURL) {
  EXPECT_EQ(IPAddressSpace::kLoopback,
            CalculateClientAddressSpace(GURL("file:///foo"), std::nullopt));
}

TEST(IPAddressSpaceUtilTest,
     CalculateClientAddressSpaceInheritedFromServiceWorker) {
  auto parsed_headers = ParsedHeaders::New();
  auto remote_endpoint = IPEndPoint();
  for (const auto ip_address_space : {
           mojom::IPAddressSpace::kLoopback,
           mojom::IPAddressSpace::kLocal,
           mojom::IPAddressSpace::kPublic,
       }) {
    for (const auto& url : {
             GURL("file:///foo"),
             GURL("http://foo.test"),
         }) {
      CalculateClientAddressSpaceParams params{
          .client_address_space_inherited_from_service_worker =
              ip_address_space,
          .parsed_headers = &parsed_headers,
          .remote_endpoint = &remote_endpoint,
      };
      EXPECT_EQ(ip_address_space,
                CalculateClientAddressSpace(GURL(url), params));
    }
  }
}

TEST(IPAddressSpaceUtilTest, CalculateClientAddressSpaceNullParams) {
  EXPECT_EQ(IPAddressSpace::kUnknown,
            CalculateClientAddressSpace(GURL("http://foo.test"), std::nullopt));
}

TEST(IPAddressSpaceUtilTest, CalculateClientAddressSpaceEmptyParams) {
  auto parsed_headers = ParsedHeaders::New();
  auto remote_endpoint = net::IPEndPoint();
  CalculateClientAddressSpaceParams params{
      .client_address_space_inherited_from_service_worker = std::nullopt,
      .parsed_headers = &parsed_headers,
      .remote_endpoint = &remote_endpoint,
  };
  EXPECT_EQ(IPAddressSpace::kUnknown,
            CalculateClientAddressSpace(GURL("http://foo.test"), params));
}

TEST(IPAddressSpaceUtilTest, CalculateClientAddressSpaceIPAddress) {
  auto parsed_headers = ParsedHeaders::New();
  auto remote_endpoint = IPEndPoint(PrivateIPv4Address(), 1234);
  CalculateClientAddressSpaceParams params{
      .client_address_space_inherited_from_service_worker = std::nullopt,
      .parsed_headers = &parsed_headers,
      .remote_endpoint = &remote_endpoint,
  };

  EXPECT_EQ(IPAddressSpace::kLocal,
            CalculateClientAddressSpace(GURL("http://foo.test"), params));
}

TEST(IPAddressSpaceUtilTest, CalculateClientAddressSpaceTreatAsPublicAddress) {
  auto csp = ContentSecurityPolicy::New();
  csp->treat_as_public_address = true;
  auto parsed_headers = ParsedHeaders::New();
  parsed_headers->content_security_policy.push_back(std::move(csp));
  auto remote_endpoint = IPEndPoint(IPAddress::IPv4Localhost(), 1234);

  CalculateClientAddressSpaceParams params{
      .client_address_space_inherited_from_service_worker = std::nullopt,
      .parsed_headers = &parsed_headers,
      .remote_endpoint = &remote_endpoint,
  };

  EXPECT_EQ(IPAddressSpace::kPublic,
            CalculateClientAddressSpace(GURL("http://foo.test"), params));
}

TEST_F(IPAddressSpaceOverridesTest, CalculateClientAddressSpaceOverride) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "10.2.3.4:80=public,8.8.8.8:8888=local", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());

  auto parsed_headers = ParsedHeaders::New();
  auto remote_endpoint = IPEndPoint(IPAddress(10, 2, 3, 4), 80);

  CalculateClientAddressSpaceParams params{
      .client_address_space_inherited_from_service_worker = std::nullopt,
      .parsed_headers = &parsed_headers,
      .remote_endpoint = &remote_endpoint,
  };

  EXPECT_EQ(IPAddressSpace::kPublic,
            CalculateClientAddressSpace(GURL("http://foo.test"), params));

  remote_endpoint = IPEndPoint(IPAddress(8, 8, 8, 8), 8888);

  EXPECT_EQ(IPAddressSpace::kLocal,
            CalculateClientAddressSpace(GURL("http://foo.test"), params));
}

TEST(IPAddressSpaceTest, CalculateResourceAddressSpaceFileURL) {
  EXPECT_EQ(IPAddressSpace::kLoopback,
            CalculateResourceAddressSpace(GURL("file:///foo"), IPEndPoint()));
}

TEST(IPAddressSpaceTest, CalculateResourceAddressSpaceIPAddress) {
  EXPECT_EQ(
      IPAddressSpace::kLoopback,
      CalculateResourceAddressSpace(
          GURL("http://foo.test"), IPEndPoint(IPAddress::IPv4Localhost(), 80)));
  EXPECT_EQ(IPAddressSpace::kLocal,
            CalculateResourceAddressSpace(
                GURL("http://foo.test"), IPEndPoint(PrivateIPv4Address(), 80)));
  EXPECT_EQ(IPAddressSpace::kPublic,
            CalculateResourceAddressSpace(GURL("http://foo.test"),
                                          IPEndPoint(PublicIPv4Address(), 80)));
  EXPECT_EQ(
      IPAddressSpace::kUnknown,
      CalculateResourceAddressSpace(GURL("http://foo.test"), IPEndPoint()));
}

TEST_F(IPAddressSpaceOverridesTest, CalculateResourceAddressSpaceOverride) {
  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "10.2.3.4:80=public,8.8.8.8:8888=local", &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());

  EXPECT_EQ(
      IPAddressSpace::kPublic,
      CalculateResourceAddressSpace(GURL("http://foo.test"),
                                    IPEndPoint(IPAddress(10, 2, 3, 4), 80)));
  EXPECT_EQ(
      IPAddressSpace::kLocal,
      CalculateResourceAddressSpace(GURL("http://foo.test"),
                                    IPEndPoint(IPAddress(8, 8, 8, 8), 8888)));
}

TEST(IPAddressSpaceTest, ParsePrivateIpFromURL) {
  EXPECT_EQ(std::nullopt, ParsePrivateIpFromUrl(GURL("http://foo.test")));
  EXPECT_EQ(std::nullopt, ParsePrivateIpFromUrl(GURL("http://8.8.8.8")));
  EXPECT_EQ(IPAddress(192, 168, 1, 10),
            ParsePrivateIpFromUrl(GURL("http://192.168.1.10")));
  EXPECT_EQ(IPAddress(10, 168, 1, 10),
            ParsePrivateIpFromUrl(GURL("http://10.168.1.10")));
}

TEST_F(IPAddressSpaceOverridesTest, GetAddressSpaceFromUrl) {
  EXPECT_EQ(std::nullopt, GetAddressSpaceFromUrl(GURL("http://foo.test")));
  EXPECT_EQ(IPAddressSpace::kPublic,
            GetAddressSpaceFromUrl(GURL("http://8.8.8.8")));
  EXPECT_EQ(IPAddressSpace::kPublic,
            GetAddressSpaceFromUrl(GURL("https://8.8.8.8")));
  EXPECT_EQ(IPAddressSpace::kLocal,
            GetAddressSpaceFromUrl(GURL("http://192.168.1.10")));
  EXPECT_EQ(IPAddressSpace::kLocal,
            GetAddressSpaceFromUrl(GURL("http://10.168.1.10")));
  EXPECT_EQ(IPAddressSpace::kLoopback,
            GetAddressSpaceFromUrl(GURL("http://localhost")));
  EXPECT_EQ(IPAddressSpace::kLoopback,
            GetAddressSpaceFromUrl(GURL("https://localhost")));
  EXPECT_EQ(IPAddressSpace::kLoopback,
            GetAddressSpaceFromUrl(GURL("https://localhost.")));
  EXPECT_EQ(IPAddressSpace::kLoopback,
            GetAddressSpaceFromUrl(GURL("http://foo.localhost")));
  EXPECT_EQ(IPAddressSpace::kLoopback,
            GetAddressSpaceFromUrl(GURL("http://foo.localhost.")));
  EXPECT_EQ(IPAddressSpace::kLocal,
            GetAddressSpaceFromUrl(GURL("http://local")));
  EXPECT_EQ(IPAddressSpace::kLocal,
            GetAddressSpaceFromUrl(GURL("http://local.")));
  EXPECT_EQ(IPAddressSpace::kLocal,
            GetAddressSpaceFromUrl(GURL("http://menu.local")));
  EXPECT_EQ(IPAddressSpace::kLocal,
            GetAddressSpaceFromUrl(GURL("https://menu.local")));
  EXPECT_EQ(IPAddressSpace::kLocal,
            GetAddressSpaceFromUrl(GURL("https://menu.local.")));
  EXPECT_EQ(IPAddressSpace::kLocal,
            GetAddressSpaceFromUrl(GURL("http://menu.local:8000")));

  std::vector<std::string> rejected_patterns;
  network::IPAddressSpaceOverrides::GetInstance().SetAuxiliaryOverrides(
      "10.2.3.4:80=public,8.8.8.8:8888=local,127.0.0.1:80=public",
      &rejected_patterns);
  EXPECT_TRUE(rejected_patterns.empty());

  EXPECT_EQ(IPAddressSpace::kPublic,
            GetAddressSpaceFromUrl(GURL("http://10.2.3.4:80")));
  EXPECT_EQ(IPAddressSpace::kPublic,
            GetAddressSpaceFromUrl(GURL("http://10.2.3.4")));
  EXPECT_EQ(IPAddressSpace::kLocal,
            GetAddressSpaceFromUrl(GURL("https://10.2.3.4")));
  EXPECT_EQ(IPAddressSpace::kLocal,
            GetAddressSpaceFromUrl(GURL("http://8.8.8.8:8888")));
  EXPECT_EQ(IPAddressSpace::kPublic,
            GetAddressSpaceFromUrl(GURL("http://8.8.8.8")));
  EXPECT_EQ(IPAddressSpace::kPublic,
            GetAddressSpaceFromUrl(GURL("https://8.8.8.8")));
  EXPECT_EQ(IPAddressSpace::kPublic,
            GetAddressSpaceFromUrl(GURL("http://localhost")));
  EXPECT_EQ(IPAddressSpace::kLoopback,
            GetAddressSpaceFromUrl(GURL("https://localhost")));
}

}  // namespace
}  // namespace network

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

// Helper for tests that do not care about command-line overrides.
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
// three "Private Use" address blocks defined in RFC 1918 is `private`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV4PrivateUse) {
  EXPECT_EQ(IPAddressToIPAddressSpace(PrivateIPv4Address()),
            IPAddressSpace::kPrivate);

  // 10.0.0.0/8

  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(9, 255, 255, 255)),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(10, 0, 0, 0)),
            IPAddressSpace::kPrivate);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(10, 255, 255, 255)),
            IPAddressSpace::kPrivate);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(11, 0, 0, 0)),
            IPAddressSpace::kPublic);

  // 172.16.0.0/12

  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(172, 15, 255, 255)),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(172, 16, 0, 0)),
            IPAddressSpace::kPrivate);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(172, 31, 255, 255)),
            IPAddressSpace::kPrivate);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(172, 32, 0, 0)),
            IPAddressSpace::kPublic);

  // 192.168.0.0/16

  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(192, 167, 255, 255)),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(192, 168, 0, 0)),
            IPAddressSpace::kPrivate);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(192, 168, 255, 255)),
            IPAddressSpace::kPrivate);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(169, 169, 0, 0)),
            IPAddressSpace::kPublic);
}

// Verifies that the address space of IP addresses belonging to the "Link-local"
// 169.254.0.0/16 block are `private`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV4LinkLocal) {
  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(169, 253, 255, 255)),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(169, 254, 0, 0)),
            IPAddressSpace::kPrivate);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(169, 254, 255, 255)),
            IPAddressSpace::kPrivate);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(169, 255, 0, 0)),
            IPAddressSpace::kPublic);
}

// Verifies that the address space of IPv4 localhost and the rest of the
// 127.0.0.0/8 block is `local`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV4Localhost) {
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress::IPv4Localhost()),
            IPAddressSpace::kLocal);

  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(126, 255, 255, 255)),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 0, 0, 0)),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(127, 255, 255, 255)),
            IPAddressSpace::kLocal);

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
// (fc00::/7) address block is `private`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV6UniqueLocal) {
  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(
                ParseIPAddress("fbff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("fc00::")),
            IPAddressSpace::kPrivate);
  EXPECT_EQ(IPAddressToIPAddressSpace(
                ParseIPAddress("fdff:ffff:ffff:ffff:ffff:ffff:ffff:ffff")),
            IPAddressSpace::kPrivate);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("fe00::")),
            IPAddressSpace::kPublic);
}

// Verifies that the address space of IPv6 addresses in the "Link-local unicast"
// (fe80::/10) address block is `private`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV6LinkLocalUnicast) {
  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(
                ParseIPAddress("fe7f:ffff:ffff:ffff:ffff:ffff:ffff:ffff")),
            IPAddressSpace::kPublic);

  // Lower and upper bounds (inclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("fe80::")),
            IPAddressSpace::kPrivate);
  EXPECT_EQ(IPAddressToIPAddressSpace(
                ParseIPAddress("febf:ffff:ffff:ffff:ffff:ffff:ffff:ffff")),
            IPAddressSpace::kPrivate);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("fec0::")),
            IPAddressSpace::kPublic);
}

// Verifies that the address space of IPv6 localhost (::1/128) is `local`.
TEST(IPAddressSpaceTest, IPEndPointToIPAddressSpaceV6Localhost) {
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress::IPv6Localhost()),
            IPAddressSpace::kLocal);

  // Lower bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("::0")),
            IPAddressSpace::kPublic);

  // Upper bound (exclusive).
  EXPECT_EQ(IPAddressToIPAddressSpace(ParseIPAddress("::2")),
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
            IPAddressSpace::kPrivate);

  EXPECT_EQ(IPAddressToIPAddressSpace(
                net::ConvertIPv4ToIPv4MappedIPv6(IPAddress::IPv4Localhost())),
            IPAddressSpace::kLocal);
}

// Verifies that 0.0.0.0/8 is mapped to non-public address spaces.
TEST(IPAddressSpaceTest, IPEndPointToAddressSpaceNullIP) {
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(0, 0, 0, 0)),
            IPAddressSpace::kLocal);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(0, 0, 0, 4)),
            IPAddressSpace::kPrivate);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(0, 255, 255, 255)),
            IPAddressSpace::kPrivate);
}

// Verifies that 0.0.0.0/8 is mapped to the public address space if configured
// via feature flag.
TEST(IPAddressSpaceTest, IPEndPointToAddressSpaceNullIPKillSwitch) {
  base::test::ScopedFeatureList enable{
      features::kTreatNullIPAsPublicAddressSpace};
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(0, 0, 0, 0)),
            IPAddressSpace::kPublic);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(0, 0, 0, 4)),
            IPAddressSpace::kPublic);
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress(0, 255, 255, 255)),
            IPAddressSpace::kPublic);
}

// Verifies that the `ip-address-space-overrides` switch can be present and
// empty, in which case it is ignored.
TEST(IPAddressSpaceTest, IPEndPointToAddressSpaceOverrideEmpty) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides, "");

  // Check a single address, to make sure things do not crash.
  EXPECT_EQ(IPAddressToIPAddressSpace(IPAddress::IPv6Localhost()),
            IPAddressSpace::kLocal);
}

// Verifies that a single IPv4 endpoints can be overridden.
TEST(IPAddressSpaceTest, IPEndPointToAddressSpaceOverrideSingle) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides,
                                 "127.0.0.1:80=public");

  // Wrong IP address.
  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(127, 0, 0, 0), 80)),
            IPAddressSpace::kLocal);

  // Wrong port.
  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(127, 0, 0, 1), 81)),
            IPAddressSpace::kLocal);

  // Exact match.
  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(127, 0, 0, 1), 80)),
            IPAddressSpace::kPublic);
}

// Verifies that multiple IPv4 endpoints can be overridden.
TEST(IPAddressSpaceTest, IPEndPointToAddressSpaceOverrideMultiple) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides,
                                 "10.2.3.4:80=public,8.8.8.8:8888=private");

  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(10, 2, 3, 4), 80)),
            IPAddressSpace::kPublic);

  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(8, 8, 8, 8), 8888)),
            IPAddressSpace::kPrivate);
}

// Verifies that invalid entries in the command-line switch comma-separated list
// are simply ignored, and that subsequent entries are still applied.
TEST(IPAddressSpaceTest, IPEndPointToAddressSpaceOverrideInvalid) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides,
                                 ","                       // Empty.
                                 "1.2.3.4:80foo=public,"   // Invalid port.
                                 "1.2.3.4:65536=private,"  // Port out of range.
                                 "1:80=public,"            // Invalid address.
                                 "1.2.3.4:80=potato,"  // Invalid address space.
                                 "1.2.3.4:=public,"    // Missing port.
                                 "1.2.3.4=public,"     // Missing colon, port.
                                 "1.2.3.4:80=,"        // Missing address space.
                                 "1.2.3.4:80,"  // Missing equal, address space.
                                 "1.2.3.4:80=private");

  // Valid override applies, despite preceding garbage.
  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(1, 2, 3, 4), 80)),
            IPAddressSpace::kPrivate);
}

// Verifies that command-line overrides that overlap with previously-given
// overrides are ignored. In other words, the first matching override is
// applied.
TEST(IPAddressSpaceTest, IPEndPointToAddressSpaceOverrideOverlap) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides,
                                 "8.8.8.8:80=local,8.8.8.8:80=private");

  // The first matching override applies.
  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(8, 8, 8, 8), 80)),
            IPAddressSpace::kLocal);
}

// Verifies that invalid IP addresses are not subject to overrides.
TEST(IPAddressSpaceTest, IPEndPointToAddressSpaceOverrideInvalidAddress) {
  // 0.0.0.0:80 should not really match the invalid IP address, but it is still
  // the most likely to match.
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides,
                                 "0.0.0.0:80=local");

  // Check that the override *does not apply* to an invalid IP address.
  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(IPAddress(), 80)),
            IPAddressSpace::kUnknown);
}

// Verifies that command-line overrides can specify IPv6 addresses.
TEST(IPAddressSpaceTest, IPEndPointToAddressSpaceOverrideV6) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides,
                                 "[2001::]:2001=local,[2020::1]:1234=private");

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
      IPAddressSpace::kLocal);

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
      IPAddressSpace::kPrivate);
}

TEST(IPAddressSpaceTest, TransportInfoToIPAddressSpaceProxiedIsUnknown) {
  EXPECT_EQ(TransportInfoToIPAddressSpace(
                ProxiedTransport(IPEndPoint(IPAddress(1, 2, 3, 4), 80))),
            IPAddressSpace::kUnknown);
}

TEST(IPAddressSpaceTest, TransportInfoToIPAddressSpaceProxiedIgnoresOverrides) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides,
                                 "127.0.0.1:80=public");

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

TEST(IPAddressSpaceTest,
     TransportInfoToIPAddressSpaceCachedFromProxyIgnoresOverrides) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides,
                                 "127.0.0.1:80=public");

  EXPECT_EQ(TransportInfoToIPAddressSpace(
                MakeTransport(TransportType::kCachedFromProxy,
                              IPEndPoint(IPAddress(127, 0, 0, 1), 80))),
            IPAddressSpace::kUnknown);
}

// Verifies that IPv4-mapped IPv6 addresses are not overridden as though they
// were the mapped IPv4 address instead.
TEST(IPAddressSpaceTest, IPEndPointToAddressSpaceOverrideIPv4MappedIPv6) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(switches::kIpAddressSpaceOverrides,
                                 "127.0.0.1:80=public");

  EXPECT_EQ(IPEndPointToIPAddressSpace(IPEndPoint(
                net::ConvertIPv4ToIPv4MappedIPv6(IPAddress(127, 0, 0, 1)), 80)),
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

TEST(IPAddressSpaceUtilTest, CalculateClientAddressSpaceFileURL) {
  EXPECT_EQ(IPAddressSpace::kLocal,
            CalculateClientAddressSpace(GURL("file:///foo"), std::nullopt));
}

TEST(IPAddressSpaceUtilTest,
     CalculateClientAddressSpaceInheritedFromServiceWorker) {
  auto parsed_headers = ParsedHeaders::New();
  auto remote_endpoint = IPEndPoint();
  for (const auto ip_address_space : {
           mojom::IPAddressSpace::kLocal,
           mojom::IPAddressSpace::kPrivate,
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

  EXPECT_EQ(IPAddressSpace::kPrivate,
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

TEST(IPAddressSpaceTest, CalculateClientAddressSpaceOverride) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(network::switches::kIpAddressSpaceOverrides,
                                 "10.2.3.4:80=public,8.8.8.8:8888=private");

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

  EXPECT_EQ(IPAddressSpace::kPrivate,
            CalculateClientAddressSpace(GURL("http://foo.test"), params));
}

TEST(IPAddressSpaceTest, CalculateResourceAddressSpaceFileURL) {
  EXPECT_EQ(IPAddressSpace::kLocal,
            CalculateResourceAddressSpace(GURL("file:///foo"), IPEndPoint()));
}

TEST(IPAddressSpaceTest, CalculateResourceAddressSpaceIPAddress) {
  EXPECT_EQ(
      IPAddressSpace::kLocal,
      CalculateResourceAddressSpace(
          GURL("http://foo.test"), IPEndPoint(IPAddress::IPv4Localhost(), 80)));
  EXPECT_EQ(IPAddressSpace::kPrivate,
            CalculateResourceAddressSpace(
                GURL("http://foo.test"), IPEndPoint(PrivateIPv4Address(), 80)));
  EXPECT_EQ(IPAddressSpace::kPublic,
            CalculateResourceAddressSpace(GURL("http://foo.test"),
                                          IPEndPoint(PublicIPv4Address(), 80)));
  EXPECT_EQ(
      IPAddressSpace::kUnknown,
      CalculateResourceAddressSpace(GURL("http://foo.test"), IPEndPoint()));
}

TEST(IPAddressSpaceTest, CalculateResourceAddressSpaceOverride) {
  auto& command_line = *base::CommandLine::ForCurrentProcess();
  command_line.AppendSwitchASCII(network::switches::kIpAddressSpaceOverrides,
                                 "10.2.3.4:80=public,8.8.8.8:8888=private");

  EXPECT_EQ(
      IPAddressSpace::kPublic,
      CalculateResourceAddressSpace(GURL("http://foo.test"),
                                    IPEndPoint(IPAddress(10, 2, 3, 4), 80)));
  EXPECT_EQ(
      IPAddressSpace::kPrivate,
      CalculateResourceAddressSpace(GURL("http://foo.test"),
                                    IPEndPoint(IPAddress(8, 8, 8, 8), 8888)));
}

}  // namespace
}  // namespace network

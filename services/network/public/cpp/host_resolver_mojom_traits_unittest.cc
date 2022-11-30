// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/host_resolver_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_over_https_server_config.h"
#include "services/network/public/cpp/ip_address_mojom_traits.h"
#include "services/network/public/cpp/ip_endpoint_mojom_traits.h"
#include "services/network/public/mojom/host_resolver.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

TEST(HostResolverMojomTraitsTest, DnsConfigOverridesRoundtrip_Empty) {
  net::DnsConfigOverrides original;

  net::DnsConfigOverrides deserialized;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DnsConfigOverrides>(
      original, deserialized));

  EXPECT_EQ(original, deserialized);
}

TEST(HostResolverMojomTraitsTest, DnsConfigOverridesRoundtrip_FullySpecified) {
  net::DnsConfigOverrides original;
  original.nameservers.emplace(
      {net::IPEndPoint(net::IPAddress(1, 2, 3, 4), 80)});
  original.search.emplace({std::string("str")});
  original.append_to_multi_label_name = true;
  original.ndots = 2;
  original.fallback_period = base::Hours(4);
  original.attempts = 1;
  original.rotate = true;
  original.use_local_ipv6 = false;
  original.dns_over_https_config =
      *net::DnsOverHttpsConfig::FromString("https://example.com/");
  original.secure_dns_mode = net::SecureDnsMode::kSecure;
  original.allow_dns_over_https_upgrade = true;
  original.clear_hosts = true;

  net::DnsConfigOverrides deserialized;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DnsConfigOverrides>(
      original, deserialized));

  EXPECT_EQ(original, deserialized);
}

TEST(HostResolverMojomTraitsTest, DnsConfigOverrides_BadInt) {
  mojom::DnsConfigOverridesPtr overrides = mojom::DnsConfigOverrides::New();
  overrides->ndots = -10;

  std::vector<uint8_t> serialized =
      mojom::DnsConfigOverrides::Serialize(&overrides);

  net::DnsConfigOverrides deserialized;
  EXPECT_FALSE(
      mojom::DnsConfigOverrides::Deserialize(serialized, &deserialized));
}

TEST(HostResolverMojomTraitsTest, DnsConfigOverrides_OnlyDnsOverHttpsServers) {
  net::DnsConfigOverrides original;
  original.dns_over_https_config =
      *net::DnsOverHttpsConfig::FromString("https://example.com/");

  net::DnsConfigOverrides deserialized;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DnsConfigOverrides>(
      original, deserialized));

  EXPECT_EQ(original, deserialized);
}

TEST(HostResolverMojomTraitsTest, DnsOverHttpsServerConfig_Roundtrip) {
  net::DnsOverHttpsServerConfig::Endpoints endpoints{
      {{192, 0, 2, 1},
       {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}},
      {{192, 0, 2, 2},
       {0x20, 0x01, 0x0d, 0xb8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2}}};
  auto original = *net::DnsOverHttpsServerConfig::FromString(
      "https://example.com/", endpoints);

  net::DnsOverHttpsServerConfig deserialized;
  EXPECT_TRUE(
      mojo::test::SerializeAndDeserialize<mojom::DnsOverHttpsServerConfig>(
          original, deserialized));

  EXPECT_EQ(original, deserialized);
}

TEST(HostResolverMojomTraitsTest, ResolveErrorInfo) {
  net::ResolveErrorInfo original;
  original.error = net::ERR_NAME_NOT_RESOLVED;

  net::ResolveErrorInfo deserialized;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ResolveErrorInfo>(
      original, deserialized));

  EXPECT_EQ(original, deserialized);
}

}  // namespace
}  // namespace network

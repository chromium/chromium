// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/host_resolver_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "net/base/net_errors.h"
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
      &original, &deserialized));

  EXPECT_EQ(original, deserialized);
}

TEST(HostResolverMojomTraitsTest, DnsConfigOverridesRoundtrip_FullySpecified) {
  net::DnsConfigOverrides original;
  original.nameservers.emplace(
      {net::IPEndPoint(net::IPAddress(1, 2, 3, 4), 80)});
  original.search.emplace({std::string("str")});
  original.append_to_multi_label_name = true;
  original.ndots = 2;
  original.timeout = base::TimeDelta::FromHours(4);
  original.attempts = 1;
  original.rotate = true;
  original.use_local_ipv6 = false;
  original.dns_over_https_servers.emplace(
      {net::DnsOverHttpsServerConfig("example.com", false)});
  original.secure_dns_mode = net::SecureDnsMode::kSecure;
  original.allow_dns_over_https_upgrade = true;
  original.disabled_upgrade_providers.emplace({std::string("provider_name")});
  original.clear_hosts = true;

  net::DnsConfigOverrides deserialized;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DnsConfigOverrides>(
      &original, &deserialized));

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
  original.dns_over_https_servers.emplace(
      {net::DnsOverHttpsServerConfig("example.com", false)});

  net::DnsConfigOverrides deserialized;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::DnsConfigOverrides>(
      &original, &deserialized));

  EXPECT_EQ(original, deserialized);
}

TEST(HostResolverMojomTraitsTest, ResolveErrorInfo) {
  net::ResolveErrorInfo original;
  original.error = net::ERR_NAME_NOT_RESOLVED;

  net::ResolveErrorInfo deserialized;
  EXPECT_TRUE(mojo::test::SerializeAndDeserialize<mojom::ResolveErrorInfo>(
      &original, &deserialized));

  EXPECT_EQ(original, deserialized);
}

}  // namespace
}  // namespace network

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/host_resolver_mojom_traits.h"

#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/network/public/cpp/ip_address_mojom_traits.h"
#include "services/network/public/cpp/ip_endpoint_mojom_traits.h"
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
  original.hosts = net::DnsHosts(
      {std::make_pair(net::DnsHostsKey("host1", net::ADDRESS_FAMILY_IPV4),
                      net::IPAddress(2, 3, 4, 5)),
       std::make_pair(net::DnsHostsKey("host2", net::ADDRESS_FAMILY_IPV4),
                      net::IPAddress(2, 3, 4, 5))});
  original.append_to_multi_label_name = true;
  original.randomize_ports = false;
  original.ndots = 2;
  original.timeout = base::TimeDelta::FromHours(4);
  original.attempts = 1;
  original.rotate = true;
  original.use_local_ipv6 = false;
  original.dns_over_https_servers.emplace(
      {net::DnsConfig::DnsOverHttpsServerConfig("example.com", false)});
  original.secure_dns_mode = net::DnsConfig::SecureDnsMode::SECURE;
  original.allow_dns_over_https_upgrade = true;
  original.disabled_upgrade_providers.emplace({std::string("provider_name")});

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

TEST(HostResolverMojomTraitsTest, DnsConfigOverrides_NonUniqueHostKeys) {
  mojom::DnsConfigOverridesPtr overrides = mojom::DnsConfigOverrides::New();
  overrides->hosts.emplace();

  // Create two different entries that share the key ("host", IPV4).
  mojom::DnsHostPtr host_entry1 = mojom::DnsHost::New();
  host_entry1->hostname = "host";
  host_entry1->address = net::IPAddress(1, 1, 1, 1);
  overrides->hosts.value().push_back(std::move(host_entry1));

  mojom::DnsHostPtr host_entry2 = mojom::DnsHost::New();
  host_entry2->hostname = "host";
  host_entry2->address = net::IPAddress(2, 2, 2, 2);
  overrides->hosts.value().push_back(std::move(host_entry2));

  std::vector<uint8_t> serialized =
      mojom::DnsConfigOverrides::Serialize(&overrides);

  net::DnsConfigOverrides deserialized;
  EXPECT_FALSE(
      mojom::DnsConfigOverrides::Deserialize(serialized, &deserialized));
}

}  // namespace
}  // namespace network

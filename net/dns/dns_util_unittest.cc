// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/dns_util.h"

#include <string_view>

#include "base/test/scoped_feature_list.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_protocol.h"
#include "net/dns/public/doh_provider_entry.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {
// Returns the DoH provider entry in `DohProviderEntry::GetList()` that matches
// `provider`. Crashes if there is no matching entry.
const DohProviderEntry& GetDohProviderEntry(std::string_view provider) {
  auto provider_list = DohProviderEntry::GetList();
  auto it =
      base::ranges::find(provider_list, provider, &DohProviderEntry::provider);
  CHECK(it != provider_list.end());
  return **it;
}
}  // namespace

class DNSUtilTest : public testing::Test {};

TEST_F(DNSUtilTest, GetURLFromTemplateWithoutParameters) {
  EXPECT_EQ("https://dnsserver.example.net/dns-query",
            GetURLFromTemplateWithoutParameters(
                "https://dnsserver.example.net/dns-query{?dns}"));
}

TEST_F(DNSUtilTest, GetDohUpgradeServersFromDotHostname) {
  std::vector<DnsOverHttpsServerConfig> doh_servers =
      GetDohUpgradeServersFromDotHostname("");
  EXPECT_EQ(0u, doh_servers.size());

  doh_servers = GetDohUpgradeServersFromDotHostname("unrecognized");
  EXPECT_EQ(0u, doh_servers.size());

  doh_servers = GetDohUpgradeServersFromDotHostname(
      "family-filter-dns.cleanbrowsing.org");
  EXPECT_EQ(1u, doh_servers.size());
  EXPECT_EQ("https://doh.cleanbrowsing.org/doh/family-filter{?dns}",
            doh_servers[0].server_template());

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{}, /*disabled_features=*/{
          GetDohProviderEntry("CleanBrowsingFamily").feature.get()});
  doh_servers = GetDohUpgradeServersFromDotHostname(
      "family-filter-dns.cleanbrowsing.org");
  EXPECT_EQ(0u, doh_servers.size());
}

TEST_F(DNSUtilTest, GetDohUpgradeServersFromNameservers) {
  std::vector<IPEndPoint> nameservers;
  // Cloudflare upgradeable IPs
  IPAddress dns_ip0(1, 0, 0, 1);
  IPAddress dns_ip1;
  EXPECT_TRUE(dns_ip1.AssignFromIPLiteral("2606:4700:4700::1111"));
  // SafeBrowsing family filter upgradeable IP
  IPAddress dns_ip2;
  EXPECT_TRUE(dns_ip2.AssignFromIPLiteral("2a0d:2a00:2::"));
  // SafeBrowsing security filter upgradeable IP
  IPAddress dns_ip3(185, 228, 169, 9);
  // None-upgradeable IP
  IPAddress dns_ip4(1, 2, 3, 4);

  nameservers.emplace_back(dns_ip0, dns_protocol::kDefaultPort);
  nameservers.emplace_back(dns_ip1, dns_protocol::kDefaultPort);
  nameservers.emplace_back(dns_ip2, 54);
  nameservers.emplace_back(dns_ip3, dns_protocol::kDefaultPort);
  nameservers.emplace_back(dns_ip4, dns_protocol::kDefaultPort);

  std::vector<DnsOverHttpsServerConfig> doh_servers =
      GetDohUpgradeServersFromNameservers(std::vector<IPEndPoint>());
  EXPECT_EQ(0u, doh_servers.size());

  doh_servers = GetDohUpgradeServersFromNameservers(nameservers);
  auto expected_config = *DnsOverHttpsConfig::FromTemplatesForTesting(
      {"https://chrome.cloudflare-dns.com/dns-query",
       "https://doh.cleanbrowsing.org/doh/family-filter{?dns}",
       "https://doh.cleanbrowsing.org/doh/security-filter{?dns}"});
  EXPECT_EQ(expected_config.servers(), doh_servers);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{
          GetDohProviderEntry("CleanBrowsingSecure").feature.get(),
          GetDohProviderEntry("Cloudflare").feature.get()});

  doh_servers = GetDohUpgradeServersFromNameservers(nameservers);
  EXPECT_THAT(doh_servers,
              testing::ElementsAre(*DnsOverHttpsServerConfig::FromString(
                  "https://doh.cleanbrowsing.org/doh/family-filter{?dns}")));
}

TEST_F(DNSUtilTest, GetDohProviderIdForHistogramFromServerConfig) {
  EXPECT_EQ("Cloudflare",
            GetDohProviderIdForHistogramFromServerConfig(
                *DnsOverHttpsServerConfig::FromString(
                    "https://chrome.cloudflare-dns.com/dns-query")));
  EXPECT_EQ("Other", GetDohProviderIdForHistogramFromServerConfig(
                         *DnsOverHttpsServerConfig::FromString(
                             "https://unexpected.dohserver.com/dns-query")));
}

TEST_F(DNSUtilTest, GetDohProviderIdForHistogramFromNameserver) {
  EXPECT_EQ("CleanBrowsingSecure",
            GetDohProviderIdForHistogramFromNameserver(IPEndPoint(
                IPAddress(185, 228, 169, 9), dns_protocol::kDefaultPort)));
  EXPECT_EQ("Other", GetDohProviderIdForHistogramFromNameserver(IPEndPoint(
                         IPAddress(1, 2, 3, 4), dns_protocol::kDefaultPort)));
}

}  // namespace net

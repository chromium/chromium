// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/masked_domain_list/network_service_proxy_allow_list.h"

#include "base/test/scoped_feature_list.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/proxy_config.mojom-shared.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {
using masked_domain_list::MaskedDomainList;

struct MatchTest {
  std::string name;
  std::string req;
  std::string top;
  bool matches;
};

}  // namespace

class NetworkServiceProxyAllowListTest : public ::testing::Test {};

TEST_F(NetworkServiceProxyAllowListTest, IsNotEnabledByDefault) {
  NetworkServiceProxyAllowList allow_list_no_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  NetworkServiceProxyAllowList allow_list_first_party_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::
          kFirstPartyToTopLevelFrame);
  EXPECT_FALSE(allow_list_no_bypass.IsEnabled());
  EXPECT_FALSE(allow_list_first_party_bypass.IsEnabled());
}

TEST_F(NetworkServiceProxyAllowListTest, IsEnabledWhenManuallySet) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({net::features::kEnableIpProtectionProxy,
                                        network::features::kMaskedDomainList},
                                       {});

  NetworkServiceProxyAllowList allow_list(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);

  EXPECT_TRUE(allow_list.IsEnabled());
  EXPECT_TRUE(allow_list.MakeIpProtectionCustomProxyConfig()
                  ->rules.restrict_to_network_service_proxy_allow_list);
}

TEST_F(NetworkServiceProxyAllowListTest, AllowListIsNotPopulatedByDefault) {
  NetworkServiceProxyAllowList allow_list(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  EXPECT_FALSE(allow_list.IsPopulated());
}

TEST_F(NetworkServiceProxyAllowListTest, AllowlistIsPopulatedWhenMDLUsed) {
  NetworkServiceProxyAllowList allow_list(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  MaskedDomainList mdl;
  auto* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("foo");
  resource_owner->add_owned_resources()->set_domain("example.com");
  allow_list.UseMaskedDomainList(mdl);

  EXPECT_TRUE(allow_list.IsPopulated());
}

TEST_F(NetworkServiceProxyAllowListTest, ShouldntMatchHttp) {
  NetworkServiceProxyAllowList allow_list_no_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  NetworkServiceProxyAllowList allow_list_first_party_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::
          kFirstPartyToTopLevelFrame);
  MaskedDomainList mdl;
  auto* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("foo");
  resource_owner->add_owned_resources()->set_domain("example.com");
  allow_list_no_bypass.UseMaskedDomainList(mdl);
  allow_list_first_party_bypass.UseMaskedDomainList(mdl);

  EXPECT_FALSE(allow_list_no_bypass.Matches(
      GURL("http://example.com"),
      net::NetworkAnonymizationKey::CreateCrossSite(
          net::SchemefulSite(GURL("http://top.com")))));
  EXPECT_FALSE(allow_list_first_party_bypass.Matches(
      GURL("http://example.com"),
      net::NetworkAnonymizationKey::CreateCrossSite(
          net::SchemefulSite(GURL("http://top.com")))));
}

TEST_F(NetworkServiceProxyAllowListTest, ShouldMatchThirdPartyToTopLevelFrame) {
  NetworkServiceProxyAllowList allow_list_no_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  NetworkServiceProxyAllowList allow_list_first_party_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::
          kFirstPartyToTopLevelFrame);
  MaskedDomainList mdl;
  auto* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("foo");
  resource_owner->add_owned_resources()->set_domain("example.com");
  allow_list_no_bypass.UseMaskedDomainList(mdl);
  allow_list_first_party_bypass.UseMaskedDomainList(mdl);

  EXPECT_TRUE(allow_list_no_bypass.Matches(
      GURL("https://example.com"),
      net::NetworkAnonymizationKey::CreateCrossSite(
          net::SchemefulSite(GURL("https://top.com")))));
  EXPECT_TRUE(allow_list_first_party_bypass.Matches(
      GURL("https://example.com"),
      net::NetworkAnonymizationKey::CreateCrossSite(
          net::SchemefulSite(GURL("https://top.com")))));
}

TEST_F(NetworkServiceProxyAllowListTest,
       MatchFirstPartyToTopLevelFrameDependsOnBypass) {
  NetworkServiceProxyAllowList allow_list_no_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  NetworkServiceProxyAllowList allow_list_first_party_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::
          kFirstPartyToTopLevelFrame);
  MaskedDomainList mdl;
  auto* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("foo");
  resource_owner->add_owned_resources()->set_domain("example.com");
  allow_list_no_bypass.UseMaskedDomainList(mdl);
  allow_list_first_party_bypass.UseMaskedDomainList(mdl);

  EXPECT_TRUE(allow_list_no_bypass.Matches(
      GURL("https://example.com"),
      net::NetworkAnonymizationKey::CreateCrossSite(
          net::SchemefulSite(GURL("https://example.com")))));
  EXPECT_FALSE(allow_list_first_party_bypass.Matches(
      GURL("https://example.com"),
      net::NetworkAnonymizationKey::CreateCrossSite(
          net::SchemefulSite(GURL("https://example.com")))));
}

TEST_F(NetworkServiceProxyAllowListTest,
       MatchFirstPartyToTopLevelFrameIfEmptyNakDependsOnBypass) {
  NetworkServiceProxyAllowList allow_list_no_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  NetworkServiceProxyAllowList allow_list_first_party_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::
          kFirstPartyToTopLevelFrame);
  MaskedDomainList mdl;
  auto* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("foo");
  resource_owner->add_owned_resources()->set_domain("example.com");
  allow_list_no_bypass.UseMaskedDomainList(mdl);
  allow_list_first_party_bypass.UseMaskedDomainList(mdl);

  EXPECT_TRUE(allow_list_no_bypass.Matches(GURL("https://example.com"),
                                           net::NetworkAnonymizationKey()));
  EXPECT_FALSE(allow_list_first_party_bypass.Matches(
      GURL("https://example.com"), net::NetworkAnonymizationKey()));
}

TEST_F(NetworkServiceProxyAllowListTest,
       ShouldNotMatchWithTransientNakIfUrlDoesNotMatch) {
  NetworkServiceProxyAllowList allow_list_no_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  NetworkServiceProxyAllowList allow_list_first_party_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::
          kFirstPartyToTopLevelFrame);
  MaskedDomainList mdl;
  auto* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("foo");
  resource_owner->add_owned_resources()->set_domain("example.com");
  allow_list_no_bypass.UseMaskedDomainList(mdl);
  allow_list_first_party_bypass.UseMaskedDomainList(mdl);

  EXPECT_FALSE(allow_list_no_bypass.Matches(
      GURL("https://other.com"),
      net::NetworkAnonymizationKey::CreateTransient()));
  EXPECT_FALSE(allow_list_first_party_bypass.Matches(
      GURL("https://other.com"),
      net::NetworkAnonymizationKey::CreateTransient()));
}

TEST_F(NetworkServiceProxyAllowListTest,
       ShouldMatchWithTransientNakIfUrlMatches) {
  NetworkServiceProxyAllowList allow_list_no_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::kNone);
  NetworkServiceProxyAllowList allow_list_first_party_bypass(
      network::mojom::IpProtectionProxyBypassPolicy::
          kFirstPartyToTopLevelFrame);
  MaskedDomainList mdl;
  auto* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("foo");
  resource_owner->add_owned_resources()->set_domain("example.com");
  allow_list_no_bypass.UseMaskedDomainList(mdl);
  allow_list_first_party_bypass.UseMaskedDomainList(mdl);

  EXPECT_TRUE(allow_list_no_bypass.Matches(
      GURL("https://example.com"),
      net::NetworkAnonymizationKey::CreateTransient()));
  EXPECT_TRUE(allow_list_first_party_bypass.Matches(
      GURL("https://example.com"),
      net::NetworkAnonymizationKey::CreateTransient()));
}
}  // namespace network

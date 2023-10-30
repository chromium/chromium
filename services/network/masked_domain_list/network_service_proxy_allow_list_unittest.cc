// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/masked_domain_list/network_service_proxy_allow_list.h"

#include "base/test/scoped_feature_list.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/features.h"
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

TEST_F(NetworkServiceProxyAllowListTest, NotEnabled) {
  NetworkServiceProxyAllowList allow_list;
  EXPECT_FALSE(allow_list.IsEnabled());
}

TEST_F(NetworkServiceProxyAllowListTest, IsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({net::features::kEnableIpProtectionProxy,
                                        network::features::kMaskedDomainList},
                                       {});

  NetworkServiceProxyAllowList allow_list;

  EXPECT_TRUE(allow_list.IsEnabled());
  EXPECT_TRUE(allow_list.MakeIpProtectionCustomProxyConfig()
                  ->rules.restrict_to_network_service_proxy_allow_list);
}

TEST_F(NetworkServiceProxyAllowListTest, IsPopulated) {
  NetworkServiceProxyAllowList allow_list;
  MaskedDomainList mdl;
  auto* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("foo");
  resource_owner->add_owned_resources()->set_domain("example.com");
  allow_list.UseMaskedDomainList(mdl);

  EXPECT_TRUE(allow_list.IsPopulated());
}

TEST_F(NetworkServiceProxyAllowListTest, IsPopulated_Empty) {
  NetworkServiceProxyAllowList allow_list;
  EXPECT_FALSE(allow_list.IsPopulated());
}

TEST_F(NetworkServiceProxyAllowListTest, Matches) {
  NetworkServiceProxyAllowList allow_list;
  MaskedDomainList mdl;
  auto* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("foo");
  resource_owner->add_owned_resources()->set_domain("example.com");
  allow_list.UseMaskedDomainList(mdl);

  EXPECT_TRUE(
      allow_list.Matches(GURL("http://example.com"),
                         net::NetworkAnonymizationKey::CreateCrossSite(
                             net::SchemefulSite(GURL("http://top.com")))));
}

TEST_F(NetworkServiceProxyAllowListTest, Matches_EmptyNak) {
  NetworkServiceProxyAllowList allow_list;
  MaskedDomainList mdl;
  auto* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("foo");
  resource_owner->add_owned_resources()->set_domain("example.com");
  allow_list.UseMaskedDomainList(mdl);

  EXPECT_FALSE(allow_list.Matches(GURL("http://example.com"),
                                  net::NetworkAnonymizationKey()));
}

TEST_F(NetworkServiceProxyAllowListTest, Matches_TransientNak) {
  NetworkServiceProxyAllowList allow_list;
  MaskedDomainList mdl;
  auto* resource_owner = mdl.add_resource_owners();
  resource_owner->set_owner_name("foo");
  resource_owner->add_owned_resources()->set_domain("example.com");
  allow_list.UseMaskedDomainList(mdl);

  EXPECT_FALSE(
      allow_list.Matches(GURL("http://example.com"),
                         net::NetworkAnonymizationKey::CreateTransient()));
}

}  // namespace network

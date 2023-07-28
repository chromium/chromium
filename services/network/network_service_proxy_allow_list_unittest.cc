// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_proxy_allow_list.h"

#include "base/test/scoped_feature_list.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {
using masked_domain_list::MaskedDomainList;
}  // namespace

class NetworkServiceProxyAllowListTest : public ::testing::Test {};

TEST_F(NetworkServiceProxyAllowListTest, NotEnabled) {
  NetworkServiceProxyAllowList allowList;
  EXPECT_FALSE(allowList.IsEnabled());
}

TEST_F(NetworkServiceProxyAllowListTest, IsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {net::features::kEnableIpProtectionProxy,
       network::features::kMaskedDomainList},
      {});

  NetworkServiceProxyAllowList allowList;

  EXPECT_TRUE(allowList.IsEnabled());
  EXPECT_TRUE(allowList.GetCustomProxyConfig()
                  ->rules.restrict_to_network_service_proxy_allow_list);
}

TEST_F(NetworkServiceProxyAllowListTest, IsPopulated) {
  NetworkServiceProxyAllowList allowList;
  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  resourceOwner->add_owned_resources()->set_domain("example.com");
  allowList.UseMaskedDomainList(mdl);

  EXPECT_TRUE(allowList.IsPopulated());
}

TEST_F(NetworkServiceProxyAllowListTest, IsPopulated_Empty) {
  NetworkServiceProxyAllowList allowList;
  EXPECT_FALSE(allowList.IsPopulated());
}

TEST_F(NetworkServiceProxyAllowListTest, Matches_AllowListNotPopulated) {
  NetworkServiceProxyAllowList allowList;

  EXPECT_FALSE(allowList.IsPopulated());
  EXPECT_FALSE(allowList.Matches(GURL("http://example.com"),
                                 GURL("http://example2.com")));
}

TEST_F(NetworkServiceProxyAllowListTest, Matches_TopFrameIsMatched) {
  NetworkServiceProxyAllowList allowList;

  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  resourceOwner->add_owned_resources()->set_domain("example.com");
  resourceOwner->add_owned_properties("example2.com");

  allowList.UseMaskedDomainList(mdl);

  EXPECT_FALSE(allowList.Matches(GURL("http://example.com"),
                                 GURL("http://example2.com")));
}

TEST_F(NetworkServiceProxyAllowListTest, Matches_TopFrameIsNotMatched) {
  NetworkServiceProxyAllowList allowList;

  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  resourceOwner->add_owned_resources()->set_domain("example.com");
  resourceOwner->add_owned_properties("example2.com");

  allowList.UseMaskedDomainList(mdl);

  EXPECT_TRUE(allowList.Matches(GURL("http://example.com"),
                                GURL("http://example3.com")));
}

TEST_F(NetworkServiceProxyAllowListTest, Matches_TopFrameUrlIsEmpty) {
  NetworkServiceProxyAllowList allowList;

  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  resourceOwner->add_owned_resources()->set_domain("example.com");
  resourceOwner->add_owned_properties("example2.com");

  allowList.UseMaskedDomainList(mdl);

  EXPECT_FALSE(allowList.Matches(GURL("http://example.com"), GURL()));
}

TEST_F(NetworkServiceProxyAllowListTest, Matches_RequestSameAsTopFrame) {
  NetworkServiceProxyAllowList allowList;

  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  resourceOwner->add_owned_resources()->set_domain("example.com");
  resourceOwner->add_owned_properties("example2.com");

  allowList.UseMaskedDomainList(mdl);

  EXPECT_FALSE(allowList.Matches(GURL("http://example.com"),
                                 GURL("http://example.com")));
}

TEST_F(NetworkServiceProxyAllowListTest, Matches_RequestNotInAllowList) {
  NetworkServiceProxyAllowList allowList;

  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  resourceOwner->add_owned_resources()->set_domain("example.com");
  resourceOwner->add_owned_properties("example2.com");

  allowList.UseMaskedDomainList(mdl);

  EXPECT_FALSE(allowList.Matches(GURL("http://example3.com"),
                                 GURL("http://example.com")));
}

TEST_F(NetworkServiceProxyAllowListTest, Matches_SubResource) {
  NetworkServiceProxyAllowList allowList;

  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  resourceOwner->add_owned_resources()->set_domain("example.com");
  resourceOwner->add_owned_properties("example2.com");

  allowList.UseMaskedDomainList(mdl);

  EXPECT_TRUE(allowList.Matches(GURL("http://sub.example.com"),
                                GURL("http://example3.com")));
}

TEST_F(NetworkServiceProxyAllowListTest, Matches_LongSubdomain) {
  NetworkServiceProxyAllowList allowList;

  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  resourceOwner->add_owned_resources()->set_domain("example.com");

  allowList.UseMaskedDomainList(mdl);

  EXPECT_TRUE(
      allowList.Matches(GURL("http://a.very.nested.subdomain.example.com"),
                        GURL("http://top.com")));
}

}  // namespace network

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/network_service_proxy_allow_list.h"

#include "base/test/scoped_feature_list.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

namespace {
using masked_domain_list::MaskedDomainList;
}  // namespace

class NetworkServiceProxyAllowlistTest : public ::testing::Test {};

TEST_F(NetworkServiceProxyAllowlistTest, NotEnabled) {
  NetworkServiceProxyAllowList allowList;
  EXPECT_FALSE(allowList.IsEnabled());
}

TEST_F(NetworkServiceProxyAllowlistTest, NotEnabled_NoCustomProxyConfigExists) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  NetworkServiceProxyAllowList allowList;
  EXPECT_FALSE(allowList.IsEnabled());
}

TEST_F(NetworkServiceProxyAllowlistTest, IsEnabled_ByFeatureAndConfigPresence) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  resourceOwner->add_owned_resources()->set_domain("example.com");

  NetworkServiceProxyAllowList allowList;
  allowList.UseMaskedDomainList(mdl);

  EXPECT_TRUE(allowList.IsEnabled());
  EXPECT_TRUE(allowList.GetCustomProxyConfig()
                  ->rules.restrict_to_network_service_proxy_allow_list);
}

TEST_F(NetworkServiceProxyAllowlistTest,
       UseMaskedDomainList_MdlHasNoResources) {
  NetworkServiceProxyAllowList allowList;
  MaskedDomainList mdl;
  allowList.UseMaskedDomainList(mdl);

  EXPECT_FALSE(allowList.IsEnabled());
}

TEST_F(NetworkServiceProxyAllowlistTest, Matches_TopFrameIsMatched) {
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

TEST_F(NetworkServiceProxyAllowlistTest, Matches_TopFrameIsNotMatched) {
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

TEST_F(NetworkServiceProxyAllowlistTest, Matches_TopFrameUrlIsEmpty) {
  NetworkServiceProxyAllowList allowList;

  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  resourceOwner->add_owned_resources()->set_domain("example.com");
  resourceOwner->add_owned_properties("example2.com");

  allowList.UseMaskedDomainList(mdl);

  EXPECT_FALSE(allowList.Matches(GURL("http://example.com"), GURL()));
}

TEST_F(NetworkServiceProxyAllowlistTest, Matches_RequestSameAsTopFrame) {
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

TEST_F(NetworkServiceProxyAllowlistTest, Matches_RequestNotInAllowList) {
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

}  // namespace network

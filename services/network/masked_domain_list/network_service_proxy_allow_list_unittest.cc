// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/masked_domain_list/network_service_proxy_allow_list.h"

#include "base/test/scoped_feature_list.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
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
  NetworkServiceProxyAllowList allowList;
  EXPECT_FALSE(allowList.IsEnabled());
}

TEST_F(NetworkServiceProxyAllowListTest, IsEnabled) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitWithFeatures({net::features::kEnableIpProtectionProxy,
                                        network::features::kMaskedDomainList},
                                       {});

  NetworkServiceProxyAllowList allowList;

  EXPECT_TRUE(allowList.IsEnabled());
  EXPECT_TRUE(allowList.MakeIpProtectionCustomProxyConfig()
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

}  // namespace network

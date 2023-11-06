// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/masked_domain_list/network_service_resource_block_list.h"

#include "base/test/scoped_feature_list.h"
#include "components/privacy_sandbox/masked_domain_list/masked_domain_list.pb.h"
#include "net/base/isolation_info.h"
#include "services/network/public/cpp/features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace {
using masked_domain_list::MaskedDomainList;

absl::optional<net::IsolationInfo> CreateIsolationInfo(
    const std::string& top_frame_domain) {
  return net::IsolationInfo::CreateForInternalRequest(
      url::Origin::Create(GURL(top_frame_domain)));
}

}  // namespace

class NetworkServiceResourceBlockListTest : public ::testing::Test {};

TEST_F(NetworkServiceResourceBlockListTest, Matches_IsNotPartOfExperiment) {
  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  auto* resource = resourceOwner->add_owned_resources();
  resource->set_domain("example.com");

  NetworkServiceResourceBlockList blockList;
  blockList.UseMaskedDomainList(mdl);

  EXPECT_FALSE(blockList.Matches(GURL("http://example.com"),
                                 CreateIsolationInfo("http://top.com")));
}

TEST_F(NetworkServiceResourceBlockListTest, Matches_ResourceIsInExperiment) {
  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  auto* resource = resourceOwner->add_owned_resources();
  resource->set_domain("example.com");
  resource->add_experiments(
      masked_domain_list::Resource_Experiment_EXPERIMENT_AFP);

  NetworkServiceResourceBlockList blockList;
  blockList.UseMaskedDomainList(mdl);

  EXPECT_TRUE(blockList.Matches(GURL("http://example.com"),
                                CreateIsolationInfo("http://top.com")));
}

TEST_F(NetworkServiceResourceBlockListTest, Matches_SkipBypassWithOpaqueSite) {
  MaskedDomainList mdl;
  auto* resourceOwner = mdl.add_resource_owners();
  resourceOwner->set_owner_name("foo");
  auto* resource = resourceOwner->add_owned_resources();
  resource->set_domain("example.com");
  resource->add_experiments(
      masked_domain_list::Resource_Experiment_EXPERIMENT_AFP);

  NetworkServiceResourceBlockList blockList;
  blockList.UseMaskedDomainList(mdl);

  EXPECT_TRUE(blockList.Matches(GURL("http://example.com"),
                                net::IsolationInfo::CreateTransient()));
}

}  // namespace network

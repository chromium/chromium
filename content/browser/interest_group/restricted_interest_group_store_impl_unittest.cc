// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/restricted_interest_group_store_impl.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/interest_group/interest_group_manager.h"
#include "content/browser/storage_partition_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_renderer_host.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/test/test_content_browser_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "third_party/blink/public/mojom/interest_group/restricted_interest_group_store.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

const char kInterestGroupName[] = "interest-group-name";

class AllowInterestGroupContentBrowserClient : public TestContentBrowserClient {
 public:
  explicit AllowInterestGroupContentBrowserClient() = default;
  ~AllowInterestGroupContentBrowserClient() override = default;

  AllowInterestGroupContentBrowserClient(
      const AllowInterestGroupContentBrowserClient&) = delete;
  AllowInterestGroupContentBrowserClient& operator=(
      const AllowInterestGroupContentBrowserClient&) = delete;

  // ContentBrowserClient overrides:
  bool IsInterestGroupAPIAllowed(content::BrowserContext* browser_context,
                                 const url::Origin& top_frame_origin,
                                 const GURL& api_url) override {
    return top_frame_origin.host() == "a.test" ||
           top_frame_origin.host() == "b.test";
  }
};

}  // namespace

class RestrictedInterestGroupStoreImplTest : public RenderViewHostTestHarness {
 public:
  RestrictedInterestGroupStoreImplTest() {
    feature_list_.InitAndEnableFeature(blink::features::kFledgeInterestGroups);
    old_content_browser_client_ =
        SetBrowserClientForTesting(&content_browser_client_);
  }

  ~RestrictedInterestGroupStoreImplTest() override {
    SetBrowserClientForTesting(old_content_browser_client_);
  }

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    NavigateAndCommit(kUrlA);

    manager_ = (static_cast<StoragePartitionImpl*>(
                    browser_context()->GetDefaultStoragePartition()))
                   ->GetInterestGroupManager();
  }

  std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
  GetInterestGroupsForOwner(const url::Origin& owner) {
    std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
        interest_groups;
    base::RunLoop run_loop;
    manager_->GetInterestGroupsForOwner(
        owner,
        base::BindLambdaForTesting(
            [&run_loop, &interest_groups](
                std::vector<auction_worklet::mojom::BiddingInterestGroupPtr>
                    groups) {
              interest_groups = std::move(groups);
              run_loop.Quit();
            }));
    run_loop.Run();
    return interest_groups;
  }

  int GetJoinCount(const url::Origin& owner, const std::string& name) {
    for (const auto& interest_group : GetInterestGroupsForOwner(owner)) {
      if (interest_group->group->name == name) {
        return interest_group->signals->join_count;
      }
    }
    return 0;
  }

  // Create a new RestrictedInterestGroupStoreImpl and use it to try and join
  // `interest_group`. Flushes the Mojo pipe to force the Mojo message to be
  // handled before returning.
  //
  // Creates a new RestrictedInterestGroupStoreImpl with each call so the RFH
  // can be navigated between different sites. And
  // RestrictedInterestGroupStoreImpl only handles one site (cross site navs use
  // different RestrictedInterestGroupStoreImpls, and generally use different
  // RFHs as well).
  void JoinInterestGroupAndFlush(
      blink::mojom::InterestGroupPtr interest_group) {
    mojo::Remote<blink::mojom::RestrictedInterestGroupStore> interest_service;
    RestrictedInterestGroupStoreImpl::CreateMojoService(
        web_contents()->GetMainFrame(),
        interest_service.BindNewPipeAndPassReceiver());

    interest_service->JoinInterestGroup(std::move(interest_group));
    interest_service.FlushForTesting();
  }

  // Analogous to JoinInterestGroupAndFlush(), but leaves an interest group
  // instead of joining one.
  void LeaveInterestGroupAndFlush(const url::Origin& owner,
                                  const std::string& name) {
    mojo::Remote<blink::mojom::RestrictedInterestGroupStore> interest_service;
    RestrictedInterestGroupStoreImpl::CreateMojoService(
        web_contents()->GetMainFrame(),
        interest_service.BindNewPipeAndPassReceiver());

    interest_service->LeaveInterestGroup(owner, name);
    interest_service.FlushForTesting();
  }

  // Helper to create a valid interest group with only an origin and name. All
  // URLs are nullopt.
  blink::mojom::InterestGroupPtr CreateInterestGroup() {
    auto interest_group = blink::mojom::InterestGroup::New();
    interest_group->expiry =
        base::Time::Now() + base::TimeDelta::FromSeconds(300);
    interest_group->name = kInterestGroupName;
    interest_group->owner = kOriginA;
    return interest_group;
  }

 protected:
  const GURL kUrlA = GURL("https://a.test/");
  const url::Origin kOriginA = url::Origin::Create(kUrlA);
  const GURL kUrlB = GURL("https://b.test/");
  const url::Origin kOriginB = url::Origin::Create(kUrlB);

  base::test::ScopedFeatureList feature_list_;

  AllowInterestGroupContentBrowserClient content_browser_client_;
  ContentBrowserClient* old_content_browser_client_ = nullptr;
  InterestGroupManager* manager_;
};

// Check basic success case.
TEST_F(RestrictedInterestGroupStoreImplTest, JoinInterestGroupBasic) {
  blink::mojom::InterestGroupPtr interest_group = CreateInterestGroup();
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // Several tests assume interest group API are also allowed on kOriginB, so
  // make sure that's enabled correctly.
  NavigateAndCommit(kUrlB);
  interest_group = CreateInterestGroup();
  interest_group->owner = kOriginB;
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(1, GetJoinCount(kOriginB, kInterestGroupName));
}

// Non-HTTPS interest groups should be rejected.
TEST_F(RestrictedInterestGroupStoreImplTest, JoinInterestGroupOriginNotHttps) {
  // Note that the ContentBrowserClient allows URLs based on hosts, not origins,
  // so it should not block this URL. Instead, it should run into the HTTPS
  // check.
  const GURL kHttpUrlA = GURL("http://a.test/");
  const url::Origin kHttpOriginA = url::Origin::Create(kHttpUrlA);
  NavigateAndCommit(kHttpUrlA);
  blink::mojom::InterestGroupPtr interest_group = CreateInterestGroup();
  interest_group->owner = kHttpOriginA;
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(0, GetJoinCount(kHttpOriginA, kInterestGroupName));
}

// Test one origin trying to add an interest group for another.
TEST_F(RestrictedInterestGroupStoreImplTest,
       JoinInterestGroupWrongOwnerOrigin) {
  blink::mojom::InterestGroupPtr interest_group = CreateInterestGroup();
  interest_group->owner = kOriginB;
  JoinInterestGroupAndFlush(std::move(interest_group));
  // Interest group should not be added for either origin.
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
  EXPECT_EQ(0, GetJoinCount(kOriginB, kInterestGroupName));
}

// Test joining an interest group with a disallowed cross-origin URL. Doesn't
// exhaustively test all cases, as the validation function has its own unit
// tests. This is just to make sure those are hooked up.
//
// TODO(mmenke): Once ReportBadMessage is called in these cases, make sure Mojo
// pipe is closed as well.
TEST_F(RestrictedInterestGroupStoreImplTest, JoinInterestGroupCrossSiteUrls) {
  const GURL kBadUrl = GURL("https://user:pass@a.test/");

  // Test `bidding_url`.
  blink::mojom::InterestGroupPtr interest_group = CreateInterestGroup();
  interest_group->bidding_url = kBadUrl;
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));

  // Test `update_url`.
  interest_group = CreateInterestGroup();
  interest_group->update_url = kBadUrl;
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));

  // Test `trusted_bidding_signals_url`.
  interest_group = CreateInterestGroup();
  interest_group->trusted_bidding_signals_url = kBadUrl;
  JoinInterestGroupAndFlush(std::move(interest_group));
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
}

// Check that cross-origin leave interest group operations don't work.
TEST_F(RestrictedInterestGroupStoreImplTest,
       LeaveInterestGroupWrongOwnerOrigin) {
  // https://a.test/ joins an interest group.
  JoinInterestGroupAndFlush(CreateInterestGroup());
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // https://b.test/ cannot leave https://a.test/'s interest group.
  NavigateAndCommit(kUrlB);
  LeaveInterestGroupAndFlush(kOriginA, kInterestGroupName);
  EXPECT_EQ(1, GetJoinCount(kOriginA, kInterestGroupName));

  // https://a.test/ can leave its own the interest group.
  NavigateAndCommit(GURL("https://a.test/"));
  LeaveInterestGroupAndFlush(kOriginA, kInterestGroupName);
  EXPECT_EQ(0, GetJoinCount(kOriginA, kInterestGroupName));
}

}  // namespace content

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_manager_impl.h"

#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/test_interest_group_observer.h"
#include "content/public/browser/k_anonymity_service_delegate.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/origin.h"

namespace content {

class InterestGroupManagerImplTestPeer {
 public:
  explicit InterestGroupManagerImplTestPeer(
      InterestGroupManagerImpl* interest_group_manager)
      : interest_group_manager_(interest_group_manager) {}

  void UpdateInterestGroup(blink::InterestGroupKey group_key,
                           InterestGroupUpdate update) {
    base::test::TestFuture<bool> update_complete_signal;
    interest_group_manager_->UpdateInterestGroup(
        group_key, std::move(update), update_complete_signal.GetCallback());
    EXPECT_TRUE(update_complete_signal.Wait());
  }

  raw_ptr<InterestGroupManagerImpl> interest_group_manager_;
};

namespace {

class TestKAnonymityServiceDelegate : public KAnonymityServiceDelegate {
 public:
  TestKAnonymityServiceDelegate() = default;

  void JoinSet(std::string id,
               base::OnceCallback<void(bool)> callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  }

  void QuerySets(
      std::vector<std::string> ids,
      base::OnceCallback<void(std::vector<bool>)> callback) override {
    size_t ids_size = ids.size();
    std::move(ids.begin(), ids.end(), std::back_inserter(queried_ids_));

    // Return that nothing is k-anonymous.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::vector<bool>(ids_size, false)));
  }

  base::TimeDelta GetJoinInterval() override { return base::Seconds(1); }
  base::TimeDelta GetQueryInterval() override { return base::Seconds(1); }

  const std::vector<std::string>& queried_ids() const { return queried_ids_; }

 private:
  std::vector<std::string> queried_ids_;
};

class InterestGroupManagerImplTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    interest_group_manager_ = std::make_unique<InterestGroupManagerImpl>(
        temp_directory_.GetPath(), /*in_memory=*/true,
        InterestGroupManagerImpl::ProcessMode::kInRenderer,
        test_url_loader_factory_.GetSafeWeakWrapper(),
        base::BindRepeating(&InterestGroupManagerImplTest::GetKAnonDelegate,
                            base::Unretained(this)));
  }

  std::vector<url::Origin> GetAllInterestGroupOwners() {
    base::test::TestFuture<std::vector<url::Origin>> result;
    interest_group_manager_->GetAllInterestGroupOwners(result.GetCallback());
    return result.Get();
  }

  scoped_refptr<StorageInterestGroups> GetInterestGroupsForOwner(
      const url::Origin& owner) {
    base::test::TestFuture<scoped_refptr<StorageInterestGroups>> result;
    interest_group_manager_->GetInterestGroupsForOwner(
        /*devtools_auction_id=*/std::nullopt, owner, result.GetCallback());
    return result.Get();
  }

  SingleStorageInterestGroup GetSingleInterestGroup(url::Origin test_origin) {
    std::vector<url::Origin> origins = GetAllInterestGroupOwners();
    EXPECT_EQ(1u, origins.size());
    EXPECT_EQ(test_origin, origins[0]);
    scoped_refptr<StorageInterestGroups> interest_groups =
        GetInterestGroupsForOwner(test_origin);
    CHECK_EQ(1u, interest_groups->size());
    return std::move(interest_groups->GetInterestGroups()[0]);
  }

  KAnonymityServiceDelegate* GetKAnonDelegate() { return &k_anon_delegate_; }

  base::ScopedTempDir temp_directory_;

  // This uses MOCK_TIME so that we can declare expectations on join_time and
  // last_updated below.
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  network::TestURLLoaderFactory test_url_loader_factory_;
  TestKAnonymityServiceDelegate k_anon_delegate_;

  std::unique_ptr<InterestGroupManagerImpl> interest_group_manager_;
};

blink::InterestGroup NewInterestGroup(url::Origin owner, std::string name) {
  blink::InterestGroup result;
  result.owner = owner;
  result.name = name;
  result.bidding_url = owner.GetURL().Resolve("/bidding_script.js");
  result.update_url = owner.GetURL().Resolve("/update_script.js");
  result.expiry = base::Time::Now() + base::Days(30);
  result.execution_mode =
      blink::InterestGroup::ExecutionMode::kCompatibilityMode;
  return result;
}

TEST_F(InterestGroupManagerImplTest, JoinInterestGroupWithNoAds) {
  base::HistogramTester histograms;
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  blink::InterestGroup test_group = NewInterestGroup(test_origin, "example");

  TestInterestGroupObserver interest_group_observer;
  interest_group_manager_->AddInterestGroupObserver(&interest_group_observer);

  interest_group_manager_->JoinInterestGroup(test_group, test_origin.GetURL());
  interest_group_observer.WaitForAccesses(
      {{/*devtools_auction_id=*/"global",
        InterestGroupManagerImpl::InterestGroupObserver::AccessType::kJoin,
        /*owner_origin=*/test_origin, /*ig_name=*/"example",
        /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt,
        /*component_seller_origin=*/std::nullopt}});

  EXPECT_THAT(k_anon_delegate_.queried_ids(), testing::ElementsAre());

  SingleStorageInterestGroup loaded_group = GetSingleInterestGroup(test_origin);

  EXPECT_EQ(test_origin, loaded_group->interest_group.owner);
  EXPECT_EQ("example", loaded_group->interest_group.name);
  EXPECT_EQ(1, loaded_group->bidding_browser_signals->join_count);
  EXPECT_EQ(0, loaded_group->bidding_browser_signals->bid_count);
  EXPECT_EQ(test_origin, loaded_group->joining_origin);
  EXPECT_EQ(base::Time::Now(), loaded_group->join_time);
  EXPECT_EQ(base::Time::Now(), loaded_group->last_updated);
  EXPECT_EQ(base::Time::Min(), loaded_group->next_update_after);

  histograms.ExpectTotalCount(
      "Ads.InterestGroup.NumSelectableBuyerAndSellerReportingIds", 0);
}

TEST_F(InterestGroupManagerImplTest, JoinInterestGroupWithOneAd) {
  base::HistogramTester histograms;
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  blink::InterestGroup test_group = NewInterestGroup(test_origin, "example");
  test_group.ads.emplace();
  test_group.ads->emplace_back(
      /*render_gurl=*/GURL("https://full.example.com/ad1"),
      /*metadata=*/"metadata1");

  TestInterestGroupObserver interest_group_observer;
  interest_group_manager_->AddInterestGroupObserver(&interest_group_observer);

  interest_group_manager_->JoinInterestGroup(test_group, test_origin.GetURL());
  interest_group_observer.WaitForAccesses(
      {{/*devtools_auction_id=*/"global",
        InterestGroupManagerImpl::InterestGroupObserver::AccessType::kJoin,
        /*owner_origin=*/test_origin, /*ig_name=*/"example",
        /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt,
        /*component_seller_origin=*/std::nullopt}});

  EXPECT_THAT(k_anon_delegate_.queried_ids(), testing::SizeIs(2));

  SingleStorageInterestGroup loaded_group = GetSingleInterestGroup(test_origin);
  ASSERT_EQ(1u, loaded_group->interest_group.ads->size());
  const blink::InterestGroup::Ad& ad = (*loaded_group->interest_group.ads)[0];
  EXPECT_EQ(GURL("https://full.example.com/ad1"), ad.render_url());
  EXPECT_EQ("metadata1", ad.metadata);

  histograms.ExpectTotalCount(
      "Ads.InterestGroup.NumSelectableBuyerAndSellerReportingIds", 0);
}

TEST_F(InterestGroupManagerImplTest,
       JoinInterestGroupWithOneAdAndSelectableBuyerAndSellerReportingIds) {
  base::test::ScopedFeatureList feature_list{
      blink::features::kFledgeAuctionDealSupport};
  base::HistogramTester histograms;
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  blink::InterestGroup test_group = NewInterestGroup(test_origin, "example");
  test_group.ads.emplace();
  test_group.ads->emplace_back(
      /*render_gurl=*/GURL("https://full.example.com/ad1"),
      /*metadata=*/"metadata1",
      /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/std::nullopt,
      /*buyer_and_seller_reporting_id=*/std::nullopt,
      /*selectable_buyer_and_seller_reporting_ids=*/
      std::vector<std::string>({"selectable1", "selectable2", "selectable3"}));

  {
    TestInterestGroupObserver interest_group_observer;
    interest_group_manager_->AddInterestGroupObserver(&interest_group_observer);

    interest_group_manager_->JoinInterestGroup(test_group,
                                               test_origin.GetURL());
    interest_group_observer.WaitForAccesses(
        {{/*devtools_auction_id=*/"global",
          InterestGroupManagerImpl::InterestGroupObserver::AccessType::kJoin,
          /*owner_origin=*/test_origin, /*ig_name=*/"example",
          /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt,
          /*component_seller_origin=*/std::nullopt}});

    EXPECT_THAT(k_anon_delegate_.queried_ids(), testing::SizeIs(5));

    SingleStorageInterestGroup loaded_group =
        GetSingleInterestGroup(test_origin);
    ASSERT_EQ(1u, loaded_group->interest_group.ads->size());
    const blink::InterestGroup::Ad& ad = (*loaded_group->interest_group.ads)[0];
    EXPECT_EQ(GURL("https://full.example.com/ad1"), ad.render_url());
    EXPECT_EQ("metadata1", ad.metadata);
    EXPECT_THAT(
        *ad.selectable_buyer_and_seller_reporting_ids,
        testing::ElementsAre("selectable1", "selectable2", "selectable3"));

    interest_group_manager_->RemoveInterestGroupObserver(
        &interest_group_observer);
  }

  histograms.ExpectUniqueSample(
      "Ads.InterestGroup.NumSelectableBuyerAndSellerReportingIds", 3, 1);

  {
    base::test::ScopedFeatureList disabled_feature_list;
    disabled_feature_list.InitAndDisableFeature(
        blink::features::kFledgeAuctionDealSupport);

    TestInterestGroupObserver interest_group_observer;
    interest_group_manager_->AddInterestGroupObserver(&interest_group_observer);

    interest_group_manager_->JoinInterestGroup(test_group,
                                               test_origin.GetURL());
    interest_group_observer.WaitForAccesses(
        {{/*devtools_auction_id=*/"global",
          InterestGroupManagerImpl::InterestGroupObserver::AccessType::kJoin,
          /*owner_origin=*/test_origin, /*ig_name=*/"example",
          /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt,
          /*component_seller_origin=*/std::nullopt}});

    EXPECT_THAT(k_anon_delegate_.queried_ids(), testing::SizeIs(5));

    SingleStorageInterestGroup loaded_group =
        GetSingleInterestGroup(test_origin);
    ASSERT_EQ(1u, loaded_group->interest_group.ads->size());
    const blink::InterestGroup::Ad& ad = (*loaded_group->interest_group.ads)[0];
    EXPECT_EQ(GURL("https://full.example.com/ad1"), ad.render_url());
    EXPECT_EQ("metadata1", ad.metadata);
    EXPECT_EQ(ad.selectable_buyer_and_seller_reporting_ids, std::nullopt);
  }
}

TEST_F(InterestGroupManagerImplTest, UpdateInterestGroupWithNoAds) {
  base::HistogramTester histograms;
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  blink::InterestGroup test_group = NewInterestGroup(test_origin, "example");

  TestInterestGroupObserver interest_group_observer;
  interest_group_manager_->AddInterestGroupObserver(&interest_group_observer);

  interest_group_manager_->JoinInterestGroup(test_group, test_origin.GetURL());
  interest_group_observer.WaitForAccesses(
      {{/*devtools_auction_id=*/"global",
        InterestGroupManagerImpl::InterestGroupObserver::AccessType::kJoin,
        /*owner_origin=*/test_origin, /*ig_name=*/"example",
        /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt,
        /*component_seller_origin=*/std::nullopt}});

  InterestGroupUpdate update;
  update.ads.emplace();

  InterestGroupManagerImplTestPeer(interest_group_manager_.get())
      .UpdateInterestGroup(blink::InterestGroupKey(test_origin, "example"),
                           std::move(update));

  EXPECT_THAT(k_anon_delegate_.queried_ids(), testing::ElementsAre());

  SingleStorageInterestGroup loaded_group = GetSingleInterestGroup(test_origin);

  EXPECT_EQ(test_origin, loaded_group->interest_group.owner);
  EXPECT_EQ("example", loaded_group->interest_group.name);
  EXPECT_EQ(1, loaded_group->bidding_browser_signals->join_count);
  EXPECT_EQ(0, loaded_group->bidding_browser_signals->bid_count);
  EXPECT_EQ(test_origin, loaded_group->joining_origin);
  EXPECT_EQ(base::Time::Now(), loaded_group->join_time);
  EXPECT_EQ(base::Time::Now(), loaded_group->last_updated);
  EXPECT_NE(base::Time::Min(), loaded_group->next_update_after);

  histograms.ExpectTotalCount(
      "Ads.InterestGroup.NumSelectableBuyerAndSellerReportingIds", 0);
}

TEST_F(InterestGroupManagerImplTest, UpdateInterestGroupWithOneAd) {
  base::HistogramTester histograms;
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  blink::InterestGroup test_group = NewInterestGroup(test_origin, "example");

  TestInterestGroupObserver interest_group_observer;
  interest_group_manager_->AddInterestGroupObserver(&interest_group_observer);

  interest_group_manager_->JoinInterestGroup(test_group, test_origin.GetURL());
  interest_group_observer.WaitForAccesses(
      {{/*devtools_auction_id=*/"global",
        InterestGroupManagerImpl::InterestGroupObserver::AccessType::kJoin,
        /*owner_origin=*/test_origin, /*ig_name=*/"example",
        /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt,
        /*component_seller_origin=*/std::nullopt}});

  InterestGroupUpdate update;
  update.ads.emplace();
  update.ads->emplace_back(
      /*render_gurl=*/GURL("https://full.example.com/ad1"),
      /*metadata=*/"metadata1");

  InterestGroupManagerImplTestPeer(interest_group_manager_.get())
      .UpdateInterestGroup(blink::InterestGroupKey(test_origin, "example"),
                           std::move(update));

  EXPECT_THAT(k_anon_delegate_.queried_ids(), testing::SizeIs(2));

  SingleStorageInterestGroup loaded_group = GetSingleInterestGroup(test_origin);
  ASSERT_EQ(1u, loaded_group->interest_group.ads->size());
  const blink::InterestGroup::Ad& ad = (*loaded_group->interest_group.ads)[0];
  EXPECT_EQ(GURL("https://full.example.com/ad1"), ad.render_url());
  EXPECT_EQ("metadata1", ad.metadata);

  histograms.ExpectTotalCount(
      "Ads.InterestGroup.NumSelectableBuyerAndSellerReportingIds", 0);
}

TEST_F(InterestGroupManagerImplTest,
       UpdateInterestGroupWithOneAdAndSelectableBuyerAndSellerReportingIds) {
  base::test::ScopedFeatureList feature_list{
      blink::features::kFledgeAuctionDealSupport};
  base::HistogramTester histograms;
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  blink::InterestGroup test_group = NewInterestGroup(test_origin, "example");

  TestInterestGroupObserver interest_group_observer;
  interest_group_manager_->AddInterestGroupObserver(&interest_group_observer);

  interest_group_manager_->JoinInterestGroup(test_group, test_origin.GetURL());
  interest_group_observer.WaitForAccesses(
      {{/*devtools_auction_id=*/"global",
        InterestGroupManagerImpl::InterestGroupObserver::AccessType::kJoin,
        /*owner_origin=*/test_origin, /*ig_name=*/"example",
        /*bid=*/std::nullopt, /*bid_currency=*/std::nullopt,
        /*component_seller_origin=*/std::nullopt}});

  {
    InterestGroupUpdate update;
    update.ads.emplace();
    update.ads->emplace_back(
        /*render_gurl=*/GURL("https://full.example.com/ad1"),
        /*metadata=*/"metadata1",
        /*size_group=*/std::nullopt,
        /*buyer_reporting_id=*/std::nullopt,
        /*buyer_and_seller_reporting_id=*/std::nullopt,
        /*selectable_buyer_and_seller_reporting_ids=*/
        std::vector<std::string>(
            {"selectable1", "selectable2", "selectable3"}));

    InterestGroupManagerImplTestPeer(interest_group_manager_.get())
        .UpdateInterestGroup(blink::InterestGroupKey(test_origin, "example"),
                             std::move(update));

    EXPECT_THAT(k_anon_delegate_.queried_ids(), testing::SizeIs(5));

    SingleStorageInterestGroup loaded_group =
        GetSingleInterestGroup(test_origin);
    ASSERT_EQ(1u, loaded_group->interest_group.ads->size());
    const blink::InterestGroup::Ad& ad = (*loaded_group->interest_group.ads)[0];
    EXPECT_EQ(GURL("https://full.example.com/ad1"), ad.render_url());
    EXPECT_EQ("metadata1", ad.metadata);
    EXPECT_THAT(
        *ad.selectable_buyer_and_seller_reporting_ids,
        testing::ElementsAre("selectable1", "selectable2", "selectable3"));
  }

  histograms.ExpectUniqueSample(
      "Ads.InterestGroup.NumSelectableBuyerAndSellerReportingIds", 3, 1);

  {
    base::test::ScopedFeatureList disabled_feature_list;
    disabled_feature_list.InitAndDisableFeature(
        blink::features::kFledgeAuctionDealSupport);

    InterestGroupUpdate update;
    update.ads.emplace();
    update.ads->emplace_back(
        /*render_gurl=*/GURL("https://full.example.com/ad1"),
        /*metadata=*/"metadata1",
        /*size_group=*/std::nullopt,
        /*buyer_reporting_id=*/std::nullopt,
        /*buyer_and_seller_reporting_id=*/std::nullopt,
        /*selectable_buyer_and_seller_reporting_ids=*/
        std::vector<std::string>(
            {"selectable1", "selectable2", "selectable3"}));

    InterestGroupManagerImplTestPeer(interest_group_manager_.get())
        .UpdateInterestGroup(blink::InterestGroupKey(test_origin, "example"),
                             std::move(update));

    SingleStorageInterestGroup loaded_group =
        GetSingleInterestGroup(test_origin);
    ASSERT_EQ(1u, loaded_group->interest_group.ads->size());
    const blink::InterestGroup::Ad& ad = (*loaded_group->interest_group.ads)[0];
    EXPECT_EQ(GURL("https://full.example.com/ad1"), ad.render_url());
    EXPECT_EQ("metadata1", ad.metadata);
    EXPECT_EQ(ad.selectable_buyer_and_seller_reporting_ids, std::nullopt);
  }
}

TEST_F(InterestGroupManagerImplTest, RecordRandomDebugReportLockout) {
  base::test::TestFuture<std::optional<DebugReportLockoutAndCooldowns>> result;
  base::flat_set<url::Origin> origins;
  interest_group_manager_->GetDebugReportLockoutAndCooldowns(
      origins, result.GetCallback());
  ASSERT_TRUE(result.Get().has_value());
  EXPECT_FALSE(result.Get()->lockout.has_value());

  base::Time now = base::Time::Now();
  interest_group_manager_->RecordRandomDebugReportLockout(now);
  base::test::TestFuture<std::optional<DebugReportLockoutAndCooldowns>> result2;
  interest_group_manager_->GetDebugReportLockoutAndCooldowns(
      origins, result2.GetCallback());

  ASSERT_TRUE(result2.Get().has_value());
  EXPECT_TRUE(result2.Get()->lockout.has_value());
  base::Time expected_time = base::Time::FromDeltaSinceWindowsEpoch(
      now.ToDeltaSinceWindowsEpoch().CeilToMultiple(base::Hours(1)));
  EXPECT_EQ(expected_time, result2.Get()->lockout->starting_time);
  EXPECT_GE(result2.Get()->lockout->duration, base::Days(1));
  EXPECT_LE(result2.Get()->lockout->duration, base::Days(90));
}
}  // namespace
}  // namespace content

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"

#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "url/origin.h"

namespace content {

namespace {

constexpr base::TimeDelta kJoinInterval = base::Hours(1);
constexpr base::TimeDelta kQueryInterval = base::Hours(2);

constexpr char kAdURL[] = "https://www.foo.com/ad1.html";
constexpr char kBiddingURL[] = "https://www.example.com/bidding_logic";
constexpr char kUpdateURL[] = "https://www.example.com/update";

class TestKAnonymityServiceDelegate : public KAnonymityServiceDelegate {
 public:
  TestKAnonymityServiceDelegate(bool has_error = false)
      : has_error_(has_error) {}

  void JoinSet(std::string id,
               base::OnceCallback<void(bool)> callback) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), !has_error_));
  }

  void QuerySets(
      std::vector<std::string> ids,
      base::OnceCallback<void(std::vector<bool>)> callback) override {
    if (has_error_) {
      // An error is indicated by an empty status.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::vector<bool>()));
    } else {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    std::vector<bool>(ids.size(), true)));
    }
  }

  base::TimeDelta GetJoinInterval() override { return kJoinInterval; }

  base::TimeDelta GetQueryInterval() override { return kQueryInterval; }

 private:
  bool has_error_;
};

blink::InterestGroup MakeInterestGroup(url::Origin owner, std::string name) {
  blink::InterestGroup group;
  group.expiry = base::Time::Now() + base::Days(1);
  group.owner = owner;
  group.name = name;
  group.bidding_url = GURL(kBiddingURL);
  group.update_url = GURL(kUpdateURL);
  group.ads.emplace();
  group.ads->push_back(blink::InterestGroup::Ad(GURL(kAdURL), /*metadata=*/""));
  EXPECT_TRUE(group.IsValid());
  return group;
}

}  // namespace

class InterestGroupKAnonymityManagerTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  absl::optional<StorageInterestGroup> GetGroup(
      InterestGroupManagerImpl* manager,
      url::Origin owner,
      std::string name) {
    absl::optional<StorageInterestGroup> result;
    base::RunLoop run_loop;
    manager->GetInterestGroup(
        blink::InterestGroupKey(owner, name),
        base::BindLambdaForTesting(
            [&result, &run_loop](absl::optional<StorageInterestGroup> group) {
              result = std::move(group);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  absl::optional<base::Time> GetLastReported(InterestGroupManagerImpl* manager,
                                             std::string key) {
    absl::optional<base::Time> result;
    base::RunLoop run_loop;
    manager->GetLastKAnonymityReported(
        key, base::BindLambdaForTesting(
                 [&result, &run_loop](absl::optional<base::Time> reported) {
                   result = std::move(reported);
                   run_loop.Quit();
                 }));
    run_loop.Run();
    return result;
  }

  std::unique_ptr<InterestGroupManagerImpl> CreateManager(
      bool has_error = false) {
    delegate_ = std::make_unique<TestKAnonymityServiceDelegate>(has_error);
    return std::make_unique<InterestGroupManagerImpl>(
        temp_directory_.GetPath(), false,
        InterestGroupManagerImpl::ProcessMode::kDedicated, nullptr,
        delegate_.get());
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 protected:
  base::ScopedTempDir temp_directory_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<KAnonymityServiceDelegate> delegate_;
};

TEST_F(InterestGroupKAnonymityManagerTest,
       QueueUpdatePerformsQuerySetsForGroup) {
  auto manager = CreateManager();
  const GURL top_frame = GURL("https://www.example.com/foo");
  const url::Origin owner = url::Origin::Create(top_frame);
  const std::string name = "foo";
  const blink::InterestGroupKey ig_key{owner, name};

  EXPECT_FALSE(GetGroup(manager.get(), owner, name));
  base::Time before_join = base::Time::Now();

  // Join queues the update, but returns first.
  blink::InterestGroup g = MakeInterestGroup(owner, name);
  manager->JoinInterestGroup(g, top_frame);
  // Set k_anon value to true so that it gets returned with the interest group.
  manager->UpdateKAnonymity(
      {blink::KAnonKeyForAdBid(g, g.ads->at(0).render_url), true,
       base::Time::Min()});

  auto maybe_group = GetGroup(manager.get(), owner, name);
  ASSERT_TRUE(maybe_group);
  EXPECT_EQ(base::Time::Min(), maybe_group->bidding_ads_kanon[0].last_updated);

  // k-anonymity update happens here.
  task_environment().FastForwardBy(base::Minutes(1));

  maybe_group = GetGroup(manager.get(), owner, name);
  ASSERT_TRUE(maybe_group);
  base::Time last_updated = maybe_group->bidding_ads_kanon[0].last_updated;
  EXPECT_LE(before_join, last_updated);
  EXPECT_GT(base::Time::Now(), last_updated);

  // Updated recently so we shouldn't update again.
  manager->QueueKAnonymityUpdateForInterestGroup(ig_key);
  task_environment().FastForwardBy(base::Minutes(1));

  maybe_group = GetGroup(manager.get(), owner, name);
  ASSERT_TRUE(maybe_group);
  EXPECT_EQ(last_updated, maybe_group->bidding_ads_kanon[0].last_updated);

  task_environment().FastForwardBy(kQueryInterval);

  // Updated more than 24 hours ago, so update.
  manager->QueueKAnonymityUpdateForInterestGroup(ig_key);
  task_environment().RunUntilIdle();
  maybe_group = GetGroup(manager.get(), owner, name);
  ASSERT_TRUE(maybe_group);
  EXPECT_LT(last_updated, maybe_group->bidding_ads_kanon[0].last_updated);
}

TEST_F(InterestGroupKAnonymityManagerTest,
       RegisterAdKeysAsJoinedPerformsJoinSet) {
  const GURL top_frame = GURL("https://www.example.com/foo");
  const url::Origin owner = url::Origin::Create(top_frame);
  const std::string name = "foo";
  blink::InterestGroup group = MakeInterestGroup(owner, "foo");
  group.bidding_url = GURL("https://www.example.com/bidding.js");

  const std::string kAd1KAnonBidKey = KAnonKeyForAdBid(group, GURL(kAdURL));
  const std::string kAd1KAnonReportNameKey =
      KAnonKeyForAdNameReporting(group, group.ads.value()[0]);

  auto manager = CreateManager();
  EXPECT_FALSE(GetGroup(manager.get(), owner, name));
  EXPECT_EQ(base::Time::Min(), GetLastReported(manager.get(), kAd1KAnonBidKey));
  EXPECT_EQ(base::Time::Min(),
            GetLastReported(manager.get(), kAd1KAnonReportNameKey));

  manager->JoinInterestGroup(group, top_frame);
  // The group *must* exist when JoinInterestGroup returns.
  ASSERT_TRUE(GetGroup(manager.get(), owner, name));

  // k-anonymity would happens here.
  task_environment().FastForwardBy(base::Minutes(1));

  // Ads are *not* reported as part of joining an interest group.
  absl::optional<base::Time> reported =
      GetLastReported(manager.get(), kAd1KAnonBidKey);
  EXPECT_EQ(base::Time::Min(), reported);
  EXPECT_EQ(base::Time::Min(),
            GetLastReported(manager.get(), kAd1KAnonReportNameKey));

  base::Time before_mark_ad = base::Time::Now();
  manager->RegisterAdKeysAsJoined({kAd1KAnonBidKey, kAd1KAnonReportNameKey});

  // k-anonymity update happens here.
  task_environment().FastForwardBy(base::Minutes(1));

  reported = GetLastReported(manager.get(), kAd1KAnonBidKey);
  EXPECT_LE(before_mark_ad, reported);
  ASSERT_TRUE(reported);
  base::Time last_reported = *reported;
  EXPECT_EQ(last_reported,
            GetLastReported(manager.get(), kAd1KAnonReportNameKey));

  manager->RegisterAdKeysAsJoined({kAd1KAnonBidKey});
  task_environment().FastForwardBy(base::Minutes(1));

  // Second update shouldn't have changed the update time (too recent).
  EXPECT_EQ(last_reported, GetLastReported(manager.get(), kAd1KAnonBidKey));
  EXPECT_EQ(last_reported,
            GetLastReported(manager.get(), kAd1KAnonReportNameKey));

  task_environment().FastForwardBy(kJoinInterval);

  // Updated more than 24 hours ago, so update.
  manager->RegisterAdKeysAsJoined({kAd1KAnonBidKey});
  task_environment().RunUntilIdle();
  EXPECT_LT(last_reported, GetLastReported(manager.get(), kAd1KAnonBidKey));
  // Other key should not have changed.
  EXPECT_EQ(last_reported,
            GetLastReported(manager.get(), kAd1KAnonReportNameKey));
}

TEST_F(InterestGroupKAnonymityManagerTest, HandlesServerErrors) {
  const GURL top_frame = GURL("https://www.example.com/foo");
  const url::Origin owner = url::Origin::Create(top_frame);
  const std::string name = "foo";

  auto manager = CreateManager(/*has_error=*/true);
  blink::InterestGroup g = MakeInterestGroup(owner, "foo");
  const std::string kAd1KAnonBidKey = KAnonKeyForAdBid(g, GURL(kAdURL));

  manager->JoinInterestGroup(g, top_frame);

  // Set kAd1KAnonBidKey's is_k_anonymous to true so that it'll be returned with
  // the interest group.
  manager->UpdateKAnonymity({kAd1KAnonBidKey, true, base::Time::Min()});

  // The group *must* exist when JoinInterestGroup returns.
  ASSERT_TRUE(GetGroup(manager.get(), owner, name));
  manager->RegisterAdKeysAsJoined({kAd1KAnonBidKey});

  // k-anonymity update happens here.
  task_environment().FastForwardBy(base::Minutes(1));

  // If the update failed then we normally would not record the update as
  // having been completed, so we would try it later.
  // For now we'll record the update as having been completed to to reduce
  // bandwidth and provide more accurate use counts.
  // When the server is actually implemented we'll need to change the expected
  // values below.

  absl::optional<base::Time> ad_reported =
      GetLastReported(manager.get(), kAd1KAnonBidKey);
  ASSERT_TRUE(ad_reported);

  EXPECT_EQ(base::Time::Min(), ad_reported);

  auto maybe_group = GetGroup(manager.get(), owner, name);
  ASSERT_TRUE(maybe_group);

  EXPECT_EQ(base::Time::Min(), maybe_group->bidding_ads_kanon[0].last_updated);
}

class MockAnonymityServiceDelegate : public KAnonymityServiceDelegate {
 public:
  void JoinSet(std::string id,
               base::OnceCallback<void(bool)> callback) override {
    joined_ids_.emplace_back(std::move(id));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  }

  void QuerySets(
      std::vector<std::string> ids,
      base::OnceCallback<void(std::vector<bool>)> callback) override {
    size_t num_ids = ids.size();
    query_ids_.emplace_back(std::move(ids));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), std::vector<bool>(num_ids, true)));
  }
  base::TimeDelta GetJoinInterval() override { return kJoinInterval; }

  base::TimeDelta GetQueryInterval() override { return kQueryInterval; }

  std::vector<std::string> TakeJoinedIDs() {
    std::vector<std::string> retval;
    std::swap(retval, joined_ids_);
    return retval;
  }

  std::vector<std::vector<std::string>> TakeQueryIDs() {
    std::vector<std::vector<std::string>> retval;
    std::swap(retval, query_ids_);
    return retval;
  }

 private:
  std::vector<std::string> joined_ids_;
  std::vector<std::vector<std::string>> query_ids_;
};

class InterestGroupKAnonymityManagerTestWithMock
    : public InterestGroupKAnonymityManagerTest {
 public:
  std::unique_ptr<InterestGroupManagerImpl> CreateManager(
      bool has_error = false) {
    delegate_ = std::make_unique<MockAnonymityServiceDelegate>();
    return std::make_unique<InterestGroupManagerImpl>(
        temp_directory_.GetPath(), false,
        InterestGroupManagerImpl::ProcessMode::kDedicated, nullptr,
        delegate_.get());
  }

  MockAnonymityServiceDelegate* delegate() {
    return static_cast<MockAnonymityServiceDelegate*>(delegate_.get());
  }
};

TEST_F(InterestGroupKAnonymityManagerTestWithMock,
       JoinSetShouldNotRequestDuplicates) {
  auto manager = CreateManager();
  const GURL top_frame = GURL("https://www.example.com/foo");
  const url::Origin owner = url::Origin::Create(top_frame);
  const GURL ad1 = GURL(kAdURL);

  blink::InterestGroup group1 = MakeInterestGroup(owner, "foo");
  const std::string kAd1KAnonKey = KAnonKeyForAdBid(group1, ad1);

  // Join one group twice, and an overlapping group once.
  manager->JoinInterestGroup(group1, top_frame);
  manager->JoinInterestGroup(group1, top_frame);
  manager->JoinInterestGroup(MakeInterestGroup(owner, "bar"), top_frame);

  manager->RegisterAdKeysAsJoined({kAd1KAnonKey});
  manager->RegisterAdKeysAsJoined({kAd1KAnonKey});

  // k-anonymity update happens here.
  task_environment().FastForwardBy(base::Minutes(1));

  // Should have no duplicates.
  std::vector<std::string> joined_ids = delegate()->TakeJoinedIDs();
  base::flat_set<std::string> id_set(joined_ids);
  EXPECT_EQ(joined_ids.size(), id_set.size());
}

TEST_F(InterestGroupKAnonymityManagerTestWithMock, QuerySetShouldBatch) {
  auto manager = CreateManager();
  const GURL top_frame = GURL("https://www.example.com/foo");
  const url::Origin owner = url::Origin::Create(top_frame);

  // Each ad creates 2 k-anon entries. We want enough ads to just exceed the
  // batch size limit.
  const size_t kNumAds = kQueryBatchSizeLimit / 2 + 1;

  blink::InterestGroup group1 = MakeInterestGroup(owner, "foo");
  group1.ads->clear();
  for (size_t i = 0; i < kNumAds; i++) {
    group1.ads->emplace_back(blink::InterestGroup::Ad(
        GURL(base::StrCat(
            {"https://www.foo.com/ad", base::NumberToString(i), ".html"})),
        /*metadata=*/""));
  }
  // Join by itself triggers an update.
  manager->JoinInterestGroup(group1, top_frame);

  // k-anonymity update happens here.
  task_environment().FastForwardBy(base::Minutes(1));

  // The queries should have been split into 1 group of the max size and 1 group
  // of the remaining one.
  std::vector<std::vector<std::string>> queried_batches =
      delegate()->TakeQueryIDs();
  ASSERT_EQ(2u, queried_batches.size());
  EXPECT_EQ(kQueryBatchSizeLimit, queried_batches[0].size());
  EXPECT_EQ(2u, queried_batches[1].size());
}

}  // namespace content

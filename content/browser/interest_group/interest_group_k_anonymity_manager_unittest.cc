// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"

#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_manager_impl.h"
#include "content/browser/interest_group/interest_group_update.h"
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

blink::InterestGroup CreateAndJoinInterestGroup(
    const url::Origin& owner,
    const std::string& name,
    InterestGroupManagerImpl* manager,
    const GURL& joining_url) {
  blink::InterestGroup group = MakeInterestGroup(owner, name);
  manager->JoinInterestGroup(group, joining_url);
  return group;
}

}  // namespace

class InterestGroupKAnonymityManagerTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  std::optional<SingleStorageInterestGroup> GetGroup(
      InterestGroupManagerImpl* manager,
      url::Origin owner,
      std::string name) {
    std::optional<SingleStorageInterestGroup> result;
    base::RunLoop run_loop;
    manager->GetInterestGroup(
        blink::InterestGroupKey(owner, name),
        base::BindLambdaForTesting(
            [&result,
             &run_loop](std::optional<SingleStorageInterestGroup> group) {
              result = std::move(group);
              run_loop.Quit();
            }));
    run_loop.Run();
    return result;
  }

  std::optional<base::Time> GetLastReported(InterestGroupManagerImpl* manager,
                                            std::string key) {
    std::optional<base::Time> result;
    base::RunLoop run_loop;
    manager->GetLastKAnonymityReported(
        key, base::BindLambdaForTesting(
                 [&result, &run_loop](std::optional<base::Time> reported) {
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
        base::BindLambdaForTesting([&]() { return delegate_.get(); }));
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
  blink::InterestGroup g =
      CreateAndJoinInterestGroup(owner, name, manager.get(), top_frame);
  // Set a k_anon value to true so that it gets returned with the interest
  // group.
  std::string k_anon_key =
      blink::HashedKAnonKeyForAdBid(g, g.ads->at(0).render_url());
  manager->UpdateKAnonymity(blink::InterestGroupKey(g.owner, g.name),
                            {k_anon_key}, base::Time::Min(),
                            /*replace_existing_values*/ true);

  {
    auto maybe_group = GetGroup(manager.get(), owner, name);
    ASSERT_TRUE(maybe_group);
    EXPECT_EQ(base::Time::Min(), maybe_group.value()->last_k_anon_updated);
    // The database just has that one key that we set above.
    ASSERT_EQ(1u, maybe_group.value()->hashed_kanon_keys.size());
    EXPECT_THAT(maybe_group.value()->hashed_kanon_keys,
                testing::ElementsAre(k_anon_key));
  }

  // k-anonymity update happens here.
  task_environment().FastForwardBy(base::Minutes(1));

  base::Time last_updated;
  {
    auto maybe_group = GetGroup(manager.get(), owner, name);
    ASSERT_TRUE(maybe_group);
    last_updated = maybe_group.value()->last_k_anon_updated;
    EXPECT_LE(before_join, last_updated);
    EXPECT_GT(base::Time::Now(), last_updated);
    // The interest group we joined has 2 associated k-anon keys, which were
    // both set to true by our mock k-anonymity service.
    ASSERT_EQ(2u, maybe_group.value()->hashed_kanon_keys.size());
    EXPECT_THAT(maybe_group.value()->hashed_kanon_keys,
                testing::Contains(k_anon_key));
  }
  // Updated recently so we shouldn't update again.
  {
    InterestGroupKanonUpdateParameter kanon_update(last_updated);
    manager->QueueKAnonymityUpdateForInterestGroup(ig_key,
                                                   std::move(kanon_update));
    task_environment().FastForwardBy(base::Minutes(1));
    auto maybe_group = GetGroup(manager.get(), owner, name);
    ASSERT_TRUE(maybe_group);
    EXPECT_EQ(last_updated, maybe_group.value()->last_k_anon_updated);
    ASSERT_EQ(2u, maybe_group.value()->hashed_kanon_keys.size());
    EXPECT_THAT(maybe_group.value()->hashed_kanon_keys,
                testing::Contains(k_anon_key));
  }

  task_environment().FastForwardBy(kQueryInterval);
  // Updated more than 24 hours ago, so update.
  {
    InterestGroupKanonUpdateParameter kanon_update(last_updated);
    kanon_update.hashed_keys = {k_anon_key};
    manager->QueueKAnonymityUpdateForInterestGroup(ig_key,
                                                   std::move(kanon_update));
    task_environment().RunUntilIdle();
    auto maybe_group = GetGroup(manager.get(), owner, name);
    ASSERT_TRUE(maybe_group);
    EXPECT_LT(last_updated, maybe_group.value()->last_k_anon_updated);
    // This update only contains one key.
    ASSERT_EQ(1u, maybe_group.value()->hashed_kanon_keys.size());
    EXPECT_THAT(maybe_group.value()->hashed_kanon_keys,
                testing::ElementsAre(k_anon_key));
  }
}

TEST_F(
    InterestGroupKAnonymityManagerTest,
    QueueUpdateCorrectlyChoosesWhetherToReplaceOrMergeWithExistingKanonValues) {
  auto manager = CreateManager();
  const GURL top_frame = GURL("https://www.example.com/foo");
  const url::Origin owner = url::Origin::Create(top_frame);
  const GURL ad1 = GURL(kAdURL);

  blink::InterestGroup group = MakeInterestGroup(owner, "foo");
  // Keep the group from expiring during the test.
  group.expiry = base::Time::Max();
  const std::string kAd1KAnonKey = HashedKAnonKeyForAdBid(group, ad1.spec());

  // Join group. Queue a k-anon update. The update will "replace" the existing
  // k-anonymous keys (no keys are k-anonymous yet because the group was just
  // joined).
  {
    manager->JoinInterestGroup(group, top_frame);
    auto maybe_group = GetGroup(manager.get(), group.owner, group.name);
    ASSERT_TRUE(maybe_group);
    EXPECT_EQ(base::Time::Min(), maybe_group.value()->last_k_anon_updated);
    EXPECT_TRUE(maybe_group.value()->hashed_kanon_keys.empty());
  }

  // Update would happen here.
  base::Time last_k_anon_updated;
  {
    task_environment().FastForwardBy(base::Minutes(1));
    auto maybe_group = GetGroup(manager.get(), group.owner, group.name);
    ASSERT_TRUE(maybe_group);
    EXPECT_LT(base::Time::Min(), maybe_group.value()->last_k_anon_updated);
    last_k_anon_updated = maybe_group.value()->last_k_anon_updated;
    EXPECT_EQ(2u, maybe_group.value()->hashed_kanon_keys.size());
  }

  // Fast forward to just before the minimum wait time.
  task_environment().FastForwardBy(kQueryInterval - base::Minutes(1) -
                                   base::Seconds(1));

  // Join an interest group again with different keys. The time won't get
  // updated but the new keys should be returned.
  {
    group.ads->emplace_back(GURL("https://www.foo.com/ad2.html"),
                            /*metadata=*/"");
    manager->JoinInterestGroup(group, top_frame);
    task_environment().FastForwardBy(base::Minutes(1));
    auto maybe_group = GetGroup(manager.get(), group.owner, group.name);
    ASSERT_TRUE(maybe_group);
    EXPECT_EQ(last_k_anon_updated, maybe_group.value()->last_k_anon_updated);
    EXPECT_EQ(4u, maybe_group.value()->hashed_kanon_keys.size());
  }

  // The minimum wait time will be over after the next fast forward. The time
  // should be updated.
  {
    task_environment().FastForwardBy(base::Seconds(1));
    manager->JoinInterestGroup(group, top_frame);
    task_environment().FastForwardBy(base::Minutes(1));
    auto maybe_group = GetGroup(manager.get(), group.owner, group.name);
    ASSERT_TRUE(maybe_group);
    EXPECT_LT(last_k_anon_updated, maybe_group.value()->last_k_anon_updated);
    EXPECT_EQ(4u, maybe_group.value()->hashed_kanon_keys.size());
  }
}

// TODO(crbug.com/334053709): Add test with reporting IDs.
TEST_F(InterestGroupKAnonymityManagerTest,
       RegisterAdKeysAsJoinedPerformsJoinSet) {
  const GURL top_frame = GURL("https://www.example.com/foo");
  const url::Origin owner = url::Origin::Create(top_frame);
  const std::string name = "foo";
  blink::InterestGroup group = MakeInterestGroup(owner, "foo");
  group.bidding_url = GURL("https://www.example.com/bidding.js");

  const std::string kAd1KAnonBidKey =
      HashedKAnonKeyForAdBid(group, GURL(kAdURL).spec());
  const std::string kAd1KAnonReportNameKey = HashedKAnonKeyForAdNameReporting(
      group, group.ads.value()[0],
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt);

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
  std::optional<base::Time> reported =
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
  blink::InterestGroup g =
      CreateAndJoinInterestGroup(owner, "foo", manager.get(), top_frame);
  const std::string kAd1KAnonBidKey =
      HashedKAnonKeyForAdBid(g, GURL(kAdURL).spec());

  // Set kAd1KAnonBidKey's is_k_anonymous to true so that it'll be returned with
  // the interest group.
  base::Time k_anon_update_time = base::Time::Now();
  manager->UpdateKAnonymity(blink::InterestGroupKey(g.owner, g.name),
                            {kAd1KAnonBidKey}, k_anon_update_time,
                            /*replace_existing_values*/ true);

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

  std::optional<base::Time> ad_reported =
      GetLastReported(manager.get(), kAd1KAnonBidKey);
  ASSERT_TRUE(ad_reported);

  EXPECT_EQ(base::Time::Min(), ad_reported);

  auto maybe_group = GetGroup(manager.get(), owner, name);
  ASSERT_TRUE(maybe_group);

  EXPECT_EQ(k_anon_update_time, maybe_group.value()->last_k_anon_updated);
}

class MockAnonymityServiceDelegate : public KAnonymityServiceDelegate {
 public:
  explicit MockAnonymityServiceDelegate(base::TimeDelta query_sets_delay)
      : query_sets_delay_(query_sets_delay) {}

  void JoinSet(std::string id,
               base::OnceCallback<void(bool)> callback) override {
    joined_ids_.emplace_back(std::move(id));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), true));
  }

  void QuerySets(
      std::vector<std::string> ids,
      base::OnceCallback<void(std::vector<bool>)> callback) override {
    // Give this a delay so that we can test the k-anonymity manager's
    // ability to manage a new QueryKAnonymityData while waiting for
    // a response from a previous query.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&MockAnonymityServiceDelegate::QuerySetsCallback,
                       base::Unretained(this), std::move(ids),
                       std::move(callback)),
        query_sets_delay_);
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
  base::TimeDelta query_sets_delay_;

  void QuerySetsCallback(std::vector<std::string> ids,
                         base::OnceCallback<void(std::vector<bool>)> callback) {
    size_t num_ids = ids.size();
    query_ids_.emplace_back(std::move(ids));
    std::move(callback).Run(std::vector<bool>(num_ids, true));
  }
};

class InterestGroupKAnonymityManagerTestWithMock
    : public InterestGroupKAnonymityManagerTest {
 public:
  std::unique_ptr<InterestGroupManagerImpl> CreateManager(
      bool has_error = false,
      base::TimeDelta query_sets_delay = base::Milliseconds(0)) {
    delegate_ =
        std::make_unique<MockAnonymityServiceDelegate>(query_sets_delay);
    return std::make_unique<InterestGroupManagerImpl>(
        temp_directory_.GetPath(), false,
        InterestGroupManagerImpl::ProcessMode::kDedicated, nullptr,
        base::BindLambdaForTesting([&]() { return delegate_.get(); }));
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
  const std::string kAd1KAnonKey = HashedKAnonKeyForAdBid(group1, ad1.spec());

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

// Make sure the InterestGroupKAnonymityManager can handle the case
// when QueueKAnonymityUpdateForInterestGroup is called while there
// is still an outstanding query request.
TEST_F(InterestGroupKAnonymityManagerTestWithMock,
       QuerySetShouldHandleOverlappingRequests) {
  auto manager = CreateManager(/*has_error=*/false,
                               /*query_sets_delay=*/base::Milliseconds(10));
  const GURL top_frame = GURL("https://www.example.com/foo");
  const url::Origin owner = url::Origin::Create(top_frame);

  blink::InterestGroup group =
      CreateAndJoinInterestGroup(owner, "foo", manager.get(), top_frame);
  blink::InterestGroupKey group_key(group.owner, group.name);
  blink::InterestGroup group2 =
      CreateAndJoinInterestGroup(owner, "bar", manager.get(), top_frame);
  task_environment().FastForwardBy(base::Minutes(1));
  // JoinInterestGroup causes an update, but get a clean slate for the tests
  // below.
  EXPECT_EQ(2u, delegate()->TakeQueryIDs().size());

  base::Time update_time;

  // Try a series of two "replacing" updates. The second update should override
  // the first.
  {
    InterestGroupKanonUpdateParameter kanon_update1(base::Time::Now() -
                                                    base::Days(2));
    kanon_update1.hashed_keys = {"update1"};
    InterestGroupKanonUpdateParameter kanon_update2(base::Time::Now() -
                                                    base::Days(1));
    kanon_update2.hashed_keys = {"update2"};
    manager->QueueKAnonymityUpdateForInterestGroup(group_key,
                                                   std::move(kanon_update1));
    task_environment().FastForwardBy(base::Milliseconds(1));
    update_time = base::Time::Now();
    manager->QueueKAnonymityUpdateForInterestGroup(group_key,
                                                   std::move(kanon_update2));

    // Check that this test works (i.e. the query sets requests overlap).
    EXPECT_EQ(0u, delegate()->TakeQueryIDs().size());
    task_environment().FastForwardBy(base::Minutes(1));
    EXPECT_EQ(2u, delegate()->TakeQueryIDs().size());

    // The second update happens (overriding the first).
    auto maybe_group = GetGroup(manager.get(), group.owner, group.name);
    ASSERT_TRUE(maybe_group);
    EXPECT_THAT(maybe_group.value()->hashed_kanon_keys,
                testing::UnorderedElementsAre("update2"));
    EXPECT_EQ(update_time, maybe_group.value()->last_k_anon_updated);
  }

  // Try a replacing update followed by a non-replacing update. They
  // should both happen.
  {
    InterestGroupKanonUpdateParameter kanon_update3(base::Time::Now() -
                                                    base::Days(2));
    kanon_update3.hashed_keys = {"update3"};
    InterestGroupKanonUpdateParameter kanon_update4(base::Time::Now() -
                                                    base::Minutes(1));
    // `hashed_keys` won't be used because this a non-replacing
    // update, but specify it anyway to make sure we use
    // `newly_added_hashed_keys`.
    kanon_update4.hashed_keys = {"update4", "new_update4"};
    kanon_update4.newly_added_hashed_keys = {"new_update4"};

    update_time = base::Time::Now();
    manager->QueueKAnonymityUpdateForInterestGroup(group_key,
                                                   std::move(kanon_update3));
    task_environment().FastForwardBy(base::Milliseconds(1));
    manager->QueueKAnonymityUpdateForInterestGroup(group_key,
                                                   std::move(kanon_update4));

    // Check that this test works (i.e. the query sets requests overlap).
    EXPECT_EQ(0u, delegate()->TakeQueryIDs().size());
    task_environment().FastForwardBy(base::Minutes(1));
    EXPECT_EQ(2u, delegate()->TakeQueryIDs().size());

    auto maybe_group = GetGroup(manager.get(), group.owner, group.name);
    ASSERT_TRUE(maybe_group);
    EXPECT_THAT(maybe_group.value()->hashed_kanon_keys,
                testing::UnorderedElementsAre("new_update4", "update3"));
    EXPECT_EQ(update_time, maybe_group.value()->last_k_anon_updated);
  }

  // Try a series of two non-replacing updates. They should both happen.
  {
    InterestGroupKanonUpdateParameter kanon_update5(base::Time::Now() -
                                                    base::Minutes(2));
    kanon_update5.hashed_keys = {"new_update5", "update4", "update3",
                                 "update2"};
    kanon_update5.newly_added_hashed_keys = {"new_update5"};
    InterestGroupKanonUpdateParameter kanon_update6(base::Time::Now() -
                                                    base::Minutes(1));
    kanon_update6.hashed_keys = {"update6", "new_update6"};
    kanon_update6.newly_added_hashed_keys = {"new_update6"};
    // Do not update `update_time` because we expect to keep the former
    // value.
    manager->QueueKAnonymityUpdateForInterestGroup(group_key,
                                                   std::move(kanon_update5));
    task_environment().FastForwardBy(base::Milliseconds(1));
    manager->QueueKAnonymityUpdateForInterestGroup(group_key,
                                                   std::move(kanon_update6));

    // Check that this test works (i.e. the query sets requests overlap).
    EXPECT_EQ(0u, delegate()->TakeQueryIDs().size());
    task_environment().FastForwardBy(base::Minutes(1));
    EXPECT_EQ(2u, delegate()->TakeQueryIDs().size());

    auto maybe_group = GetGroup(manager.get(), group.owner, group.name);
    ASSERT_TRUE(maybe_group);
    EXPECT_THAT(maybe_group.value()->hashed_kanon_keys,
                testing::UnorderedElementsAre("new_update4", "update3",
                                              "new_update5", "new_update6"));
    EXPECT_EQ(update_time, maybe_group.value()->last_k_anon_updated);
  }

  // Try a non-replacing update followed by a replacing update. Only the
  // second should take effect.
  {
    InterestGroupKanonUpdateParameter kanon_update7(base::Time::Now() -
                                                    base::Minutes(2));
    kanon_update7.hashed_keys = {"new_update7", "update7"};
    kanon_update7.newly_added_hashed_keys = {"new_update7"};
    InterestGroupKanonUpdateParameter kanon_update8(base::Time::Now() -
                                                    base::Days(2));
    kanon_update8.hashed_keys = {"update8", "new_update8"};
    kanon_update8.newly_added_hashed_keys = {"new_update8"};

    manager->QueueKAnonymityUpdateForInterestGroup(group_key,
                                                   std::move(kanon_update7));
    task_environment().FastForwardBy(base::Milliseconds(1));
    update_time = base::Time::Now();
    manager->QueueKAnonymityUpdateForInterestGroup(group_key,
                                                   std::move(kanon_update8));

    // Check that this test works (i.e. the query sets requests overlap).
    EXPECT_EQ(0u, delegate()->TakeQueryIDs().size());
    task_environment().FastForwardBy(base::Minutes(1));
    EXPECT_EQ(2u, delegate()->TakeQueryIDs().size());

    auto maybe_group = GetGroup(manager.get(), group.owner, group.name);
    ASSERT_TRUE(maybe_group);
    EXPECT_THAT(maybe_group.value()->hashed_kanon_keys,
                testing::UnorderedElementsAre("update8", "new_update8"));
    EXPECT_EQ(update_time, maybe_group.value()->last_k_anon_updated);
  }

  // Try a chain of several replacing updates. Only the last should take effect.
  {
    for (int i = 0; i < 5; i++) {
      InterestGroupKanonUpdateParameter update(base::Time::Now() -
                                               base::Days(2));
      update.hashed_keys = {"update"};
      manager->QueueKAnonymityUpdateForInterestGroup(group_key,
                                                     std::move(update));
      task_environment().FastForwardBy(base::Milliseconds(1));
    }
    InterestGroupKanonUpdateParameter last_update(base::Time::Now() -
                                                  base::Days(2));
    last_update.hashed_keys = {"last_update"};
    update_time = base::Time::Now();
    manager->QueueKAnonymityUpdateForInterestGroup(group_key,
                                                   std::move(last_update));

    // Check that this test works (i.e. the query sets requests overlap).
    EXPECT_EQ(0u, delegate()->TakeQueryIDs().size());
    task_environment().FastForwardBy(base::Minutes(1));
    EXPECT_EQ(6u, delegate()->TakeQueryIDs().size());

    auto maybe_group = GetGroup(manager.get(), group.owner, group.name);
    ASSERT_TRUE(maybe_group);
    EXPECT_THAT(maybe_group.value()->hashed_kanon_keys,
                testing::UnorderedElementsAre("last_update"));
    EXPECT_EQ(update_time, maybe_group.value()->last_k_anon_updated);
  }

  // Try a chain of several non-replacing updates. They should all take effect.
  {
    std::vector<std::string> expected_keys = {"last_update"};
    for (int i = 0; i < 5; i++) {
      InterestGroupKanonUpdateParameter update(base::Time::Now() -
                                               base::Seconds(2));
      std::string key(base::NumberToString(i));
      update.hashed_keys = {key, "123"};
      update.newly_added_hashed_keys = {key};
      expected_keys.emplace_back(key);
      manager->QueueKAnonymityUpdateForInterestGroup(group_key,
                                                     std::move(update));
      task_environment().FastForwardBy(base::Milliseconds(1));
    }
    InterestGroupKanonUpdateParameter last_update(base::Time::Now() -
                                                  base::Seconds(2));
    last_update.newly_added_hashed_keys = {"last_partial_update"};
    last_update.hashed_keys = {"last_partial_update", "123"};
    expected_keys.emplace_back("last_partial_update");
    manager->QueueKAnonymityUpdateForInterestGroup(group_key,
                                                   std::move(last_update));

    // Check that this test works (i.e. the query sets requests overlap).
    EXPECT_EQ(0u, delegate()->TakeQueryIDs().size());
    task_environment().FastForwardBy(base::Minutes(1));
    EXPECT_EQ(6u, delegate()->TakeQueryIDs().size());

    auto maybe_group = GetGroup(manager.get(), group.owner, group.name);
    ASSERT_TRUE(maybe_group);
    EXPECT_THAT(maybe_group.value()->hashed_kanon_keys,
                testing::UnorderedElementsAreArray(expected_keys));
    EXPECT_EQ(update_time, maybe_group.value()->last_k_anon_updated);
  }

  // Try two overlapping QuerySets calls for different interest groups. They
  // should both succeed.
  {
    InterestGroupKanonUpdateParameter kanon_update_group1(base::Time::Now() -
                                                          base::Days(2));
    kanon_update_group1.hashed_keys = {"group1", "123"};
    kanon_update_group1.newly_added_hashed_keys = {"group1"};
    InterestGroupKanonUpdateParameter kanon_update_group2(base::Time::Now() -
                                                          base::Days(2));
    kanon_update_group2.hashed_keys = {"group2", "123"};
    kanon_update_group2.newly_added_hashed_keys = {"123"};
    manager->QueueKAnonymityUpdateForInterestGroup(
        group_key, std::move(kanon_update_group1));
    update_time = base::Time::Now();
    task_environment().FastForwardBy(base::Milliseconds(1));
    base::Time update_time2 = base::Time::Now();
    manager->QueueKAnonymityUpdateForInterestGroup(
        blink::InterestGroupKey(group2.owner, group2.name),
        std::move(kanon_update_group2));

    // Check that this test works (i.e. the query sets requests overlap).
    EXPECT_EQ(0u, delegate()->TakeQueryIDs().size());
    task_environment().FastForwardBy(base::Minutes(1));
    EXPECT_EQ(2u, delegate()->TakeQueryIDs().size());

    auto maybe_group = GetGroup(manager.get(), group.owner, group.name);
    ASSERT_TRUE(maybe_group);
    EXPECT_THAT(maybe_group.value()->hashed_kanon_keys,
                testing::UnorderedElementsAre("group1", "123"));
    EXPECT_EQ(update_time, maybe_group.value()->last_k_anon_updated);

    auto maybe_group2 = GetGroup(manager.get(), group2.owner, group2.name);
    ASSERT_TRUE(maybe_group2);
    EXPECT_THAT(maybe_group2.value()->hashed_kanon_keys,
                testing::UnorderedElementsAre("group2", "123"));
    EXPECT_EQ(update_time2, maybe_group2.value()->last_k_anon_updated);
  }
}

TEST_F(InterestGroupKAnonymityManagerTestWithMock,
       QuerySetShouldHandleOutdatedOutstandingRequest) {
  auto manager = CreateManager(/*has_error=*/false,
                               /*query_sets_delay=*/base::Hours(10));
  const GURL top_frame = GURL("https://www.example.com/foo");
  const url::Origin owner = url::Origin::Create(top_frame);

  blink::InterestGroup group =
      CreateAndJoinInterestGroup(owner, "foo", manager.get(), top_frame);
  blink::InterestGroupKey group_key(group.owner, group.name);
  // Let the update triggered by JoinInterestGroup pass.
  task_environment().FastForwardBy(base::Hours(10));
  EXPECT_EQ(1u, delegate()->TakeQueryIDs().size());
  auto maybe_group = GetGroup(manager.get(), group.owner, group.name);
  ASSERT_TRUE(maybe_group);
  base::flat_set<std::string> pre_existing_kanon =
      maybe_group.value()->hashed_kanon_keys;
  EXPECT_FALSE(pre_existing_kanon.empty());

  // Make sure that if there is an outstanding k-anonymity update that has
  // been waiting for a very long time (possibly because it's failed),
  // we can continue with newer k-anonymity updates.
  InterestGroupKanonUpdateParameter kanon_update1(base::Time::Now() -
                                                  base::Minutes(1));
  kanon_update1.hashed_keys = {"update1", "new_update1"};
  kanon_update1.newly_added_hashed_keys = {"new_update1"};
  manager->QueueKAnonymityUpdateForInterestGroup(group_key,
                                                 std::move(kanon_update1));
  task_environment().FastForwardBy(kQueryInterval + base::Seconds(1));
  InterestGroupKanonUpdateParameter kanon_update2(base::Time::Now() -
                                                  base::Minutes(1));
  kanon_update2.hashed_keys = {"update2", "new_update2"};
  kanon_update2.newly_added_hashed_keys = {"new_update2"};
  EXPECT_EQ(0u, delegate()->TakeQueryIDs().size());
  manager->QueueKAnonymityUpdateForInterestGroup(group_key,
                                                 std::move(kanon_update2));
  task_environment().FastForwardBy(base::Hours(10));
  EXPECT_EQ(2u, delegate()->TakeQueryIDs().size());

  auto maybe_group_after_update =
      GetGroup(manager.get(), group.owner, group.name);
  ASSERT_TRUE(maybe_group_after_update);
  std::vector<std::string> expected_keys = {"new_update2"};
  expected_keys.insert(expected_keys.end(), pre_existing_kanon.begin(),
                       pre_existing_kanon.end());
  EXPECT_THAT(maybe_group_after_update.value()->hashed_kanon_keys,
              testing::UnorderedElementsAreArray(expected_keys));
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

  // No update has happened yet -- all keys are false.
  {
    auto maybe_group = GetGroup(manager.get(), owner, "foo");
    ASSERT_TRUE(maybe_group);
    EXPECT_EQ(0u, maybe_group.value()->hashed_kanon_keys.size());
  }

  // k-anonymity update happens here.
  task_environment().FastForwardBy(base::Minutes(1));

  // The queries should have been split into 1 group of the max size and 1 group
  // of the remaining one.
  std::vector<std::vector<std::string>> queried_batches =
      delegate()->TakeQueryIDs();
  ASSERT_EQ(2u, queried_batches.size());
  EXPECT_EQ(kQueryBatchSizeLimit, queried_batches[0].size());
  EXPECT_EQ(2u, queried_batches[1].size());

  // All keys are true.
  {
    auto maybe_group = GetGroup(manager.get(), owner, "foo");
    ASSERT_TRUE(maybe_group);
    EXPECT_EQ(base::Time::Now() - base::Minutes(1),
              maybe_group.value()->last_k_anon_updated);
    // There are two k-anon keys per ad, one bidding key and one reporting key.
    EXPECT_EQ(2 * kNumAds, maybe_group.value()->hashed_kanon_keys.size());
  }
}

}  // namespace content

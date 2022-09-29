// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_k_anonymity_manager.h"

#include "base/files/scoped_temp_dir.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/sequenced_task_runner_handle.h"
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
constexpr char kUpdateURL[] = "https://www.example.com/update";

class TestKAnonymityServiceDelegate : public KAnonymityServiceDelegate {
 public:
  TestKAnonymityServiceDelegate(bool has_error = false)
      : has_error_(has_error) {}

  void JoinSet(std::string id,
               base::OnceCallback<void(bool)> callback) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), has_error_));
  }

  void QuerySets(
      std::vector<std::string> ids,
      base::OnceCallback<void(std::vector<bool>)> callback) override {
    if (has_error_) {
      // An error is indicated by an empty status.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback), std::vector<bool>()));
    } else {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
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
  group.daily_update_url = GURL(kUpdateURL);
  group.ads.emplace();
  group.ads->push_back(blink::InterestGroup::Ad(GURL(kAdURL), /*metadata=*/""));
  EXPECT_TRUE(group.IsValid());
  return group;
}

}  // namespace

class InterestGroupKAnonymityManagerTest : public testing::Test {
 public:
  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  absl::optional<StorageInterestGroup> getGroup(
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

  absl::optional<base::Time> getLastReported(InterestGroupManagerImpl* manager,
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

 private:
  base::ScopedTempDir temp_directory_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestKAnonymityServiceDelegate> delegate_;
};

TEST_F(InterestGroupKAnonymityManagerTest,
       QueueUpdatePerformsQuerySetsForGroup) {
  auto manager = CreateManager();
  const GURL top_frame = GURL("https://www.example.com/foo");
  const url::Origin owner = url::Origin::Create(top_frame);
  const std::string name = "foo";

  EXPECT_FALSE(getGroup(manager.get(), owner, name));
  base::Time before_join = base::Time::Now();

  // Join queues the update, but returns first.
  manager->JoinInterestGroup(MakeInterestGroup(owner, "foo"), top_frame);
  auto maybe_group = getGroup(manager.get(), owner, name);
  ASSERT_TRUE(maybe_group);
  EXPECT_EQ(base::Time::Min(), maybe_group->name_kanon->last_updated);

  // k-anonymity update happens here.
  task_environment().FastForwardBy(base::Minutes(1));

  maybe_group = getGroup(manager.get(), owner, name);
  ASSERT_TRUE(maybe_group);
  base::Time last_updated = maybe_group->name_kanon->last_updated;
  EXPECT_LE(before_join, last_updated);
  EXPECT_GT(base::Time::Now(), last_updated);

  // Updated recently so we shouldn't update again.
  manager->QueueKAnonymityUpdateForInterestGroup(*maybe_group);
  task_environment().FastForwardBy(base::Minutes(1));

  maybe_group = getGroup(manager.get(), owner, name);
  ASSERT_TRUE(maybe_group);
  EXPECT_EQ(last_updated, maybe_group->name_kanon->last_updated);

  task_environment().FastForwardBy(kQueryInterval);

  // Updated more than 24 hours ago, so update.
  manager->QueueKAnonymityUpdateForInterestGroup(*maybe_group);
  task_environment().RunUntilIdle();
  maybe_group = getGroup(manager.get(), owner, name);
  ASSERT_TRUE(maybe_group);
  EXPECT_LT(last_updated, maybe_group->name_kanon->last_updated);
}

TEST_F(InterestGroupKAnonymityManagerTest, QueueUpdatePerformsJoinSetForGroup) {
  const GURL top_frame = GURL("https://www.example.com/foo");
  const url::Origin owner = url::Origin::Create(top_frame);
  const std::string name = "foo";

  std::string group_name_url = "https://www.example.com/\nfoo";
  std::string group_update_url = kUpdateURL;

  auto manager = CreateManager();
  EXPECT_FALSE(getLastReported(manager.get(), group_name_url));
  EXPECT_FALSE(getGroup(manager.get(), owner, name));
  base::Time before_join = base::Time::Now();

  // JoinInterestGroup should call QueueKAnonymityUpdateForInterestGroup.
  manager->JoinInterestGroup(MakeInterestGroup(owner, "foo"), top_frame);

  // k-anonymity update happens here.
  task_environment().FastForwardBy(base::Minutes(1));

  EXPECT_TRUE(getGroup(manager.get(), owner, name));

  absl::optional<base::Time> group_name_reported =
      getLastReported(manager.get(), group_name_url);
  ASSERT_TRUE(group_name_reported);
  EXPECT_LE(before_join, group_name_reported);

  absl::optional<base::Time> update_url_reported =
      getLastReported(manager.get(), kUpdateURL);
  ASSERT_TRUE(update_url_reported);
  EXPECT_LE(before_join, update_url_reported);

  auto maybe_group = getGroup(manager.get(), owner, name);
  ASSERT_TRUE(maybe_group);

  manager->QueueKAnonymityUpdateForInterestGroup(*maybe_group);

  // k-anonymity update would happen here.
  task_environment().FastForwardBy(base::Minutes(1));

  // Second update shouldn't change anything.
  EXPECT_EQ(group_name_reported,
            getLastReported(manager.get(), group_name_url));
  EXPECT_EQ(update_url_reported, getLastReported(manager.get(), kUpdateURL));

  task_environment().FastForwardBy(kJoinInterval);

  // Updated more than GetJoinInterval() ago, so update.
  manager->QueueKAnonymityUpdateForInterestGroup(*maybe_group);
  task_environment().RunUntilIdle();
  EXPECT_LT(update_url_reported, getLastReported(manager.get(), kUpdateURL));
}

TEST_F(InterestGroupKAnonymityManagerTest, RegisterAdAsWonPerformsJoinSet) {
  const GURL top_frame = GURL("https://www.example.com/foo");
  const url::Origin owner = url::Origin::Create(top_frame);
  const std::string name = "foo";

  auto manager = CreateManager();
  EXPECT_FALSE(getGroup(manager.get(), owner, name));
  EXPECT_FALSE(getLastReported(manager.get(), kAdURL));

  manager->JoinInterestGroup(MakeInterestGroup(owner, "foo"), top_frame);
  // The group *must* exist when JoinInterestGroup returns.
  ASSERT_TRUE(getGroup(manager.get(), owner, name));

  // k-anonymity would happens here.
  task_environment().FastForwardBy(base::Minutes(1));

  // Ads are *not* reported as part of joining an interest group.
  absl::optional<base::Time> reported = getLastReported(manager.get(), kAdURL);
  EXPECT_EQ(base::Time::Min(), reported);

  base::Time before_mark_ad = base::Time::Now();
  manager->RegisterAdAsWon(GURL(kAdURL));

  // k-anonymity update happens here.
  task_environment().FastForwardBy(base::Minutes(1));

  reported = getLastReported(manager.get(), kAdURL);
  EXPECT_LE(before_mark_ad, reported);
  ASSERT_TRUE(reported);
  base::Time last_reported = *reported;

  manager->RegisterAdAsWon(GURL(kAdURL));
  task_environment().FastForwardBy(base::Minutes(1));

  // Second update shouldn't have changed the update time (too recent).
  EXPECT_EQ(last_reported, getLastReported(manager.get(), kAdURL));

  task_environment().FastForwardBy(kJoinInterval);

  // Updated more than 24 hours ago, so update.
  manager->RegisterAdAsWon(GURL(kAdURL));
  task_environment().RunUntilIdle();
  EXPECT_LT(last_reported, getLastReported(manager.get(), kAdURL));
}

TEST_F(InterestGroupKAnonymityManagerTest, HandlesServerErrors) {
  const GURL top_frame = GURL("https://www.example.com/foo");
  const url::Origin owner = url::Origin::Create(top_frame);
  const std::string name = "foo";

  base::Time start_time = base::Time::Now();

  auto manager = CreateManager(/*has_error=*/true);
  manager->JoinInterestGroup(MakeInterestGroup(owner, "foo"), top_frame);
  // The group *must* exist when JoinInterestGroup returns.
  ASSERT_TRUE(getGroup(manager.get(), owner, name));

  // k-anonymity update happens here.
  task_environment().FastForwardBy(base::Minutes(1));

  // If the updates succeed then we normally would not record the update as
  // having been completed, so we would try it later.
  // For now we'll record the update as having been completed to to reduce
  // bandwidth and provide more accurate use counts.
  // When the server is actually implemented we'll need to change the expected
  // values below.

  absl::optional<base::Time> group_name_reported =
      getLastReported(manager.get(), kUpdateURL);
  ASSERT_TRUE(group_name_reported);

  // TODO(behamilton): Change this once we expect the server to be stable.
  EXPECT_LE(start_time, group_name_reported);
  // EXPECT_EQ(base::Time::Min(), group_name_reported);

  auto maybe_group = getGroup(manager.get(), owner, name);
  ASSERT_TRUE(maybe_group);

  // TODO(behamilton): Change this once we expect the server to be stable.
  EXPECT_LE(start_time, maybe_group->name_kanon->last_updated);
  // EXPECT_EQ(base::Time::Min(), maybe_group->name_kanon->last_updated);
}

}  // namespace content

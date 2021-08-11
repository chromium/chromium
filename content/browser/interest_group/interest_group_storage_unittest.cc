// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_storage.h"

#include <functional>
#include <memory>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "sql/database.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/origin.h"

namespace content {

using blink::InterestGroup;

class InterestGroupStorageTest : public testing::Test {
 public:
  InterestGroupStorageTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }

  std::unique_ptr<InterestGroupStorage> CreateStorage() {
    return std::make_unique<InterestGroupStorage>(temp_directory_.GetPath());
  }

  base::FilePath db_path() {
    return temp_directory_.GetPath().Append(
        FILE_PATH_LITERAL("InterestGroups"));
  }

  base::test::SingleThreadTaskEnvironment& task_environment() {
    return task_environment_;
  }

  InterestGroup NewInterestGroup(url::Origin owner, std::string name) {
    InterestGroup result;
    result.owner = owner;
    result.name = name;
    result.expiry = base::Time::Now() + base::TimeDelta::FromDays(30);
    return result;
  }

 private:
  base::ScopedTempDir temp_directory_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(InterestGroupStorageTest, DatabaseInitialized_CreateDatabase) {
  base::HistogramTester histograms;

  EXPECT_FALSE(base::PathExists(db_path()));

  { std::unique_ptr<InterestGroupStorage> storage = CreateStorage(); }

  // InterestGroupStorageSqlImpl opens the database lazily.
  EXPECT_FALSE(base::PathExists(db_path()));

  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    url::Origin test_origin =
        url::Origin::Create(GURL("https://owner.example.com"));
    storage->LeaveInterestGroup(test_origin, "example");
  }

  // InterestGroupStorage creates the database if it doesn't exist.
  EXPECT_TRUE(base::PathExists(db_path()));

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    // [interest_groups], [join_history], [bid_history], [win_history], [meta].
    EXPECT_EQ(4u, sql::test::CountSQLTables(&raw_db));
  }
}

TEST_F(InterestGroupStorageTest, DatabaseJoin) {
  url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  InterestGroup test_group = NewInterestGroup(test_origin, "example");
  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    storage->JoinInterestGroup(test_group, test_origin.GetURL());
  }
  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
    EXPECT_EQ(1u, origins.size());
    EXPECT_EQ(test_origin, origins[0]);
    std::vector<BiddingInterestGroup> interest_groups =
        storage->GetInterestGroupsForOwner(test_origin);
    EXPECT_EQ(1u, interest_groups.size());
    EXPECT_EQ(test_origin, interest_groups[0].group->group.owner);
    EXPECT_EQ("example", interest_groups[0].group->group.name);
    EXPECT_EQ(1, interest_groups[0].group->signals->join_count);
    EXPECT_EQ(0, interest_groups[0].group->signals->bid_count);
  }
}

// Test that joining and interest group twice increments the counter.
// Test that joining multiple interest groups with the same owner only creates a
// single distinct owner. Test that leaving one interest group does not affect
// membership of other interest groups by the same owner.
TEST_F(InterestGroupStorageTest, JoinJoinLeave) {
  url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"),
                             test_origin.GetURL());
  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"),
                             test_origin.GetURL());

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  std::vector<BiddingInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].group->group.name);
  EXPECT_EQ(2, interest_groups[0].group->signals->join_count);
  EXPECT_EQ(0, interest_groups[0].group->signals->bid_count);

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example2"),
                             test_origin.GetURL());

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(2u, interest_groups.size());

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  storage->LeaveInterestGroup(test_origin, "example");

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example2", interest_groups[0].group->group.name);
  EXPECT_EQ(1, interest_groups[0].group->signals->join_count);
  EXPECT_EQ(0, interest_groups[0].group->signals->bid_count);

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);
}

TEST_F(InterestGroupStorageTest, BidCount) {
  url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"),
                             test_origin.GetURL());

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  std::vector<BiddingInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].group->group.name);
  EXPECT_EQ(1, interest_groups[0].group->signals->join_count);
  EXPECT_EQ(0, interest_groups[0].group->signals->bid_count);

  storage->RecordInterestGroupBid(test_origin, "example");

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].group->group.name);
  EXPECT_EQ(1, interest_groups[0].group->signals->join_count);
  EXPECT_EQ(1, interest_groups[0].group->signals->bid_count);

  storage->RecordInterestGroupBid(test_origin, "example");

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].group->group.name);
  EXPECT_EQ(1, interest_groups[0].group->signals->join_count);
  EXPECT_EQ(2, interest_groups[0].group->signals->bid_count);
}

TEST_F(InterestGroupStorageTest, RecordsWins) {
  url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  GURL ad1_url = GURL("http://owner.example.com/ad1");
  GURL ad2_url = GURL("http://owner.example.com/ad2");
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"),
                             test_origin.GetURL());

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  std::vector<BiddingInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].group->group.name);
  EXPECT_EQ(1, interest_groups[0].group->signals->join_count);
  EXPECT_EQ(0, interest_groups[0].group->signals->bid_count);

  std::string ad1_json = "{url: '" + ad1_url.spec() + "'}";
  storage->RecordInterestGroupBid(test_origin, "example");
  storage->RecordInterestGroupWin(test_origin, "example", ad1_json);

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].group->group.name);
  EXPECT_EQ(1, interest_groups[0].group->signals->join_count);
  EXPECT_EQ(1, interest_groups[0].group->signals->bid_count);

  // Add the second win *after* the first so we can check ordering.
  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(1));
  std::string ad2_json = "{url: '" + ad2_url.spec() + "'}";
  storage->RecordInterestGroupBid(test_origin, "example");
  storage->RecordInterestGroupWin(test_origin, "example", ad2_json);

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].group->group.name);
  EXPECT_EQ(1, interest_groups[0].group->signals->join_count);
  EXPECT_EQ(2, interest_groups[0].group->signals->bid_count);
  EXPECT_EQ(2u, interest_groups[0].group->signals->prev_wins.size());
  // Ad wins should be listed in reverse chronological order.
  EXPECT_EQ(ad2_json, interest_groups[0].group->signals->prev_wins[0]->ad_json);
  EXPECT_EQ(ad1_json, interest_groups[0].group->signals->prev_wins[1]->ad_json);

  // Try delete
  storage->DeleteInterestGroupData(
      base::BindLambdaForTesting([&test_origin](const url::Origin& candidate) {
        return candidate == test_origin;
      }));

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(0u, origins.size());
}

TEST_F(InterestGroupStorageTest, StoresAllFields) {
  url::Origin partial_origin =
      url::Origin::Create(GURL("https://partial.example.com"));
  InterestGroup partial = NewInterestGroup(partial_origin, "partial");
  url::Origin full_origin =
      url::Origin::Create(GURL("https://full.example.com"));
  InterestGroup full;
  full.owner = full_origin;
  full.name = "full";
  full.expiry = base::Time::Now() + base::TimeDelta::FromDays(30);
  full.bidding_url = GURL("https://full.example.com/bid");
  full.update_url = GURL("https://full.example.com/update");
  full.trusted_bidding_signals_url = GURL("https://full.example.com/signals");
  full.trusted_bidding_signals_keys =
      absl::make_optional(std::vector<std::string>{"a", "b", "c", "d"});
  full.user_bidding_signals = "foo";
  full.ads.emplace();
  full.ads->emplace_back(blink::InterestGroup::Ad(
      GURL("https://full.example.com/ad1"), "metadata1"));
  full.ads->emplace_back(blink::InterestGroup::Ad(
      GURL("https://full.example.com/ad2"), "metadata2"));

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(partial, partial_origin.GetURL());
  storage->JoinInterestGroup(full, full_origin.GetURL());

  std::vector<BiddingInterestGroup> bidding_interest_groups =
      storage->GetInterestGroupsForOwner(partial_origin);
  ASSERT_EQ(1u, bidding_interest_groups.size());
  EXPECT_TRUE(
      partial.IsEqualForTesting(bidding_interest_groups[0].group->group));

  bidding_interest_groups = storage->GetInterestGroupsForOwner(full_origin);
  ASSERT_EQ(1u, bidding_interest_groups.size());
  EXPECT_TRUE(full.IsEqualForTesting(bidding_interest_groups[0].group->group));
}

TEST_F(InterestGroupStorageTest, DeleteOriginDeleteAll) {
  std::vector<::url::Origin> test_origins = {
      url::Origin::Create(GURL("https://owner.example.com")),
      url::Origin::Create(GURL("https://owner2.example.com")),
      url::Origin::Create(GURL("https://owner3.example.com")),
  };
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  for (const auto& origin : test_origins)
    storage->JoinInterestGroup(NewInterestGroup(origin, "example"),
                               origin.GetURL());

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(3u, origins.size());

  storage->DeleteInterestGroupData(
      base::BindLambdaForTesting([&test_origins](const url::Origin& origin) {
        return origin == test_origins[0];
      }));

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(2u, origins.size());

  storage->DeleteInterestGroupData({});

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(0u, origins.size());
}

TEST_F(InterestGroupStorageTest, DBMaintenanceExpiresOldInterestGroups) {
  url::Origin keep_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  std::vector<::url::Origin> test_origins = {
      url::Origin::Create(GURL("https://owner.example.com")),
      url::Origin::Create(GURL("https://owner2.example.com")),
      url::Origin::Create(GURL("https://owner3.example.com")),
  };

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  base::Time original_maintenance_time =
      storage->GetLastMaintenanceTimeForTesting();
  EXPECT_EQ(base::Time::Min(), original_maintenance_time);

  storage->JoinInterestGroup(NewInterestGroup(keep_origin, "keep"),
                             keep_origin.GetURL());
  for (const auto& origin : test_origins)
    storage->JoinInterestGroup(NewInterestGroup(origin, "discard"),
                               origin.GetURL());

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(3u, origins.size());

  std::vector<BiddingInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(keep_origin);
  EXPECT_EQ(2u, interest_groups.size());
  base::Time next_maintenance_time =
      base::Time::Now() + InterestGroupStorage::kIdlePeriod;

  //  Maintenance should not have run yet as we are not idle.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(),
            original_maintenance_time);

  task_environment().FastForwardBy(InterestGroupStorage::kIdlePeriod -
                                   base::TimeDelta::FromSeconds(1));

  //  Maintenance should not have run yet as we are not idle.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(),
            original_maintenance_time);

  task_environment().FastForwardBy(base::TimeDelta::FromSeconds(2));
  // Verify that maintenance has run.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(), next_maintenance_time);
  original_maintenance_time = storage->GetLastMaintenanceTimeForTesting();

  task_environment().FastForwardBy(InterestGroupStorage::kHistoryLength -
                                   base::TimeDelta::FromDays(1));
  // Verify that maintenance has not run. It's been long enough, but we haven't
  // made any calls.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(),
            original_maintenance_time);

  storage->JoinInterestGroup(NewInterestGroup(keep_origin, "keep"),
                             keep_origin.GetURL());
  next_maintenance_time = base::Time::Now() + InterestGroupStorage::kIdlePeriod;

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(3u, origins.size());

  interest_groups = storage->GetInterestGroupsForOwner(keep_origin);
  EXPECT_EQ(2u, interest_groups.size());

  //  Maintenance should not have run since we have not been idle.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(),
            original_maintenance_time);

  // Advance past expiration and check that since DB maintenance last ran before
  // the expiration the outdated entries are present but not reported.
  task_environment().FastForwardBy(base::TimeDelta::FromDays(1) +
                                   base::TimeDelta::FromSeconds(1));

  // Verify that maintenance has run.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(), next_maintenance_time);
  original_maintenance_time = storage->GetLastMaintenanceTimeForTesting();

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());

  interest_groups = storage->GetInterestGroupsForOwner(keep_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("keep", interest_groups[0].group->group.name);
  EXPECT_EQ(1, interest_groups[0].group->signals->join_count);
  EXPECT_EQ(0, interest_groups[0].group->signals->bid_count);
  next_maintenance_time = base::Time::Now() + InterestGroupStorage::kIdlePeriod;

  // All the groups should still be in the database since they shouldn't have
  // been cleaned up yet.
  interest_groups = storage->GetAllInterestGroupsUnfilteredForTesting();
  EXPECT_EQ(4u, interest_groups.size());

  // Wait an hour to perform DB maintenance.
  task_environment().FastForwardBy(InterestGroupStorage::kMaintenanceInterval);

  // Verify that maintenance has run.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(), next_maintenance_time);
  original_maintenance_time = storage->GetLastMaintenanceTimeForTesting();

  // Verify that the database only contains unexpired entries.
  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());

  interest_groups = storage->GetAllInterestGroupsUnfilteredForTesting();
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("keep", interest_groups[0].group->group.name);
  EXPECT_EQ(1, interest_groups[0].group->signals->join_count);
  EXPECT_EQ(0, interest_groups[0].group->signals->bid_count);
}

}  // namespace content

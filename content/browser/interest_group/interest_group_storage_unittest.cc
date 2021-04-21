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
#include "base/time/time.h"
#include "sql/database.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/origin.h"

namespace content {

using ::auction_worklet::mojom::BiddingInterestGroupPtr;
using ::blink::mojom::InterestGroupPtr;

namespace {
InterestGroupPtr NewInterestGroup(url::Origin owner, std::string name) {
  InterestGroupPtr result = blink::mojom::InterestGroup::New();
  result->owner = owner;
  result->name = name;
  return result;
}
}  // namespace

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

 private:
  base::ScopedTempDir temp_directory_;
};

TEST_F(InterestGroupStorageTest, DatabaseInitialized_CreateDatabase) {
  base::HistogramTester histograms;

  EXPECT_FALSE(base::PathExists(db_path()));

  { std::unique_ptr<InterestGroupStorage> storage = CreateStorage(); }

  // InterestGroupStorageSqlImpl opens the database lazily.
  EXPECT_FALSE(base::PathExists(db_path()));

  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    ::url::Origin test_origin =
        ::url::Origin::Create(GURL("https://owner.example.com"));
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
  InterestGroupPtr test_group = NewInterestGroup(test_origin, "example");
  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    storage->JoinInterestGroup(test_group.Clone());
  }
  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
    EXPECT_EQ(1u, origins.size());
    EXPECT_EQ(test_origin, origins[0]);
    std::vector<BiddingInterestGroupPtr> interest_groups =
        storage->GetInterestGroupsForOwner(test_origin);
    EXPECT_EQ(1u, interest_groups.size());
    EXPECT_EQ(test_origin, interest_groups[0]->group->owner);
    EXPECT_EQ("example", interest_groups[0]->group->name);
    EXPECT_EQ(1, interest_groups[0]->signals->join_count);
    EXPECT_EQ(0, interest_groups[0]->signals->bid_count);
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

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"));
  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"));

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  std::vector<BiddingInterestGroupPtr> interest_groups =
      storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0]->group->name);
  EXPECT_EQ(2, interest_groups[0]->signals->join_count);
  EXPECT_EQ(0, interest_groups[0]->signals->bid_count);

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example2"));

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(2u, interest_groups.size());

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  storage->LeaveInterestGroup(test_origin, "example");

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example2", interest_groups[0]->group->name);
  EXPECT_EQ(1, interest_groups[0]->signals->join_count);
  EXPECT_EQ(0, interest_groups[0]->signals->bid_count);

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);
}

TEST_F(InterestGroupStorageTest, BidCount) {
  url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"));

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  std::vector<BiddingInterestGroupPtr> interest_groups =
      storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0]->group->name);
  EXPECT_EQ(1, interest_groups[0]->signals->join_count);
  EXPECT_EQ(0, interest_groups[0]->signals->bid_count);

  storage->RecordInterestGroupBid(test_origin, "example");

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0]->group->name);
  EXPECT_EQ(1, interest_groups[0]->signals->join_count);
  EXPECT_EQ(1, interest_groups[0]->signals->bid_count);

  storage->RecordInterestGroupBid(test_origin, "example");

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0]->group->name);
  EXPECT_EQ(1, interest_groups[0]->signals->join_count);
  EXPECT_EQ(2, interest_groups[0]->signals->bid_count);
}

TEST_F(InterestGroupStorageTest, RecordsWins) {
  url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  GURL ad1_url = GURL("http://owner.example.com/ad1");
  GURL ad2_url = GURL("http://owner.example.com/ad2");
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"));

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  std::vector<BiddingInterestGroupPtr> interest_groups =
      storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0]->group->name);
  EXPECT_EQ(1, interest_groups[0]->signals->join_count);
  EXPECT_EQ(0, interest_groups[0]->signals->bid_count);

  std::string ad1_json = "{url: '" + ad1_url.spec() + "'}";
  storage->RecordInterestGroupBid(test_origin, "example");
  storage->RecordInterestGroupWin(test_origin, "example", ad1_json);

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0]->group->name);
  EXPECT_EQ(1, interest_groups[0]->signals->join_count);
  EXPECT_EQ(1, interest_groups[0]->signals->bid_count);

  std::string ad2_json = "{url: '" + ad2_url.spec() + "'}";
  storage->RecordInterestGroupBid(test_origin, "example");
  storage->RecordInterestGroupWin(test_origin, "example", ad2_json);

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0]->group->name);
  EXPECT_EQ(1, interest_groups[0]->signals->join_count);
  EXPECT_EQ(2, interest_groups[0]->signals->bid_count);
  EXPECT_EQ(2u, interest_groups[0]->signals->prev_wins.size());
  // Ad wins should be listed in reverse chronological order.
  EXPECT_EQ(ad2_json, interest_groups[0]->signals->prev_wins[0]->ad_json);
  EXPECT_EQ(ad1_json, interest_groups[0]->signals->prev_wins[1]->ad_json);

  // Try delete
  storage->DeleteInterestGroupData(test_origin);

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(0u, origins.size());
}

TEST_F(InterestGroupStorageTest, StoresAllFields) {
  url::Origin partial_origin =
      url::Origin::Create(GURL("https://partial.example.com"));
  InterestGroupPtr partial = NewInterestGroup(partial_origin, "partial");
  url::Origin full_origin =
      url::Origin::Create(GURL("https://full.example.com"));
  InterestGroupPtr full = blink::mojom::InterestGroup::New();
  full->owner = full_origin;
  full->name = "full";
  full->bidding_url = GURL("https://full.example.com/bid");
  full->update_url = GURL("https://full.example.com/update");
  full->trusted_bidding_signals_url = GURL("https://full.example.com/signals");
  full->trusted_bidding_signals_keys =
      base::make_optional(std::vector<std::string>{"a", "b", "c", "d"});
  full->user_bidding_signals = "foo";
  full->ads = std::vector<blink::mojom::InterestGroupAdPtr>();
  full->ads->emplace_back(blink::mojom::InterestGroupAd::New(
      GURL("https://full.example.com/ad1"), "metadata1"));
  full->ads->emplace_back(blink::mojom::InterestGroupAd::New(
      GURL("https://full.example.com/ad2"), "metadata2"));

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(partial.Clone());
  storage->JoinInterestGroup(full.Clone());

  std::vector<BiddingInterestGroupPtr> bidding_interest_groups =
      storage->GetInterestGroupsForOwner(partial_origin);
  ASSERT_EQ(1u, bidding_interest_groups.size());
  EXPECT_EQ(partial, bidding_interest_groups[0]->group);

  bidding_interest_groups = storage->GetInterestGroupsForOwner(full_origin);
  ASSERT_EQ(1u, bidding_interest_groups.size());
  EXPECT_EQ(full, bidding_interest_groups[0]->group);
}

TEST_F(InterestGroupStorageTest, DeleteOriginDeleteAll) {
  std::vector<::url::Origin> test_origins = {
      ::url::Origin::Create(GURL("https://owner.example.com")),
      ::url::Origin::Create(GURL("https://owner2.example.com")),
      ::url::Origin::Create(GURL("https://owner3.example.com")),
  };
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  for (const auto& origin : test_origins)
    storage->JoinInterestGroup(NewInterestGroup(origin, "example"));

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(3u, origins.size());

  storage->DeleteInterestGroupData(test_origins[0]);

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(2u, origins.size());

  storage->DeleteInterestGroupData(::url::Origin());

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(0u, origins.size());
}

}  // namespace content

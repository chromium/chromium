// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_storage.h"

#include <stddef.h>

#include <functional>
#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/origin.h"

namespace content {
namespace {

using blink::InterestGroup;
using testing::Field;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;
using SellerCapabilities = blink::SellerCapabilities;
using SellerCapabilitiesType = blink::SellerCapabilitiesType;

class InterestGroupStorageTest : public testing::Test {
 public:
  InterestGroupStorageTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kInterestGroupStorage,
        {{"max_owners", "10"},
         {"max_groups_per_owner", "10"},
         {"max_ops_before_maintenance", "100"},
         {"max_storage_per_owner", "2048"}});
  }

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
    result.bidding_url = owner.GetURL().Resolve("/bidding_script.js");
    result.expiry = base::Time::Now() + base::Days(30);
    result.execution_mode =
        blink::InterestGroup::ExecutionMode::kCompatibilityMode;
    return result;
  }

  // This test is in a helper function so that it can also be run after
  // UpgradeFromV6.
  void StoresAllFieldsTest() {
    const url::Origin partial_origin =
        url::Origin::Create(GURL("https://partial.example.com"));
    InterestGroup partial = NewInterestGroup(partial_origin, "partial");
    const url::Origin full_origin =
        url::Origin::Create(GURL("https://full.example.com"));
    InterestGroup full(
        /*expiry=*/base::Time::Now() + base::Days(30), /*owner=*/full_origin,
        /*name=*/"full", /*priority=*/1.0,
        /*enable_bidding_signals_prioritization=*/true,
        /*priority_vector=*/{{{"a", 2}, {"b", -2.2}}},
        /*priority_signals_overrides=*/{{{"a", -2}, {"c", 10}, {"d", 1.2}}},
        /*seller_capabilities=*/
        {{{full_origin, SellerCapabilities::kInterestGroupCounts},
          {partial_origin, SellerCapabilities::kLatencyStats}}},
        /*all_sellers_capabilities=*/
        {SellerCapabilities::kInterestGroupCounts,
         SellerCapabilities::kLatencyStats},
        /*execution_mode=*/InterestGroup::ExecutionMode::kCompatibilityMode,
        /*bidding_url=*/GURL("https://full.example.com/bid"),
        /*bidding_wasm_helper_url=*/GURL("https://full.example.com/bid_wasm"),
        /*update_url=*/GURL("https://full.example.com/update"),
        /*trusted_bidding_signals_url=*/
        GURL("https://full.example.com/signals"),
        /*trusted_bidding_signals_keys=*/
        std::vector<std::string>{"a", "b", "c", "d"},
        /*user_bidding_signals=*/"foo",
        /*ads=*/
        std::vector<InterestGroup::Ad>{
            blink::InterestGroup::Ad(GURL("https://full.example.com/ad1"),
                                     "metadata1", "group_1"),
            blink::InterestGroup::Ad(GURL("https://full.example.com/ad2"),
                                     "metadata2", "group_2")},
        /*ad_components=*/
        std::vector<InterestGroup::Ad>{
            blink::InterestGroup::Ad(
                GURL("https://full.example.com/adcomponent1"), "metadata1c",
                "group_1"),
            blink::InterestGroup::Ad(
                GURL("https://full.example.com/adcomponent2"), "metadata2c",
                "group_2")},
        /*ad_sizes=*/
        {{{"size_1", blink::AdSize(300, blink::AdSize::LengthUnit::kPixels, 150,
                                   blink::AdSize::LengthUnit::kPixels)},
          {"size_2", blink::AdSize(640, blink::AdSize::LengthUnit::kPixels, 480,
                                   blink::AdSize::LengthUnit::kPixels)},
          {"size_3",
           blink::AdSize(100, blink::AdSize::LengthUnit::kScreenWidth, 100,
                         blink::AdSize::LengthUnit::kScreenWidth)}}},
        /*size_groups=*/
        {{{"group_1", std::vector<std::string>{"size_1"}},
          {"group_2", std::vector<std::string>{"size_1", "size_2"}},
          {"group_3", std::vector<std::string>{"size_3"}}}});
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

    storage->JoinInterestGroup(partial, partial_origin.GetURL());
    storage->JoinInterestGroup(full, full_origin.GetURL());

    std::vector<StorageInterestGroup> storage_interest_groups =
        storage->GetInterestGroupsForOwner(partial_origin);
    ASSERT_EQ(1u, storage_interest_groups.size());
    EXPECT_TRUE(
        partial.IsEqualForTesting(storage_interest_groups[0].interest_group));

    storage_interest_groups = storage->GetInterestGroupsForOwner(full_origin);
    ASSERT_EQ(1u, storage_interest_groups.size());
    EXPECT_TRUE(
        full.IsEqualForTesting(storage_interest_groups[0].interest_group));
    base::Time join_time = base::Time::Now();
    EXPECT_EQ(storage_interest_groups[0].join_time, join_time);
    EXPECT_EQ(storage_interest_groups[0].last_updated, join_time);

    // Test update as well.

    // Pass time, so can check if `join_time` or `last_updated` is updated.
    task_environment().FastForwardBy(base::Seconds(1234));

    InterestGroupUpdate update;
    update.bidding_url = GURL("https://full.example.com/bid2");
    update.bidding_wasm_helper_url = GURL("https://full.example.com/bid_wasm2");
    update.trusted_bidding_signals_url =
        GURL("https://full.example.com/signals2");
    update.trusted_bidding_signals_keys =
        std::vector<std::string>{"a", "b2", "c", "d"};
    update.ads = full.ads;
    update.ads->emplace_back(GURL("https://full.example.com/ad3"), "metadata3",
                             "group_3");
    update.ad_components = full.ad_components;
    update.ad_components->emplace_back(
        GURL("https://full.example.com/adcomponent3"), "metadata3c", "group_3");
    storage->UpdateInterestGroup(blink::InterestGroupKey(full.owner, full.name),
                                 update);

    InterestGroup updated = full;
    updated.bidding_url = update.bidding_url;
    updated.bidding_wasm_helper_url = update.bidding_wasm_helper_url;
    updated.trusted_bidding_signals_url = update.trusted_bidding_signals_url;
    updated.trusted_bidding_signals_keys = update.trusted_bidding_signals_keys;
    updated.ads = update.ads;
    updated.ad_components = update.ad_components;

    storage_interest_groups = storage->GetInterestGroupsForOwner(full_origin);
    ASSERT_EQ(1u, storage_interest_groups.size());
    EXPECT_TRUE(
        updated.IsEqualForTesting(storage_interest_groups[0].interest_group));
    // `join_time` should not be modified be updates, but `last_updated` should
    // be.
    EXPECT_EQ(storage_interest_groups[0].join_time, join_time);
    EXPECT_EQ(storage_interest_groups[0].last_updated, base::Time::Now());
    // Make sure the clock was advanced.
    EXPECT_NE(storage_interest_groups[0].join_time,
              storage_interest_groups[0].last_updated);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir temp_directory_;
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(InterestGroupStorageTest, DatabaseInitialized_CreateDatabase) {
  EXPECT_FALSE(base::PathExists(db_path()));

  { std::unique_ptr<InterestGroupStorage> storage = CreateStorage(); }

  // InterestGroupStorageSqlImpl opens the database lazily.
  EXPECT_FALSE(base::PathExists(db_path()));

  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    const url::Origin test_origin =
        url::Origin::Create(GURL("https://owner.example.com"));
    storage->LeaveInterestGroup(blink::InterestGroupKey(test_origin, "example"),
                                test_origin);
  }

  // InterestGroupStorage creates the database if it doesn't exist.
  EXPECT_TRUE(base::PathExists(db_path()));

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    // [interest_groups], [join_history], [bid_history], [win_history],
    // [k_anon], [meta].
    EXPECT_EQ(6u, sql::test::CountSQLTables(&raw_db)) << raw_db.GetSchema();
  }
}

TEST_F(InterestGroupStorageTest, DatabaseRazesOldVersion) {
  ASSERT_FALSE(base::PathExists(db_path()));

  // Create an empty database with old schema version (version=1).
  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    sql::MetaTable meta_table;
    EXPECT_TRUE(
        meta_table.Init(&raw_db, /*version=*/1, /*compatible_version=*/1));

    EXPECT_EQ(1u, sql::test::CountSQLTables(&raw_db)) << raw_db.GetSchema();
  }

  EXPECT_TRUE(base::PathExists(db_path()));
  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    // We need to perform an interest group operation to trigger DB
    // initialization.
    const url::Origin test_origin =
        url::Origin::Create(GURL("https://owner.example.com"));
    storage->LeaveInterestGroup(blink::InterestGroupKey(test_origin, "example"),
                                test_origin);
  }

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    // [interest_groups], [join_history], [bid_history], [win_history],
    // [k_anon], [meta].
    EXPECT_EQ(6u, sql::test::CountSQLTables(&raw_db)) << raw_db.GetSchema();
  }
}

TEST_F(InterestGroupStorageTest, DatabaseRazesNewVersion) {
  ASSERT_FALSE(base::PathExists(db_path()));

  // Create an empty database with a newer schema version (version=1000000).
  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    sql::MetaTable meta_table;
    EXPECT_TRUE(meta_table.Init(&raw_db, /*version=*/1000000,
                                /*compatible_version=*/1000000));

    EXPECT_EQ(1u, sql::test::CountSQLTables(&raw_db)) << raw_db.GetSchema();
  }

  EXPECT_TRUE(base::PathExists(db_path()));
  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    // We need to perform an interest group operation to trigger DB
    // initialization.
    const url::Origin test_origin =
        url::Origin::Create(GURL("https://owner.example.com"));
    storage->LeaveInterestGroup(blink::InterestGroupKey(test_origin, "example"),
                                test_origin);
  }

  {
    sql::Database raw_db;
    EXPECT_TRUE(raw_db.Open(db_path()));

    // [interest_groups], [join_history], [bid_history], [win_history],
    // [k_anon], [meta].
    EXPECT_EQ(6u, sql::test::CountSQLTables(&raw_db)) << raw_db.GetSchema();
  }
}

TEST_F(InterestGroupStorageTest, DatabaseJoin) {
  base::HistogramTester histograms;
  const url::Origin test_origin =
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
    std::vector<StorageInterestGroup> interest_groups =
        storage->GetInterestGroupsForOwner(test_origin);
    EXPECT_EQ(1u, interest_groups.size());
    EXPECT_EQ(test_origin, interest_groups[0].interest_group.owner);
    EXPECT_EQ("example", interest_groups[0].interest_group.name);
    EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
    EXPECT_EQ(0, interest_groups[0].bidding_browser_signals->bid_count);
    EXPECT_EQ(interest_groups[0].joining_origin, test_origin);
    EXPECT_EQ(interest_groups[0].join_time, base::Time::Now());
    EXPECT_EQ(interest_groups[0].last_updated, base::Time::Now());
  }
  histograms.ExpectUniqueSample("Storage.InterestGroup.PerSiteCount", 1u, 1);
}

// Test that joining an interest group twice increments the counter.
// Test that joining multiple interest groups with the same owner only creates a
// single distinct owner. Test that leaving one interest group does not affect
// membership of other interest groups by the same owner.
TEST_F(InterestGroupStorageTest, JoinJoinLeave) {
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"),
                             test_origin.GetURL());
  base::RunLoop().RunUntilIdle();

  // Advance time so can verify that `join_time` and `last_updated` are updated.
  task_environment().FastForwardBy(base::Seconds(125));
  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"),
                             test_origin.GetURL());

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  std::vector<StorageInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].interest_group.name);
  EXPECT_EQ(2, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(0, interest_groups[0].bidding_browser_signals->bid_count);
  EXPECT_EQ(interest_groups[0].join_time, base::Time::Now());
  EXPECT_EQ(interest_groups[0].last_updated, base::Time::Now());

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example2"),
                             test_origin.GetURL());

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(2u, interest_groups.size());

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  storage->LeaveInterestGroup(blink::InterestGroupKey(test_origin, "example"),
                              test_origin);

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example2", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(0, interest_groups[0].bidding_browser_signals->bid_count);

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);
}

// Join 5 interest groups in the same origin, and one interest group in another
// origin.
//
// Fetch interest groups for update with a limit of 2 interest groups. Only 2
// interest groups should be returned, and they should all belong to the first
// test origin.
//
// Then, fetch 100 groups for update. Only 5 should be returned.
TEST_F(InterestGroupStorageTest, GetInterestGroupsForUpdate) {
  const url::Origin test_origin1 =
      url::Origin::Create(GURL("https://owner1.example.com"));
  const url::Origin test_origin2 =
      url::Origin::Create(GURL("https://owner2.example.com"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  constexpr size_t kNumOrigin1Groups = 5, kSmallFetchGroups = 2,
                   kLargeFetchGroups = 100;
  ASSERT_LT(kSmallFetchGroups, kNumOrigin1Groups);
  ASSERT_GT(kLargeFetchGroups, kNumOrigin1Groups);
  for (size_t i = 0; i < kNumOrigin1Groups; i++) {
    storage->JoinInterestGroup(
        NewInterestGroup(test_origin1,
                         base::StrCat({"example", base::NumberToString(i)})),
        test_origin1.GetURL());
  }
  storage->JoinInterestGroup(NewInterestGroup(test_origin2, "example"),
                             test_origin2.GetURL());

  std::vector<StorageInterestGroup> update_groups =
      storage->GetInterestGroupsForUpdate(test_origin1,
                                          /*groups_limit=*/kSmallFetchGroups);

  EXPECT_EQ(kSmallFetchGroups, update_groups.size());
  for (const auto& group : update_groups) {
    EXPECT_EQ(test_origin1, group.interest_group.owner);
  }

  update_groups =
      storage->GetInterestGroupsForUpdate(test_origin1,
                                          /*groups_limit=*/kLargeFetchGroups);

  EXPECT_EQ(kNumOrigin1Groups, update_groups.size());
  for (const auto& group : update_groups) {
    EXPECT_EQ(test_origin1, group.interest_group.owner);
  }
}

TEST_F(InterestGroupStorageTest, BidCount) {
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"),
                             test_origin.GetURL());
  blink::InterestGroupKey group_key(test_origin, "example");

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  std::vector<StorageInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(0, interest_groups[0].bidding_browser_signals->bid_count);

  storage->RecordInterestGroupBids({group_key});

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->bid_count);

  storage->RecordInterestGroupBids({group_key});

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(2, interest_groups[0].bidding_browser_signals->bid_count);
}

TEST_F(InterestGroupStorageTest, RecordsWins) {
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  const GURL ad1_url = GURL("http://owner.example.com/ad1");
  const GURL ad2_url = GURL("http://owner.example.com/ad2");
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  blink::InterestGroupKey group_key(test_origin, "example");

  storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"),
                             test_origin.GetURL());

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(test_origin, origins[0]);

  std::vector<StorageInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(0, interest_groups[0].bidding_browser_signals->bid_count);

  std::string ad1_json = "{url: '" + ad1_url.spec() + "'}";
  storage->RecordInterestGroupBids({group_key});
  storage->RecordInterestGroupWin(group_key, ad1_json);

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->bid_count);

  // Add the second win *after* the first so we can check ordering.
  task_environment().FastForwardBy(base::Seconds(1));
  std::string ad2_json = "{url: '" + ad2_url.spec() + "'}";
  storage->RecordInterestGroupBids({group_key});
  storage->RecordInterestGroupWin(group_key, ad2_json);

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, interest_groups.size());
  EXPECT_EQ("example", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(2, interest_groups[0].bidding_browser_signals->bid_count);
  EXPECT_EQ(2u, interest_groups[0].bidding_browser_signals->prev_wins.size());
  // Ad wins should be listed in reverse chronological order.
  EXPECT_EQ(ad2_json,
            interest_groups[0].bidding_browser_signals->prev_wins[0]->ad_json);
  EXPECT_EQ(ad1_json,
            interest_groups[0].bidding_browser_signals->prev_wins[1]->ad_json);

  // Try delete
  storage->DeleteInterestGroupData(base::BindLambdaForTesting(
      [&test_origin](const blink::StorageKey& candidate) {
        return candidate == blink::StorageKey::CreateFirstParty(test_origin);
      }));

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(0u, origins.size());
}

TEST_F(InterestGroupStorageTest, UpdatesAdKAnonymity) {
  url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  GURL ad1_url = GURL("https://owner.example.com/ad1");
  GURL ad2_url = GURL("https://owner.example.com/ad2");
  GURL ad3_url = GURL("https://owner.example.com/ad3");

  InterestGroup g = NewInterestGroup(test_origin, "name");
  g.ads.emplace();
  g.ads->push_back(blink::InterestGroup::Ad(ad1_url, "metadata1"));
  g.ads->push_back(blink::InterestGroup::Ad(ad2_url, "metadata2"));
  g.ad_components.emplace();
  g.ad_components->push_back(
      blink::InterestGroup::Ad(ad1_url, "component_metadata1"));
  g.ad_components->push_back(
      blink::InterestGroup::Ad(ad3_url, "component_metadata3"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  std::vector<StorageInterestGroup> groups =
      storage->GetInterestGroupsForOwner(test_origin);

  EXPECT_EQ(0u, groups.size());

  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join"));

  groups = storage->GetInterestGroupsForOwner(test_origin);

  std::vector<StorageInterestGroup::KAnonymityData> expected_bidding = {
      {blink::KAnonKeyForAdBid(g, ad1_url), false, base::Time::Min()},
      {blink::KAnonKeyForAdBid(g, ad2_url), false, base::Time::Min()},
  };
  std::vector<StorageInterestGroup::KAnonymityData> expected_component_ad = {
      {blink::KAnonKeyForAdComponentBid(ad1_url), false, base::Time::Min()},
      {blink::KAnonKeyForAdComponentBid(ad3_url), false, base::Time::Min()},
  };
  std::vector<StorageInterestGroup::KAnonymityData> expected_reporting = {
      {blink::KAnonKeyForAdNameReporting(g, g.ads.value()[0]), false,
       base::Time::Min()},
      {blink::KAnonKeyForAdNameReporting(g, g.ads.value()[1]), false,
       base::Time::Min()},
  };

  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].bidding_ads_kanon,
              testing::UnorderedElementsAreArray(expected_bidding));
  EXPECT_THAT(groups[0].component_ads_kanon,
              testing::UnorderedElementsAreArray(expected_component_ad));
  EXPECT_THAT(groups[0].reporting_ads_kanon,
              testing::UnorderedElementsAreArray(expected_reporting));

  base::Time update_time = base::Time::Now();
  StorageInterestGroup::KAnonymityData kanon_bid{
      blink::KAnonKeyForAdBid(g, ad1_url), true, update_time};
  StorageInterestGroup::KAnonymityData kanon_report{
      blink::KAnonKeyForAdNameReporting(g, g.ads.value()[0]), true,
      update_time};
  storage->UpdateKAnonymity(kanon_bid);
  storage->UpdateKAnonymity(kanon_report);
  expected_bidding[0] = kanon_bid;
  expected_reporting[0] = kanon_report;

  groups = storage->GetInterestGroupsForOwner(test_origin);

  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].bidding_ads_kanon,
              testing::UnorderedElementsAreArray(expected_bidding));
  EXPECT_THAT(groups[0].component_ads_kanon,
              testing::UnorderedElementsAreArray(expected_component_ad));
  EXPECT_THAT(groups[0].reporting_ads_kanon,
              testing::UnorderedElementsAreArray(expected_reporting));

  task_environment().FastForwardBy(base::Seconds(1));

  update_time = base::Time::Now();
  kanon_bid = StorageInterestGroup::KAnonymityData{
      blink::KAnonKeyForAdBid(g, ad2_url), true, update_time};
  StorageInterestGroup::KAnonymityData kanon_component{
      blink::KAnonKeyForAdComponentBid(ad3_url), true, update_time};
  kanon_report = StorageInterestGroup::KAnonymityData{
      blink::KAnonKeyForAdNameReporting(g, g.ads.value()[1]), true,
      update_time};
  storage->UpdateKAnonymity(kanon_bid);
  storage->UpdateKAnonymity(kanon_component);
  storage->UpdateKAnonymity(kanon_report);
  expected_bidding[1] = kanon_bid;
  expected_component_ad[1] = kanon_component;
  expected_reporting[1] = kanon_report;

  groups = storage->GetInterestGroupsForOwner(test_origin);

  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].bidding_ads_kanon,
              testing::UnorderedElementsAreArray(expected_bidding));
  EXPECT_THAT(groups[0].component_ads_kanon,
              testing::UnorderedElementsAreArray(expected_component_ad));
  EXPECT_THAT(groups[0].reporting_ads_kanon,
              testing::UnorderedElementsAreArray(expected_reporting));
}

TEST_F(InterestGroupStorageTest, KAnonDataExpires) {
  GURL update_url("https://owner.example.com/groupUpdate");
  url::Origin test_origin = url::Origin::Create(update_url);
  const std::string name = "name";
  const std::string key = test_origin.GetURL().spec() + '\n' + name;
  // We make the ad urls equal to the name key and update urls to verify the
  // database stores them separately.
  GURL ad1_url = GURL("https://owner.example.com/groupUpdate");
  GURL ad2_url = GURL("https://owner.example.com/name");

  InterestGroup g = NewInterestGroup(test_origin, name);
  g.ads.emplace();
  g.ads->push_back(blink::InterestGroup::Ad(ad1_url, "metadata1"));
  g.ad_components.emplace();
  g.ad_components->push_back(
      blink::InterestGroup::Ad(ad2_url, "component_metadata2"));
  g.update_url = update_url;
  g.expiry = base::Time::Now() + base::Days(1);

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join"));

  // Update the k-anonymity data.
  base::Time update_kanon_time = base::Time::Now();
  StorageInterestGroup::KAnonymityData ad1_bid_kanon{
      blink::KAnonKeyForAdBid(g, ad1_url), true, update_kanon_time};
  StorageInterestGroup::KAnonymityData ad1_report_kanon{
      blink::KAnonKeyForAdNameReporting(g, g.ads.value()[0]), true,
      update_kanon_time};
  StorageInterestGroup::KAnonymityData ad2_bid_kanon{
      blink::KAnonKeyForAdComponentBid(ad2_url), true, update_kanon_time};
  storage->UpdateKAnonymity(ad1_bid_kanon);
  storage->UpdateKAnonymity(ad1_report_kanon);
  storage->UpdateKAnonymity(ad2_bid_kanon);

  // Check k-anonymity data was correctly set.
  std::vector<StorageInterestGroup> groups =
      storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].bidding_ads_kanon,
              testing::UnorderedElementsAre(ad1_bid_kanon));
  EXPECT_THAT(groups[0].component_ads_kanon,
              testing::UnorderedElementsAre(ad2_bid_kanon));
  EXPECT_THAT(groups[0].reporting_ads_kanon,
              testing::UnorderedElementsAre(ad1_report_kanon));

  // Fast-forward past interest group expiration.
  task_environment().FastForwardBy(base::Days(2));

  // Interest group should no longer exist.
  groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(0u, groups.size());

  // Join again and expect the same kanon values.
  g.expiry = base::Time::Now() + base::Days(1);
  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join3"));

  // K-anon data should still be the same.
  groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].bidding_ads_kanon,
              testing::UnorderedElementsAre(ad1_bid_kanon));
  EXPECT_THAT(groups[0].component_ads_kanon,
              testing::UnorderedElementsAre(ad2_bid_kanon));
  EXPECT_THAT(groups[0].reporting_ads_kanon,
              testing::UnorderedElementsAre(ad1_report_kanon));

  // Fast-forward past interest group and kanon value expiration.
  task_environment().FastForwardBy(InterestGroupStorage::kHistoryLength);

  // Interest group should no longer exist.
  groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(0u, groups.size());

  // Allow enough idle time to trigger maintenance.
  task_environment().FastForwardBy(InterestGroupStorage::kIdlePeriod +
                                   base::Seconds(1));

  // Join again and expect the default kanon values.
  g.expiry = base::Time::Now() + base::Days(1);
  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join3"));

  // K-anon data should be the default.
  ad1_bid_kanon = {blink::KAnonKeyForAdBid(g, ad1_url),
                   /*is_k_anonymous=*/false, base::Time::Min()};
  ad1_report_kanon = {blink::KAnonKeyForAdNameReporting(g, g.ads.value()[0]),
                      /*is_k_anonymous=*/false, base::Time::Min()};
  ad2_bid_kanon = {blink::KAnonKeyForAdComponentBid(ad2_url),
                   /*is_k_anonymous=*/false, base::Time::Min()};
  groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].bidding_ads_kanon,
              testing::UnorderedElementsAre(ad1_bid_kanon));
  EXPECT_THAT(groups[0].component_ads_kanon,
              testing::UnorderedElementsAre(ad2_bid_kanon));
  EXPECT_THAT(groups[0].reporting_ads_kanon,
              testing::UnorderedElementsAre(ad1_report_kanon));
}

TEST_F(InterestGroupStorageTest, StoresAllFields) {
  StoresAllFieldsTest();
}

TEST_F(InterestGroupStorageTest, DeleteOriginDeleteAll) {
  const url::Origin owner_originA =
      url::Origin::Create(GURL("https://owner.example.com"));
  const url::Origin owner_originB =
      url::Origin::Create(GURL("https://owner2.example.com"));
  const url::Origin owner_originC =
      url::Origin::Create(GURL("https://owner3.example.com"));
  const url::Origin joining_originA =
      url::Origin::Create(GURL("https://joinerA.example.com"));
  const url::Origin joining_originB =
      url::Origin::Create(GURL("https://joinerB.example.com"));

  GURL ad1_url = GURL("https://owner.example.com/ad1");

  InterestGroup g1 = NewInterestGroup(owner_originA, "example");
  g1.ads.emplace();
  g1.ads->push_back(blink::InterestGroup::Ad(ad1_url, "metadata1"));

  std::string k_anon_key = blink::KAnonKeyForAdBid(g1, ad1_url);

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  storage->JoinInterestGroup(g1, joining_originA.GetURL());
  storage->JoinInterestGroup(NewInterestGroup(owner_originB, "example"),
                             joining_originA.GetURL());
  storage->JoinInterestGroup(NewInterestGroup(owner_originC, "example"),
                             joining_originA.GetURL());
  storage->JoinInterestGroup(NewInterestGroup(owner_originB, "exampleB"),
                             joining_originB.GetURL());
  storage->UpdateLastKAnonymityReported(k_anon_key);

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_THAT(origins, UnorderedElementsAre(owner_originA, owner_originB,
                                            owner_originC));
  std::vector<url::Origin> joining_origins =
      storage->GetAllInterestGroupJoiningOrigins();
  EXPECT_THAT(joining_origins,
              UnorderedElementsAre(joining_originA, joining_originB));

  storage->DeleteInterestGroupData(base::BindLambdaForTesting(
      [&owner_originA](const blink::StorageKey& storage_key) {
        return storage_key ==
               blink::StorageKey::CreateFirstParty(owner_originA);
      }));

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_THAT(origins, UnorderedElementsAre(owner_originB, owner_originC));
  joining_origins = storage->GetAllInterestGroupJoiningOrigins();
  EXPECT_THAT(joining_origins,
              UnorderedElementsAre(joining_originA, joining_originB));

  // Delete all interest groups that joined on joining_origin A. We expect that
  // we will be left with the one that joined on joining_origin B.
  storage->DeleteInterestGroupData(base::BindLambdaForTesting(
      [&joining_originA](const blink::StorageKey& storage_key) {
        return storage_key ==
               blink::StorageKey::CreateFirstParty(joining_originA);
      }));

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_THAT(origins, UnorderedElementsAre(owner_originB));
  joining_origins = storage->GetAllInterestGroupJoiningOrigins();
  EXPECT_THAT(joining_origins, UnorderedElementsAre(joining_originB));

  storage->DeleteInterestGroupData(base::NullCallback());

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(0u, origins.size());

  // DeleteInterestGroupData shouldn't have deleted kanon data.
  EXPECT_NE(base::Time::Min(), storage->GetLastKAnonymityReported(k_anon_key));

  storage->DeleteAllInterestGroupData();
  // DeleteAllInterestGroupData should have deleted *everything*.
  EXPECT_EQ(base::Time::Min(), storage->GetLastKAnonymityReported(k_anon_key));
}

TEST_F(InterestGroupStorageTest, DeleteOwnerJoinerPair) {
  // Set up owner origin, joining origin pairs.
  const url::Origin owner_originA =
      url::Origin::Create(GURL("https://ownerA.example.com"));
  const url::Origin owner_originB =
      url::Origin::Create(GURL("https://ownerB.example.com"));
  const url::Origin owner_originC =
      url::Origin::Create(GURL("https://ownerC.example.com"));
  const url::Origin joining_originA =
      url::Origin::Create(GURL("https://joinerA.example.com"));
  const url::Origin joining_originB =
      url::Origin::Create(GURL("https://joinerB.example.com"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  storage->JoinInterestGroup(NewInterestGroup(owner_originA, "groupA"),
                             joining_originA.GetURL());
  storage->JoinInterestGroup(NewInterestGroup(owner_originB, "groupB"),
                             joining_originA.GetURL());
  storage->JoinInterestGroup(NewInterestGroup(owner_originC, "groupC"),
                             joining_originA.GetURL());
  storage->JoinInterestGroup(NewInterestGroup(owner_originB, "groupB2"),
                             joining_originB.GetURL());

  // Validate that pairs are retrieved correctly.
  {
    std::vector<std::pair<url::Origin, url::Origin>>
        expected_owner_joiner_pairs = {{owner_originA, joining_originA},
                                       {owner_originB, joining_originA},
                                       {owner_originC, joining_originA},
                                       {owner_originB, joining_originB}};
    std::vector<std::pair<url::Origin, url::Origin>> owner_joiner_pairs =
        storage->GetAllInterestGroupOwnerJoinerPairs();
    EXPECT_THAT(owner_joiner_pairs,
                UnorderedElementsAreArray(expected_owner_joiner_pairs));
  }

  // Remove an interest group with specified owner origin and joining origin.
  storage->RemoveInterestGroupsMatchingOwnerAndJoiner(owner_originB,
                                                      joining_originA);

  // Validate that only the specified interest group is removed.
  {
    std::vector<std::pair<url::Origin, url::Origin>>
        expected_owner_joiner_pairs = {{owner_originA, joining_originA},
                                       {owner_originC, joining_originA},
                                       {owner_originB, joining_originB}};
    std::vector<std::pair<url::Origin, url::Origin>> owner_joiner_pairs =
        storage->GetAllInterestGroupOwnerJoinerPairs();
    EXPECT_THAT(owner_joiner_pairs,
                UnorderedElementsAreArray(expected_owner_joiner_pairs));
  }
}

// Maintenance should prune the number of interest groups and interest group
// owners based on the set limit.
TEST_F(InterestGroupStorageTest, JoinTooManyGroupNames) {
  base::HistogramTester histograms;
  const size_t kExcessOwners = 10;
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  const size_t max_groups_per_owner =
      blink::features::kInterestGroupStorageMaxGroupsPerOwner.Get();
  const size_t num_groups = max_groups_per_owner + kExcessOwners;
  std::vector<std::string> added_groups;

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  for (size_t i = 0; i < num_groups; i++) {
    const std::string group_name = base::NumberToString(i);
    // Allow time to pass so that they have different expiration times.
    // This makes which groups get removed deterministic as they are sorted by
    // expiration time.
    task_environment().FastForwardBy(base::Microseconds(1));

    storage->JoinInterestGroup(NewInterestGroup(test_origin, group_name),
                               test_origin.GetURL());
    added_groups.push_back(group_name);
  }

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());

  std::vector<StorageInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(test_origin);
  EXPECT_EQ(num_groups, interest_groups.size());
  histograms.ExpectBucketCount("Storage.InterestGroup.PerSiteCount", num_groups,
                               1);

  // Allow enough idle time to trigger maintenance.
  task_environment().FastForwardBy(InterestGroupStorage::kIdlePeriod +
                                   base::Seconds(1));

  interest_groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(max_groups_per_owner, interest_groups.size());
  histograms.ExpectBucketCount("Storage.InterestGroup.PerSiteCount",
                               max_groups_per_owner, 1);
  histograms.ExpectTotalCount("Storage.InterestGroup.PerSiteCount", 2);

  std::vector<std::string> remaining_groups;
  for (const auto& db_group : interest_groups) {
    remaining_groups.push_back(db_group.interest_group.name);
  }
  std::vector<std::string> remaining_groups_expected(
      added_groups.begin() + kExcessOwners, added_groups.end());
  EXPECT_THAT(remaining_groups,
              UnorderedElementsAreArray(remaining_groups_expected));
  histograms.ExpectTotalCount("Storage.InterestGroup.DBSize", 1);
  histograms.ExpectTotalCount("Storage.InterestGroup.DBMaintenanceTime", 1);
}

// Maintenance should prune groups when the interest group owner exceeds the
// storage size limit.
TEST_F(InterestGroupStorageTest, JoinTooMuchStorage) {
  base::HistogramTester histograms;
  const size_t kExcessGroups = 3;
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://owner.example.com"));
  const size_t kGroupSize = 800;
  const size_t groups_before_full =
      blink::features::kInterestGroupStorageMaxStoragePerOwner.Get() /
      kGroupSize;
  std::vector<std::string> added_groups;

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  for (size_t i = 0; i < groups_before_full + kExcessGroups; i++) {
    const std::string group_name = base::NumberToString(i);
    // Allow time to pass so that they have different expiration times.
    // This makes which groups get removed deterministic as they are sorted by
    // expiration time.
    task_environment().FastForwardBy(base::Microseconds(1));
    blink::InterestGroup group = NewInterestGroup(kTestOrigin, group_name);
    ASSERT_GT(kGroupSize, group.EstimateSize());
    group.user_bidding_signals =
        std::string(kGroupSize - group.EstimateSize(), 'P');
    EXPECT_EQ(kGroupSize, group.EstimateSize());

    storage->JoinInterestGroup(group, kTestOrigin.GetURL());
    added_groups.push_back(group_name);
  }

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());

  std::vector<StorageInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(kTestOrigin);
  EXPECT_EQ(added_groups.size(), interest_groups.size());

  // Allow enough idle time to trigger maintenance.
  task_environment().FastForwardBy(InterestGroupStorage::kIdlePeriod +
                                   base::Seconds(1));

  interest_groups = storage->GetInterestGroupsForOwner(kTestOrigin);
  ASSERT_EQ(groups_before_full, interest_groups.size());

  std::vector<std::string> remaining_groups;
  for (const auto& db_group : interest_groups) {
    remaining_groups.push_back(db_group.interest_group.name);
  }
  std::vector<std::string> remaining_groups_expected(
      added_groups.begin() + kExcessGroups, added_groups.end());
  EXPECT_THAT(remaining_groups,
              UnorderedElementsAreArray(remaining_groups_expected));
}

// Excess group owners should have their groups pruned by maintenance.
// In this test we trigger maintenance by having too many operations in a short
// period to test max_ops_before_maintenance_.
TEST_F(InterestGroupStorageTest, JoinTooManyGroupOwners) {
  const size_t kExcessGroups = 10;
  const size_t max_owners =
      blink::features::kInterestGroupStorageMaxOwners.Get();
  const size_t max_ops =
      blink::features::kInterestGroupStorageMaxOpsBeforeMaintenance.Get();
  const size_t num_groups = max_owners + kExcessGroups;
  std::vector<url::Origin> added_origins;

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  for (size_t i = 0; i < num_groups; i++) {
    const url::Origin test_origin = url::Origin::Create(GURL(
        base::StrCat({"https://", base::NumberToString(i), ".example.com/"})));

    // Allow time to pass so that they have different expiration times.
    // This makes which groups get removed deterministic as they are sorted by
    // expiration time.
    task_environment().FastForwardBy(base::Microseconds(1));

    storage->JoinInterestGroup(NewInterestGroup(test_origin, "example"),
                               test_origin.GetURL());
    added_origins.push_back(test_origin);
  }

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_THAT(origins, UnorderedElementsAreArray(added_origins));

  // Perform enough operations to trigger maintenance, without passing time.
  for (size_t i = 0; i < max_ops; i++) {
    // Any read-only operation will work here. This one should be fast.
    origins = storage->GetAllInterestGroupOwners();
  }

  // The oldest few interest groups should have been cleared during maintenance.
  origins = storage->GetAllInterestGroupOwners();
  std::vector<url::Origin> remaining_origins_expected(
      added_origins.begin() + kExcessGroups, added_origins.end());
  EXPECT_THAT(origins, UnorderedElementsAreArray(remaining_origins_expected));
}

TEST_F(InterestGroupStorageTest, DBMaintenanceExpiresOldInterestGroups) {
  base::HistogramTester histograms;

  const url::Origin keep_origin =
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

  std::vector<StorageInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(keep_origin);
  EXPECT_EQ(2u, interest_groups.size());
  base::Time next_maintenance_time =
      base::Time::Now() + InterestGroupStorage::kIdlePeriod;

  //  Maintenance should not have run yet as we are not idle.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(),
            original_maintenance_time);

  task_environment().FastForwardBy(InterestGroupStorage::kIdlePeriod -
                                   base::Seconds(1));

  //  Maintenance should not have run yet as we are not idle.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(),
            original_maintenance_time);

  task_environment().FastForwardBy(base::Seconds(2));
  // Verify that maintenance has run.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(), next_maintenance_time);
  original_maintenance_time = storage->GetLastMaintenanceTimeForTesting();
  histograms.ExpectTotalCount("Storage.InterestGroup.DBSize", 1);
  histograms.ExpectTotalCount("Storage.InterestGroup.DBMaintenanceTime", 1);

  task_environment().FastForwardBy(InterestGroupStorage::kHistoryLength -
                                   base::Days(1));
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
  task_environment().FastForwardBy(base::Days(1) + base::Seconds(1));

  // Verify that maintenance has run.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(), next_maintenance_time);
  original_maintenance_time = storage->GetLastMaintenanceTimeForTesting();
  histograms.ExpectTotalCount("Storage.InterestGroup.DBSize", 2);
  histograms.ExpectTotalCount("Storage.InterestGroup.DBMaintenanceTime", 2);

  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());

  interest_groups = storage->GetInterestGroupsForOwner(keep_origin);
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("keep", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(0, interest_groups[0].bidding_browser_signals->bid_count);
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
  histograms.ExpectTotalCount("Storage.InterestGroup.DBSize", 3);
  histograms.ExpectTotalCount("Storage.InterestGroup.DBMaintenanceTime", 3);

  // Verify that the database only contains unexpired entries.
  origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(1u, origins.size());

  interest_groups = storage->GetAllInterestGroupsUnfilteredForTesting();
  EXPECT_EQ(1u, interest_groups.size());
  EXPECT_EQ("keep", interest_groups[0].interest_group.name);
  EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
  EXPECT_EQ(0, interest_groups[0].bidding_browser_signals->bid_count);
}

// Upgrades a v6 database dump to an expected current database.
// The v6 database dump was extracted from the InterestGroups database in
// a browser profile by using `sqlite3 dump <path-to-database>` and then
// cleaning up and formatting the output.
TEST_F(InterestGroupStorageTest, UpgradeFromV6) {
  // Create V6 database from dump
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
  file_path =
      file_path.AppendASCII("content/test/data/interest_group/schemaV6.sql");
  ASSERT_TRUE(base::PathExists(file_path));
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(), file_path));

  auto expected_interest_group_matcher = testing::UnorderedElementsAre(
      testing::AllOf(
          Field(
              "interest_group", &StorageInterestGroup::interest_group,
              testing::AllOf(
                  Field("expiry", &InterestGroup::expiry,
                        base::Time::FromDeltaSinceWindowsEpoch(
                            base::Microseconds(13293932603076872))),
                  Field("owner", &InterestGroup::owner,
                        url::Origin::Create(GURL("https://owner.example.com"))),
                  Field("name", &InterestGroup::name, "group1"),
                  Field("priority", &InterestGroup::priority, 0.0),
                  Field("enable_bidding_signals_prioritization",
                        &InterestGroup::enable_bidding_signals_prioritization,
                        false),
                  Field("priority_vector", &InterestGroup::priority_vector,
                        absl::nullopt),
                  Field("priority_signals_overrides",
                        &InterestGroup::priority_signals_overrides,
                        absl::nullopt),
                  Field("seller_capabilities",
                        &InterestGroup::seller_capabilities, absl::nullopt),
                  Field("all_sellers_capabilities",
                        &InterestGroup::all_sellers_capabilities,
                        SellerCapabilitiesType()),
                  Field("bidding_url", &InterestGroup::bidding_url,
                        GURL("https://owner.example.com/bidder.js")),
                  Field("bidding_wasm_helper_url",
                        &InterestGroup::bidding_wasm_helper_url, absl::nullopt),
                  Field("update_url", &InterestGroup::update_url,
                        GURL("https://owner.example.com/update")),
                  Field("trusted_bidding_signals_url",
                        &InterestGroup::trusted_bidding_signals_url,
                        GURL("https://owner.example.com/signals")),
                  Field("trusted_bidding_signals_keys",
                        &InterestGroup::trusted_bidding_signals_keys,
                        std::vector<std::string>{"group1"}),
                  Field("user_bidding_signals",
                        &InterestGroup::user_bidding_signals,
                        "[[\"1\",\"2\"]]"),
                  Field("ads", &InterestGroup::ads,
                        testing::Property(
                            "value()",
                            &absl::optional<
                                std::vector<blink::InterestGroup::Ad>>::value,
                            testing::ElementsAre(testing::AllOf(
                                Field("render_url",
                                      &InterestGroup::Ad::render_url,
                                      GURL("https://ads.example.com/1")),
                                Field("metadata", &InterestGroup::Ad::metadata,
                                      "[\"4\",\"5\",null,\"6\"]"))))),
                  Field("ad_components", &InterestGroup::ad_components,
                        absl::nullopt),
                  Field("ad_sizes", &InterestGroup::ad_components,
                        absl::nullopt),
                  Field("size_groups", &InterestGroup::ad_components,
                        absl::nullopt))),
          Field(
              "bidding_browser_signals",
              &StorageInterestGroup::bidding_browser_signals,
              testing::AllOf(
                  Pointee(Field("join_count",
                                &auction_worklet::mojom::BiddingBrowserSignals::
                                    join_count,
                                5)),
                  Pointee(Field(
                      "bid_count",
                      &auction_worklet::mojom::BiddingBrowserSignals::bid_count,
                      4)))),
          Field("bidding_ads_kanon", &StorageInterestGroup::bidding_ads_kanon,
                testing::UnorderedElementsAre(
                    StorageInterestGroup::KAnonymityData{
                        "AdBid\n"
                        "https://owner.example.com/\n"
                        "https://owner.example.com/bidder.js\n"
                        "https://ads.example.com/1",
                        false, base::Time::Min()})),
          Field("reporting_ads_kanon",
                &StorageInterestGroup::reporting_ads_kanon,
                testing::UnorderedElementsAre(
                    StorageInterestGroup::KAnonymityData{
                        "NameReport\n"
                        "https://owner.example.com/\n"
                        "https://owner.example.com/bidder.js\n"
                        "https://ads.example.com/1\n"
                        "group1",
                        false, base::Time::Min()})),
          Field("joining_origin", &StorageInterestGroup::joining_origin,
                url::Origin::Create(GURL("https://publisher.example.com"))),
          Field("join_time", &StorageInterestGroup::join_time,
                base::Time::FromDeltaSinceWindowsEpoch(
                    base::Microseconds(13291340603081533))),
          Field("last_updated", &StorageInterestGroup::last_updated,
                base::Time::FromDeltaSinceWindowsEpoch(
                    base::Microseconds(13291340603081533)))),
      testing::AllOf(
          Field(
              "interest_group", &StorageInterestGroup::interest_group,
              testing::AllOf(
                  Field("expiry", &InterestGroup::expiry,
                        base::Time::FromDeltaSinceWindowsEpoch(
                            base::Microseconds(13293932603080090))),
                  Field("owner", &InterestGroup::owner,
                        url::Origin::Create(GURL("https://owner.example.com"))),
                  Field("name", &InterestGroup::name, "group2"),
                  Field("priority", &InterestGroup::priority, 0.0),
                  Field("enable_bidding_signals_prioritization",
                        &InterestGroup::enable_bidding_signals_prioritization,
                        false),
                  Field("priority_vector", &InterestGroup::priority_vector,
                        absl::nullopt),
                  Field("priority_signals_overrides",
                        &InterestGroup::priority_signals_overrides,
                        absl::nullopt),
                  Field("seller_capabilities",
                        &InterestGroup::seller_capabilities, absl::nullopt),
                  Field("all_sellers_capabilities",
                        &InterestGroup::all_sellers_capabilities,
                        SellerCapabilitiesType()),
                  Field("bidding_url", &InterestGroup::bidding_url,
                        GURL("https://owner.example.com/bidder.js")),
                  Field("bidding_wasm_helper_url",
                        &InterestGroup::bidding_wasm_helper_url, absl::nullopt),
                  Field("update_url", &InterestGroup::update_url,
                        GURL("https://owner.example.com/update")),
                  Field("trusted_bidding_signals_url",
                        &InterestGroup::trusted_bidding_signals_url,
                        GURL("https://owner.example.com/signals")),
                  Field("trusted_bidding_signals_keys",
                        &InterestGroup::trusted_bidding_signals_keys,
                        std::vector<std::string>{"group2"}),
                  Field("user_bidding_signals",
                        &InterestGroup::user_bidding_signals,
                        "[[\"1\",\"3\"]]"),
                  Field("ads", &InterestGroup::ads,
                        testing::Property(
                            "value()",
                            &absl::optional<
                                std::vector<blink::InterestGroup::Ad>>::value,
                            testing::ElementsAre(testing::AllOf(
                                Field("render_url",
                                      &InterestGroup::Ad::render_url,
                                      GURL("https://ads.example.com/1")),
                                Field("metadata", &InterestGroup::Ad::metadata,
                                      "[\"4\",\"5\",null,\"6\"]"))))),
                  Field("ad_components", &InterestGroup::ad_components,
                        absl::nullopt),
                  Field("ad_sizes", &InterestGroup::ad_components,
                        absl::nullopt),
                  Field("size_groups", &InterestGroup::ad_components,
                        absl::nullopt))),
          Field(
              "bidding_browser_signals",
              &StorageInterestGroup::bidding_browser_signals,
              testing::AllOf(
                  Pointee(Field("join_count",
                                &auction_worklet::mojom::BiddingBrowserSignals::
                                    join_count,
                                5)),
                  Pointee(Field(
                      "bid_count",
                      &auction_worklet::mojom::BiddingBrowserSignals::bid_count,
                      3)))),
          Field("bidding_ads_kanon", &StorageInterestGroup::bidding_ads_kanon,
                testing::UnorderedElementsAre(
                    StorageInterestGroup::KAnonymityData{
                        "AdBid\n"
                        "https://owner.example.com/\n"
                        "https://owner.example.com/bidder.js\n"
                        "https://ads.example.com/1",
                        false, base::Time::Min()})),
          Field("reporting_ads_kanon",
                &StorageInterestGroup::reporting_ads_kanon,
                testing::UnorderedElementsAre(
                    StorageInterestGroup::KAnonymityData{
                        "NameReport\n"
                        "https://owner.example.com/\n"
                        "https://owner.example.com/bidder.js\n"
                        "https://ads.example.com/1\n"
                        "group2",
                        false, base::Time::Min()})),
          Field("joining_origin", &StorageInterestGroup::joining_origin,
                url::Origin::Create(GURL("https://publisher.example.com"))),
          Field("join_time", &StorageInterestGroup::join_time,
                base::Time::FromDeltaSinceWindowsEpoch(
                    base::Microseconds(13291340603089914))),
          Field("last_updated", &StorageInterestGroup::last_updated,
                base::Time::FromDeltaSinceWindowsEpoch(
                    base::Microseconds(13291340603089914)))),
      testing::AllOf(
          Field(
              "interest_group", &StorageInterestGroup::interest_group,
              testing::AllOf(
                  Field("expiry", &InterestGroup::expiry,
                        base::Time::FromDeltaSinceWindowsEpoch(
                            base::Microseconds(13293932603052561))),
                  Field("owner", &InterestGroup::owner,
                        url::Origin::Create(GURL("https://owner.example.com"))),
                  Field("name", &InterestGroup::name, "group3"),
                  Field("priority", &InterestGroup::priority, 0.0),
                  Field("enable_bidding_signals_prioritization",
                        &InterestGroup::enable_bidding_signals_prioritization,
                        false),
                  Field("priority_vector", &InterestGroup::priority_vector,
                        absl::nullopt),
                  Field("priority_signals_overrides",
                        &InterestGroup::priority_signals_overrides,
                        absl::nullopt),
                  Field("seller_capabilities",
                        &InterestGroup::seller_capabilities, absl::nullopt),
                  Field("all_sellers_capabilities",
                        &InterestGroup::all_sellers_capabilities,
                        SellerCapabilitiesType()),
                  Field("bidding_url", &InterestGroup::bidding_url,
                        GURL("https://owner.example.com/bidder.js")),
                  Field("bidding_wasm_helper_url",
                        &InterestGroup::bidding_wasm_helper_url, absl::nullopt),
                  Field("update_url", &InterestGroup::update_url,
                        GURL("https://owner.example.com/update")),
                  Field("trusted_bidding_signals_url",
                        &InterestGroup::trusted_bidding_signals_url,
                        GURL("https://owner.example.com/signals")),
                  Field("trusted_bidding_signals_keys",
                        &InterestGroup::trusted_bidding_signals_keys,
                        std::vector<std::string>{"group3"}),
                  Field("user_bidding_signals",
                        &InterestGroup::user_bidding_signals,
                        "[[\"3\",\"2\"]]"),
                  Field("ads", &InterestGroup::ads,
                        testing::Property(
                            "value()",
                            &absl::optional<
                                std::vector<blink::InterestGroup::Ad>>::value,
                            testing::ElementsAre(testing::AllOf(
                                Field("render_url",
                                      &InterestGroup::Ad::render_url,
                                      GURL("https://ads.example.com/1")),
                                Field("metadata", &InterestGroup::Ad::metadata,
                                      "[\"4\",\"5\",null,\"6\"]"))))),
                  Field("ad_components", &InterestGroup::ad_components,
                        absl::nullopt),
                  Field("ad_sizes", &InterestGroup::ad_components,
                        absl::nullopt),
                  Field("size_groups", &InterestGroup::ad_components,
                        absl::nullopt))),
          Field(
              "bidding_browser_signals",
              &StorageInterestGroup::bidding_browser_signals,
              testing::AllOf(
                  Pointee(Field("join_count",
                                &auction_worklet::mojom::BiddingBrowserSignals::
                                    join_count,
                                4)),
                  Pointee(Field(
                      "bid_count",
                      &auction_worklet::mojom::BiddingBrowserSignals::bid_count,
                      4)))),
          Field("bidding_ads_kanon", &StorageInterestGroup::bidding_ads_kanon,
                testing::UnorderedElementsAre(
                    StorageInterestGroup::KAnonymityData{
                        "AdBid\n"
                        "https://owner.example.com/\n"
                        "https://owner.example.com/bidder.js\n"
                        "https://ads.example.com/1",
                        false, base::Time::Min()})),
          Field("reporting_ads_kanon",
                &StorageInterestGroup::reporting_ads_kanon,
                testing::UnorderedElementsAre(
                    StorageInterestGroup::KAnonymityData{
                        "NameReport\n"
                        "https://owner.example.com/\n"
                        "https://owner.example.com/bidder.js\n"
                        "https://ads.example.com/1\n"
                        "group3",
                        false, base::Time::Min()})),
          Field("joining_origin", &StorageInterestGroup::joining_origin,
                url::Origin::Create(GURL("https://publisher.example.com"))),
          Field("join_time", &StorageInterestGroup::join_time,
                base::Time::FromDeltaSinceWindowsEpoch(
                    base::Microseconds(13291340603098283))),
          Field("last_updated", &StorageInterestGroup::last_updated,
                base::Time::FromDeltaSinceWindowsEpoch(
                    base::Microseconds(13291340603098283)))));

  // Upgrade and read.
  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    ASSERT_TRUE(storage);

    std::vector<StorageInterestGroup> interest_groups =
        storage->GetAllInterestGroupsUnfilteredForTesting();

    EXPECT_THAT(interest_groups, expected_interest_group_matcher);
  }

  // Make sure the database still works if we open it again.
  {
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
    std::vector<StorageInterestGroup> interest_groups =
        storage->GetAllInterestGroupsUnfilteredForTesting();

    EXPECT_THAT(interest_groups, expected_interest_group_matcher);
  }
}

// Upgrades a v6 database dump to an expected current database, then attempts to
// insert new rows into the migrated database.
//
// The v6 database dump was extracted from the InterestGroups database in
// a browser profile by using `sqlite3 dump <path-to-database>` and then
// cleaning up and formatting the output.
TEST_F(InterestGroupStorageTest, UpgradeFromV6ThenAcceptNewData) {
  // Create V6 database from dump
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &file_path);
  file_path =
      file_path.AppendASCII("content/test/data/interest_group/schemaV6.sql");
  ASSERT_TRUE(base::PathExists(file_path));
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(), file_path));

  // Upgrade.
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  ASSERT_TRUE(storage);

  // Make sure the database can accept new data (including new fields) correctly
  // after the migration.
  StoresAllFieldsTest();
}

TEST_F(InterestGroupStorageTest,
       ClusteredGroupsClearedWhenClusterChangesOnJoin) {
  const url::Origin cluster_origin =
      url::Origin::Create(GURL("https://cluster.com/"));
  const url::Origin other_origin =
      url::Origin::Create(GURL("https://other.com/"));

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  ASSERT_TRUE(storage);

  const std::string clusters[] = {
      {"cluster0"}, {"cluster1"}, {"cluster2"}, {"cluster3"}};
  for (const auto& name : clusters) {
    blink::InterestGroup group = NewInterestGroup(cluster_origin, name);
    group.execution_mode =
        blink::InterestGroup::ExecutionMode::kGroupedByOriginMode;
    storage->JoinInterestGroup(group, cluster_origin.GetURL());
  }
  storage->JoinInterestGroup(NewInterestGroup(cluster_origin, "separate_same"),
                             cluster_origin.GetURL());
  storage->JoinInterestGroup(NewInterestGroup(other_origin, "separate_other"),
                             other_origin.GetURL());
  {
    blink::InterestGroup group =
        NewInterestGroup(cluster_origin, "clustered_different");
    group.execution_mode =
        blink::InterestGroup::ExecutionMode::kGroupedByOriginMode;
    storage->JoinInterestGroup(group, other_origin.GetURL());
  }
  {
    blink::InterestGroup group =
        NewInterestGroup(other_origin, "clustered_other");
    group.execution_mode =
        blink::InterestGroup::ExecutionMode::kGroupedByOriginMode;
    storage->JoinInterestGroup(group, other_origin.GetURL());
  }

  std::vector<StorageInterestGroup> interest_groups =
      storage->GetAllInterestGroupsUnfilteredForTesting();
  EXPECT_EQ(8u, interest_groups.size());

  {
    blink::InterestGroup group = NewInterestGroup(cluster_origin, "cluster0");
    group.execution_mode =
        blink::InterestGroup::ExecutionMode::kGroupedByOriginMode;
    storage->JoinInterestGroup(group, other_origin.GetURL());
  }

  auto expected_interest_group_matcher = testing::UnorderedElementsAre(
      testing::AllOf(
          Field("joining_origin", &StorageInterestGroup::joining_origin,
                cluster_origin),
          Field("interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(
                    Field("owner", &InterestGroup::owner, cluster_origin),
                    Field("name", &InterestGroup::name, "separate_same"),
                    Field("execution_mode", &InterestGroup::execution_mode,
                          blink::InterestGroup::ExecutionMode::
                              kCompatibilityMode)))),
      testing::AllOf(
          Field("joining_origin", &StorageInterestGroup::joining_origin,
                other_origin),
          Field("interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(
                    Field("owner", &InterestGroup::owner, other_origin),
                    Field("name", &InterestGroup::name, "separate_other"),
                    Field("execution_mode", &InterestGroup::execution_mode,
                          blink::InterestGroup::ExecutionMode::
                              kCompatibilityMode)))),
      testing::AllOf(
          Field("joining_origin", &StorageInterestGroup::joining_origin,
                other_origin),
          Field("interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(
                    Field("owner", &InterestGroup::owner, cluster_origin),
                    Field("name", &InterestGroup::name, "clustered_different"),
                    Field("execution_mode", &InterestGroup::execution_mode,
                          blink::InterestGroup::ExecutionMode::
                              kGroupedByOriginMode)))),
      testing::AllOf(
          Field("joining_origin", &StorageInterestGroup::joining_origin,
                other_origin),
          Field("interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(
                    Field("owner", &InterestGroup::owner, other_origin),
                    Field("name", &InterestGroup::name, "clustered_other"),
                    Field("execution_mode", &InterestGroup::execution_mode,
                          blink::InterestGroup::ExecutionMode::
                              kGroupedByOriginMode)))),
      testing::AllOf(
          Field("joining_origin", &StorageInterestGroup::joining_origin,
                other_origin),
          Field("interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(
                    Field("owner", &InterestGroup::owner, cluster_origin),
                    Field("name", &InterestGroup::name, "cluster0"),
                    Field("execution_mode", &InterestGroup::execution_mode,
                          blink::InterestGroup::ExecutionMode::
                              kGroupedByOriginMode)))));
  interest_groups = storage->GetAllInterestGroupsUnfilteredForTesting();
  EXPECT_THAT(interest_groups, expected_interest_group_matcher);
}

TEST_F(InterestGroupStorageTest,
       ClusteredGroupsClearedWhenClusterChangesOnLeave) {
  const url::Origin cluster_origin =
      url::Origin::Create(GURL("https://cluster.com/"));
  const url::Origin other_origin =
      url::Origin::Create(GURL("https://other.com/"));

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  ASSERT_TRUE(storage);

  const std::string clusters[] = {
      {"cluster0"}, {"cluster1"}, {"cluster2"}, {"cluster3"}};
  for (const auto& name : clusters) {
    blink::InterestGroup group = NewInterestGroup(cluster_origin, name);
    group.execution_mode =
        blink::InterestGroup::ExecutionMode::kGroupedByOriginMode;
    storage->JoinInterestGroup(group, cluster_origin.GetURL());
  }
  storage->JoinInterestGroup(NewInterestGroup(cluster_origin, "separate_same"),
                             cluster_origin.GetURL());
  storage->JoinInterestGroup(NewInterestGroup(other_origin, "separate_other"),
                             other_origin.GetURL());

  {
    blink::InterestGroup group =
        NewInterestGroup(cluster_origin, "clustered_different");
    group.execution_mode =
        blink::InterestGroup::ExecutionMode::kGroupedByOriginMode;
    storage->JoinInterestGroup(group, other_origin.GetURL());
  }

  {
    blink::InterestGroup group =
        NewInterestGroup(other_origin, "clustered_other");
    group.execution_mode =
        blink::InterestGroup::ExecutionMode::kGroupedByOriginMode;
    storage->JoinInterestGroup(group, other_origin.GetURL());
  }

  std::vector<StorageInterestGroup> interest_groups =
      storage->GetAllInterestGroupsUnfilteredForTesting();
  EXPECT_EQ(8u, interest_groups.size());

  storage->LeaveInterestGroup(
      blink::InterestGroupKey(cluster_origin, "cluster0"), other_origin);

  auto expected_interest_group_matcher = testing::UnorderedElementsAre(
      testing::AllOf(
          Field("joining_origin", &StorageInterestGroup::joining_origin,
                cluster_origin),
          Field("interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(
                    Field("owner", &InterestGroup::owner, cluster_origin),
                    Field("name", &InterestGroup::name, "separate_same"),
                    Field("execution_mode", &InterestGroup::execution_mode,
                          blink::InterestGroup::ExecutionMode::
                              kCompatibilityMode)))),
      testing::AllOf(
          Field("joining_origin", &StorageInterestGroup::joining_origin,
                other_origin),
          Field("interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(
                    Field("owner", &InterestGroup::owner, other_origin),
                    Field("name", &InterestGroup::name, "separate_other"),
                    Field("execution_mode", &InterestGroup::execution_mode,
                          blink::InterestGroup::ExecutionMode::
                              kCompatibilityMode)))),
      testing::AllOf(
          Field("joining_origin", &StorageInterestGroup::joining_origin,
                other_origin),
          Field("interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(
                    Field("owner", &InterestGroup::owner, cluster_origin),
                    Field("name", &InterestGroup::name, "clustered_different"),
                    Field("execution_mode", &InterestGroup::execution_mode,
                          blink::InterestGroup::ExecutionMode::
                              kGroupedByOriginMode)))),
      testing::AllOf(
          Field("joining_origin", &StorageInterestGroup::joining_origin,
                other_origin),
          Field("interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(
                    Field("owner", &InterestGroup::owner, other_origin),
                    Field("name", &InterestGroup::name, "clustered_other"),
                    Field("execution_mode", &InterestGroup::execution_mode,
                          blink::InterestGroup::ExecutionMode::
                              kGroupedByOriginMode)))));

  interest_groups = storage->GetAllInterestGroupsUnfilteredForTesting();
  EXPECT_THAT(interest_groups, expected_interest_group_matcher);
}

TEST_F(InterestGroupStorageTest, SetGetLastKAnonReported) {
  url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  GURL ad1_url = GURL("https://owner.example.com/ad1");
  GURL ad2_url = GURL("https://owner.example.com/ad2");
  GURL ad3_url = GURL("https://owner.example.com/ad3");

  InterestGroup g = NewInterestGroup(test_origin, "name");
  g.ads.emplace();
  g.ads->push_back(blink::InterestGroup::Ad(ad1_url, "metadata1"));
  g.ads->push_back(blink::InterestGroup::Ad(ad2_url, "metadata2"));
  g.ad_components.emplace();
  g.ad_components->push_back(
      blink::InterestGroup::Ad(ad1_url, "component_metadata1"));
  g.ad_components->push_back(
      blink::InterestGroup::Ad(ad3_url, "component_metadata3"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  absl::optional<base::Time> last_report =
      storage->GetLastKAnonymityReported(ad1_url.spec());
  EXPECT_EQ(base::Time::Min(), last_report);  // Not in the database.

  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join"));

  base::Time expected_last_report;
  last_report = storage->GetLastKAnonymityReported(ad1_url.spec());
  EXPECT_EQ(last_report, base::Time::Min());
  storage->UpdateLastKAnonymityReported(ad1_url.spec());
  expected_last_report = base::Time::Now();

  task_environment().FastForwardBy(base::Seconds(1));

  last_report = storage->GetLastKAnonymityReported(ad1_url.spec());
  EXPECT_EQ(last_report, expected_last_report);

  task_environment().FastForwardBy(base::Seconds(1));

  last_report = storage->GetLastKAnonymityReported(ad2_url.spec());
  EXPECT_EQ(last_report, base::Time::Min());
  storage->UpdateLastKAnonymityReported(ad2_url.spec());
  expected_last_report = base::Time::Now();

  task_environment().FastForwardBy(base::Seconds(1));

  last_report = storage->GetLastKAnonymityReported(ad2_url.spec());
  EXPECT_EQ(last_report, expected_last_report);

  task_environment().FastForwardBy(base::Seconds(1));

  last_report = storage->GetLastKAnonymityReported(ad3_url.spec());
  EXPECT_EQ(last_report, base::Time::Min());
  storage->UpdateLastKAnonymityReported(ad3_url.spec());
  expected_last_report = base::Time::Now();

  task_environment().FastForwardBy(base::Seconds(1));

  last_report = storage->GetLastKAnonymityReported(ad3_url.spec());
  EXPECT_EQ(last_report, expected_last_report);

  task_environment().FastForwardBy(base::Seconds(1));

  std::string group_name_key = test_origin.GetURL().spec() + "\nname";
  last_report = storage->GetLastKAnonymityReported(group_name_key);
  EXPECT_EQ(last_report, base::Time::Min());
  storage->UpdateLastKAnonymityReported(group_name_key);
  expected_last_report = base::Time::Now();

  task_environment().FastForwardBy(base::Seconds(1));

  last_report = storage->GetLastKAnonymityReported(group_name_key);
  EXPECT_EQ(last_report, expected_last_report);
}

TEST_F(InterestGroupStorageTest, UpdatePrioritySignalsOverrides) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://example.test"));
  const char kName[] = "Name";
  const blink::InterestGroupKey kInterestGroupKey(kOrigin, kName);

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  // Join a group without any priority signals overrides.
  InterestGroup original_group = NewInterestGroup(kOrigin, kName);
  // Set priority vector, so can make sure it's never modified.
  original_group.priority_vector = {{"key1", 0}};
  storage->JoinInterestGroup(original_group,
                             /*main_frame_joining_url=*/kOrigin.GetURL());
  std::vector<StorageInterestGroup> storage_interest_groups =
      storage->GetInterestGroupsForOwner(kOrigin);
  ASSERT_EQ(1u, storage_interest_groups.size());
  EXPECT_TRUE(original_group.IsEqualForTesting(
      storage_interest_groups[0].interest_group));

  // Updating a group that has no overrides should add an overrides maps and set
  // the corresponding keys.
  base::flat_map<std::string, auction_worklet::mojom::PrioritySignalsDoublePtr>
      update1;
  // Can't put these in an inlined initializer list, since those must use
  // copyable types.
  update1.emplace("key1",
                  auction_worklet::mojom::PrioritySignalsDouble::New(0));
  update1.emplace("key2", auction_worklet::mojom::PrioritySignalsDoublePtr());
  update1.emplace("key3",
                  auction_worklet::mojom::PrioritySignalsDouble::New(-4));
  update1.emplace("key4",
                  auction_worklet::mojom::PrioritySignalsDouble::New(5));
  storage->UpdateInterestGroupPriorityOverrides(kInterestGroupKey,
                                                std::move(update1));
  storage_interest_groups = storage->GetInterestGroupsForOwner(kOrigin);
  ASSERT_EQ(1u, storage_interest_groups.size());
  EXPECT_EQ(kOrigin, storage_interest_groups[0].interest_group.owner);
  EXPECT_EQ(kName, storage_interest_groups[0].interest_group.name);
  EXPECT_EQ(original_group.priority_vector,
            storage_interest_groups[0].interest_group.priority_vector);
  EXPECT_EQ(
      (base::flat_map<std::string, double>{
          {"key1", 0}, {"key3", -4}, {"key4", 5}}),
      storage_interest_groups[0].interest_group.priority_signals_overrides);

  // Updating a group that has overrides should modify the existing overrides.
  base::flat_map<std::string, auction_worklet::mojom::PrioritySignalsDoublePtr>
      update2;
  // Can't put these in an inlined initializer list, since those must use
  // copyable types.
  update2.emplace("key1", auction_worklet::mojom::PrioritySignalsDoublePtr());
  update2.emplace("key2",
                  auction_worklet::mojom::PrioritySignalsDouble::New(6));
  update2.emplace("key3",
                  auction_worklet::mojom::PrioritySignalsDouble::New(0));
  storage->UpdateInterestGroupPriorityOverrides(kInterestGroupKey,
                                                std::move(update2));
  storage_interest_groups = storage->GetInterestGroupsForOwner(kOrigin);
  ASSERT_EQ(1u, storage_interest_groups.size());
  EXPECT_EQ(kOrigin, storage_interest_groups[0].interest_group.owner);
  EXPECT_EQ(kName, storage_interest_groups[0].interest_group.name);
  EXPECT_EQ(original_group.priority_vector,
            storage_interest_groups[0].interest_group.priority_vector);
  EXPECT_EQ(
      (base::flat_map<std::string, double>{
          {"key2", 6}, {"key3", 0}, {"key4", 5}}),
      storage_interest_groups[0].interest_group.priority_signals_overrides);

  // Try and set overrides to make an InterestGroup that is too big. Update
  // should fail, and the InterestGroup should be unmodified.
  std::vector<
      std::pair<std::string, auction_worklet::mojom::PrioritySignalsDoublePtr>>
      overrides_too_big_vector;
  for (size_t i = 0; i < blink::mojom::kMaxInterestGroupSize / sizeof(double);
       ++i) {
    overrides_too_big_vector.emplace_back(
        base::NumberToString(i),
        auction_worklet::mojom::PrioritySignalsDouble::New(i));
  }
  storage->UpdateInterestGroupPriorityOverrides(
      kInterestGroupKey,
      base::flat_map<std::string,
                     auction_worklet::mojom::PrioritySignalsDoublePtr>(
          std::move(overrides_too_big_vector)));
  storage_interest_groups = storage->GetInterestGroupsForOwner(kOrigin);
  ASSERT_EQ(1u, storage_interest_groups.size());
  EXPECT_EQ(kOrigin, storage_interest_groups[0].interest_group.owner);
  EXPECT_EQ(kName, storage_interest_groups[0].interest_group.name);
  EXPECT_EQ(original_group.priority_vector,
            storage_interest_groups[0].interest_group.priority_vector);
  EXPECT_EQ(
      (base::flat_map<std::string, double>{
          {"key2", 6}, {"key3", 0}, {"key4", 5}}),
      storage_interest_groups[0].interest_group.priority_signals_overrides);
}

TEST_F(InterestGroupStorageTest, OnlyDeletesExpiredKAnon) {
  url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  GURL ad1_url = GURL("https://owner.example.com/ad1");
  GURL ad2_url = GURL("https://owner.example.com/ad2");

  InterestGroup g = NewInterestGroup(test_origin, "name");
  g.ads.emplace();
  g.ads->push_back(blink::InterestGroup::Ad(ad1_url, "metadata1"));
  g.ads->push_back(blink::InterestGroup::Ad(ad2_url, "metadata2"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join"));

  std::vector<StorageInterestGroup> groups =
      storage->GetInterestGroupsForOwner(test_origin);

  storage->UpdateLastKAnonymityReported(ad1_url.spec());
  storage->UpdateLastKAnonymityReported(ad2_url.spec());

  EXPECT_NE(base::Time::Min(),
            storage->GetLastKAnonymityReported(ad1_url.spec()));
  EXPECT_NE(base::Time::Min(),
            storage->GetLastKAnonymityReported(ad2_url.spec()));

  task_environment().FastForwardBy(base::Days(1));

  // fast-forward 30 days.
  for (int i = 0; i < InterestGroupStorage::kHistoryLength / base::Days(1);
       i++) {
    storage->JoinInterestGroup(g, GURL("https://owner.example.com/join"));
    storage->UpdateLastKAnonymityReported(ad1_url.spec());
    task_environment().FastForwardBy(base::Days(1));
  }

  EXPECT_NE(base::Time::Min(),
            storage->GetLastKAnonymityReported(ad1_url.spec()));
  EXPECT_EQ(base::Time::Min(),
            storage->GetLastKAnonymityReported(ad2_url.spec()));
}

}  // namespace
}  // namespace content

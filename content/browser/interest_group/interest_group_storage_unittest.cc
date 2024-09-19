// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_storage.h"

#include <stddef.h>

#include <functional>
#include <memory>
#include <optional>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"
#include "content/browser/interest_group/for_debugging_only_report_util.h"
#include "content/browser/interest_group/interest_group_update.h"
#include "content/browser/interest_group/storage_interest_group.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "crypto/sha2.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/interest_group/test/interest_group_test_utils.h"
#include "third_party/blink/public/common/interest_group/test_interest_group_builder.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

namespace content {
namespace {

using ::blink::IgExpectEqualsForTesting;
using ::blink::IgExpectNotEqualsForTesting;
using ::blink::InterestGroup;
using ::testing::Field;
using ::testing::Property;
using ::testing::UnorderedElementsAre;
using ::testing::UnorderedElementsAreArray;
using SellerCapabilities = blink::SellerCapabilities;
using SellerCapabilitiesType = blink::SellerCapabilitiesType;

constexpr char kFullOriginStr[] = "https://full.example.com";
constexpr char kPartialOriginStr[] = "https://partial.example.com";

constexpr int kOldestAllFieldsVersion = 28;

class InterestGroupStorageTest : public testing::Test {
 public:
  InterestGroupStorageTest() = default;

  void SetUp() override {
    ASSERT_TRUE(temp_directory_.CreateUniqueTempDir());
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        blink::features::kInterestGroupStorage,
        {{"max_owners", "10"},
         {"max_groups_per_owner", "10"},
         {"max_negative_groups_per_owner", "30"},
         {"max_ops_before_maintenance", "100"},
         {"max_storage_per_owner", "4096"}});
  }

  // Returns a summary of all interest groups. Each interest group is returned
  // as a string of the form:
  // "<origin>;<name>". This allows for easily checking that only the expected
  // interest groups remain.
  std::vector<std::string> GetInterestGroupSummary(
      InterestGroupStorage& storage) {
    std::vector<content::StorageInterestGroup> groups =
        storage.GetAllInterestGroupsUnfilteredForTesting();
    std::vector<std::string> summary;
    for (const auto& group : groups) {
      summary.emplace_back(base::StringPrintf(
          "%s;%s", group.interest_group.owner.Serialize().c_str(),
          group.interest_group.name.c_str()));
    }
    return summary;
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
    result.update_url = owner.GetURL().Resolve("/update_script.js");
    result.expiry = base::Time::Now() + base::Days(30);
    result.execution_mode =
        blink::InterestGroup::ExecutionMode::kCompatibilityMode;
    return result;
  }

  InterestGroup NewNegativeInterestGroup(url::Origin owner, std::string name) {
    constexpr blink::InterestGroup::AdditionalBidKey kAdditionalBidKey = {
        0x7d, 0x4d, 0x0e, 0x7f, 0x61, 0x53, 0xa6, 0x9b, 0x62, 0x42, 0xb5,
        0x22, 0xab, 0xbe, 0xe6, 0x85, 0xfd, 0xa4, 0x42, 0x0f, 0x88, 0x34,
        0xb1, 0x08, 0xc3, 0xbd, 0xae, 0x36, 0x9e, 0xf5, 0x49, 0xfa};

    InterestGroup result;
    result.owner = owner;
    result.name = name;
    result.additional_bid_key = kAdditionalBidKey;
    result.expiry = base::Time::Now() + base::Days(30);
    return result;
  }

  // Produces full interest group. The `version_number` parameter controls the
  // set of fields added to the full interest group -- only fields that existed
  // in that version will be added. By default, all fields for the current
  // version are added.
  //
  // If non-null, *version_changed_ig_fields will be set indicating if
  // `version_number` changed any new interest group fields when compared to
  // `version_number` - 1, if it exists. (Some versions merely changed the
  // format of interest groups on disk and didn't add fields, or added
  // fields that don't affect blink::InterestGroup). This is needed for a check
  // to ensure that IgExpect[Not]EqualsForTesting() gets updated when adding a
  // new version.
  blink::InterestGroup ProduceAllFields(
      int version_number = -1,
      bool* version_changed_ig_fields = nullptr) {
    if (version_number == -1) {
      version_number =
          InterestGroupStorage::GetCurrentVersionNumberForTesting();
    }

    if (version_changed_ig_fields) {
      switch (version_number) {
        case 9:
        case 16:
        case 17:
        case 18:
        case 20:
        case 22:
        case 24:
        case 25:
        case 26:
        case 27:
        case 30:
          *version_changed_ig_fields = false;
          break;
        default:
          *version_changed_ig_fields = true;
      }
    }

    // These fields have been supported for many old versions -- setting them
    // here avoids having to check and maybe initialize multiple times in each
    // version.
    blink::InterestGroup result;
    result.ads.emplace();
    result.ad_components.emplace();
    result.ads->emplace_back(
        /*render_gurl=*/GURL("https://full.example.com/ad1"),
        /*metadata=*/"metadata1");
    result.ads->emplace_back(
        /*render_gurl=*/GURL("https://full.example.com/ad2"),
        /*metadata=*/"metadata2");
    result.ad_components->emplace_back(
        /*render_gurl=*/GURL("https://full.example.com/adcomponent1"),
        /*metadata=*/"metadata1c");
    result.ad_components->emplace_back(
        /*render_gurl=*/GURL("https://full.example.com/adcomponent2"),
        /*metadata=*/"metadata2c");
    result.ad_sizes.emplace();

    // ***NOTE***: Please use non-default values in the assignments below -- it
    // helps validate that non-default fields get upgraded correctly, for
    // instance.

    switch (version_number) {
      case 30:
        // Compressed AdsProto, but introduced no new fields.
        ABSL_FALLTHROUGH_INTENDED;
      case 29:
        result.ads.value()[0].selectable_buyer_and_seller_reporting_ids = {
            "selectable_id1", "selectable_id2"};
        ABSL_FALLTHROUGH_INTENDED;
      case 28:
        // NOTE: As this is the oldest version supported by ProduceAllFields(),
        // it also initializes fields from before version 28.
        EXPECT_EQ(kOldestAllFieldsVersion, 28);

        result.owner = kFullOrigin;
        result.name = "full";
        result.priority = 1.0;
        result.enable_bidding_signals_prioritization = true;
        result.priority_vector = {{{"a", 2}, {"b", -2.2}}};
        result.priority_signals_overrides = {
            {{"a", -2}, {"c", 10}, {"d", 1.2}}};
        result.seller_capabilities = {
            {{kFullOrigin, {SellerCapabilities::kInterestGroupCounts}},
             {kPartialOrigin, {SellerCapabilities::kLatencyStats}}}};
        result.all_sellers_capabilities = {
            SellerCapabilities::kInterestGroupCounts,
            SellerCapabilities::kLatencyStats};
        result.execution_mode =
            blink::InterestGroup::ExecutionMode::kFrozenContext;
        result.bidding_url = GURL("https://full.example.com/bid");
        result.bidding_wasm_helper_url =
            GURL("https://full.example.com/bid_wasm");
        result.update_url = GURL("https://full.example.com/update");
        result.trusted_bidding_signals_url =
            GURL("https://full.example.com/signals");
        result.trusted_bidding_signals_keys = {"a", "b", "c", "d"};
        result.trusted_bidding_signals_slot_size_mode = blink::InterestGroup::
            TrustedBiddingSignalsSlotSizeMode::kAllSlotsRequestedSizes;
        result.max_trusted_bidding_signals_url_length = 8000;
        result.trusted_bidding_signals_coordinator =
            url::Origin::Create(GURL("https://coordinator.test/"));
        result.user_bidding_signals = "foo";
        result.ads.value()[0].size_group = "group_1";
        result.ads.value()[0].buyer_reporting_id = "buyer_id";
        result.ads.value()[0].buyer_and_seller_reporting_id = "shared_id";
        result.ads.value()[0].ad_render_id = "adRenderId";
        result.ads.value()[0].allowed_reporting_origins = {
            {url::Origin::Create(GURL("https://reporting.com"))}};
        result.ads.value()[1].size_group = "group_2";
        result.ads.value()[1].buyer_reporting_id = "buyer_id2";
        result.ad_components.value()[0].size_group = "group_1";
        result.ad_components.value()[0].ad_render_id = "adRenderId2";
        result.ad_components.value()[1].size_group = "group_2";
        result.ad_sizes.value()["size_1"].width = 300;
        result.ad_sizes.value()["size_1"].width_units =
            blink::AdSize::LengthUnit::kPixels;
        result.ad_sizes.value()["size_1"].height = 150;
        result.ad_sizes.value()["size_1"].height_units =
            blink::AdSize::LengthUnit::kPixels;
        result.ad_sizes.value()["size_2"].width = 640;
        result.ad_sizes.value()["size_2"].width_units =
            blink::AdSize::LengthUnit::kPixels;
        result.ad_sizes.value()["size_2"].height = 480;
        result.ad_sizes.value()["size_2"].height_units =
            blink::AdSize::LengthUnit::kPixels;
        result.ad_sizes.value()["size_3"].width = 100;
        result.ad_sizes.value()["size_3"].width_units =
            blink::AdSize::LengthUnit::kScreenWidth;
        result.ad_sizes.value()["size_3"].height = 100;
        result.ad_sizes.value()["size_3"].height_units =
            blink::AdSize::LengthUnit::kScreenWidth;
        result.size_groups = {
            {{"group_1", std::vector<std::string>{"size_1"}},
             {"group_2", std::vector<std::string>{"size_1", "size_2"}},
             {"group_3", std::vector<std::string>{"size_3"}}}};
        result.auction_server_request_flags = {
            blink::AuctionServerRequestFlagsEnum::kOmitAds,
            blink::AuctionServerRequestFlagsEnum::kIncludeFullAds};
        // Note that `additional_bid_key` can only be set for negative
        // interest groups, so cannot be set here.
        result.aggregation_coordinator_origin =
            url::Origin::Create(GURL("https://coordinator.test/"));
        break;
      default:
        ADD_FAILURE()
            << "Requested version number " << version_number
            << " isn't currently supported by ProduceAllFields(). Please "
               "update ProduceAllFields() for this version -- and if "
               "appropriate, mark this version's `version_changed_ig_fields` "
               "as false.";
    }

    // Set to a valid non-expired time, to match InterestGroupBuilder. Note that
    // Now() will change each run (time starts at the actual current time, even
    // with MOCK_TIME), so upgrade tests will need to ignore the expiry.
    result.expiry = base::Time::Now() + base::Days(30);

    return result;
  }

  // This test is in a helper function so that it can also be run after
  // UpgradeFromV6.
  void StoresAllFieldsTest() {
    InterestGroup partial = NewInterestGroup(kPartialOrigin, "partial");

    InterestGroup full = ProduceAllFields();

    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

    storage->JoinInterestGroup(partial, kPartialOrigin.GetURL());
    storage->JoinInterestGroup(full, kFullOrigin.GetURL());

    std::vector<StorageInterestGroup> storage_interest_groups =
        storage->GetInterestGroupsForOwner(kPartialOrigin);
    ASSERT_EQ(1u, storage_interest_groups.size());
    IgExpectEqualsForTesting(
        /*actual=*/storage_interest_groups[0].interest_group,
        /*expected=*/partial);

    storage_interest_groups = storage->GetInterestGroupsForOwner(kFullOrigin);
    ASSERT_EQ(1u, storage_interest_groups.size());
    IgExpectEqualsForTesting(
        /*actual=*/storage_interest_groups[0].interest_group,
        /*expected=*/full);
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
    update.ads->emplace_back(
        GURL("https://full.example.com/ad3"), "metadata3", "group_3",
        "new_buyer_id", "another_share_id",
        std::vector<std::string>{"new_selectable_id1", "new_selectable_id2"},
        "adRenderId3",
        std::vector<url::Origin>{
            url::Origin::Create(GURL("https://reporting.updated.com"))});
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

    storage_interest_groups = storage->GetInterestGroupsForOwner(kFullOrigin);
    ASSERT_EQ(1u, storage_interest_groups.size());
    IgExpectEqualsForTesting(
        /*actual=*/storage_interest_groups[0].interest_group,
        /*expected=*/updated);
    // `join_time` should not be modified be updates, but `last_updated` should
    // be.
    EXPECT_EQ(storage_interest_groups[0].join_time, join_time);
    EXPECT_EQ(storage_interest_groups[0].last_updated, base::Time::Now());
    // Make sure the clock was advanced.
    EXPECT_NE(storage_interest_groups[0].join_time,
              storage_interest_groups[0].last_updated);
  }

  const url::Origin kFullOrigin = url::Origin::Create(GURL(kFullOriginStr));
  const url::Origin kPartialOrigin =
      url::Origin::Create(GURL(kPartialOriginStr));

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
    // [joined_k_anon], [meta], [lockout_debugging_only_report],
    // [cooldown_debugging_only_report], [bidding_and_auction_server_keys].
    EXPECT_EQ(9u, sql::test::CountSQLTables(&raw_db)) << raw_db.GetSchema();
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
    // [joined_k_anon], [meta], [lockout_debugging_only_report],
    // [cooldown_debugging_only_report], [bidding_and_auction_server_keys].
    EXPECT_EQ(9u, sql::test::CountSQLTables(&raw_db)) << raw_db.GetSchema();
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
    // [joined_k_anon], [meta], [lockout_debugging_only_report],
    // [cooldown_debugging_only_report], [bidding_and_auction_server_keys].
    EXPECT_EQ(9u, sql::test::CountSQLTables(&raw_db)) << raw_db.GetSchema();
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
    EXPECT_EQ(interest_groups[0].next_update_after, base::Time::Min());
  }
  histograms.ExpectUniqueSample("Storage.InterestGroup.PerSiteCount", 1u, 1);
}

TEST_F(InterestGroupStorageTest, GetGroupDoesNotReturnOutdatedKanonKeys) {
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  GURL ad1_url = GURL("https://owner.example.com/ad1");
  GURL ad2_url = GURL("https://owner.example.com/ad2");

  InterestGroup g = NewInterestGroup(test_origin, "name");
  blink::InterestGroupKey group_key(g.owner, g.name);
  std::vector<InterestGroup::Ad> ads;
  std::vector<InterestGroup::Ad> ad_components;
  ads.emplace_back(
      ad1_url, "metadata1",
      /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/"brid1",
      /*buyer_and_seller_reporting_id=*/"shrid1",
      /*selectable_buyer_and_seller_reporting_ids=*/
      std::vector<std::string>{"selectable_id1", "selectable_id2"});
  ads.emplace_back(ad2_url, "metadata2",
                   /*size_group=*/std::nullopt,
                   /*buyer_reporting_id=*/"brid2",
                   /*buyer_and_seller_reporting_id=*/std::nullopt);
  ads.emplace_back(ad2_url, "metadata3",
                   /*size_group=*/std::nullopt,
                   /*buyer_reporting_id=*/std::nullopt,
                   /*buyer_and_seller_reporting_id=*/std::nullopt);
  ad_components.emplace_back(ad2_url, "component_metadata3");
  ad_components.emplace_back(ad1_url, "component_metadata1");

  g.ads = ads;
  g.ad_components = ad_components;
  std::string kanon_bid1 = blink::HashedKAnonKeyForAdBid(g, ad1_url.spec());
  std::string kanon_report1 = blink::HashedKAnonKeyForAdNameReporting(
      g, g.ads.value()[0],
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt);
  std::string kanon_report1a = blink::HashedKAnonKeyForAdNameReporting(
      g, g.ads.value()[0], std::string("selectable_id1"));
  std::string kanon_report1b = blink::HashedKAnonKeyForAdNameReporting(
      g, g.ads.value()[0], std::string("selectable_id2"));
  std::string kanon_bid2 = blink::HashedKAnonKeyForAdBid(g, ad2_url.spec());
  std::string kanon_report2 = blink::HashedKAnonKeyForAdNameReporting(
      g, g.ads.value()[1],
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt);
  std::string kanon_component_1 = blink::HashedKAnonKeyForAdComponentBid(
      g.ad_components.value()[0].render_url());

  storage->JoinInterestGroup(g, test_origin.GetURL());
  std::vector<std::string> expected_positive_returned_keys = {
      kanon_bid1, kanon_report1, kanon_report1a,   kanon_report1b,
      kanon_bid2, kanon_report2, kanon_component_1};
  storage->UpdateKAnonymity(group_key, expected_positive_returned_keys,
                            base::Time::Now(), true);

  EXPECT_THAT(
      storage->GetInterestGroup(group_key)->hashed_kanon_keys,
      testing::UnorderedElementsAreArray(expected_positive_returned_keys));

  // Make some keys outdated via another join.
  g.ad_components.reset();
  storage->JoinInterestGroup(g, test_origin.GetURL());

  expected_positive_returned_keys.pop_back();  // Remove the component key.
  EXPECT_THAT(
      storage->GetInterestGroup(group_key)->hashed_kanon_keys,
      testing::UnorderedElementsAreArray(expected_positive_returned_keys));

  // Make some keys outdated via an update.
  InterestGroupUpdate update;
  update.ads = {ads[0]};
  storage->UpdateInterestGroup(group_key, update);

  expected_positive_returned_keys = {kanon_bid1, kanon_report1, kanon_report1a,
                                     kanon_report1b};
  EXPECT_THAT(
      storage->GetInterestGroup(group_key)->hashed_kanon_keys,
      testing::UnorderedElementsAreArray(expected_positive_returned_keys));
}

TEST_F(InterestGroupStorageTest,
       JoinAndUpdateReturnCorrectKanonUpdateParameter) {
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  GURL ad1_url = GURL("https://owner.example.com/ad1");
  GURL ad2_url = GURL("https://owner.example.com/ad2");
  GURL ad3_url = GURL("https://owner.example.com/ad3");

  InterestGroup g = NewInterestGroup(test_origin, "name");
  blink::InterestGroupKey group_key(g.owner, g.name);
  std::vector<InterestGroup::Ad> ads;
  std::vector<InterestGroup::Ad> ad_components;
  ads.emplace_back(
      ad1_url, "metadata1",
      /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/"brid1",
      /*buyer_and_seller_reporting_id=*/"shrid1",
      /*selectable_buyer_and_seller_reporting_ids=*/
      std::vector<std::string>{"selectable_id1", "selectable_id2"});
  ads.emplace_back(ad2_url, "metadata2",
                   /*size_group=*/std::nullopt,
                   /*buyer_reporting_id=*/"brid2",
                   /*buyer_and_seller_reporting_id=*/std::nullopt);
  ads.emplace_back(ad3_url, "metadata2",
                   /*size_group=*/std::nullopt,
                   /*buyer_reporting_id=*/"brid2",
                   /*buyer_and_seller_reporting_id=*/std::nullopt);
  ad_components.emplace_back(ad3_url, "component_metadata3");
  ad_components.emplace_back(ad1_url, "component_metadata1");

  g.ads = ads;
  g.ad_components = ad_components;
  std::string kanon_bid1 = blink::HashedKAnonKeyForAdBid(g, ad1_url.spec());
  std::string kanon_report1 = blink::HashedKAnonKeyForAdNameReporting(
      g, g.ads.value()[0],
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt);
  std::string kanon_report1a = blink::HashedKAnonKeyForAdNameReporting(
      g, g.ads.value()[0], std::string("selectable_id1"));
  std::string kanon_report1b = blink::HashedKAnonKeyForAdNameReporting(
      g, g.ads.value()[0], std::string("selectable_id2"));
  std::string kanon_bid2 = blink::HashedKAnonKeyForAdBid(g, ad2_url.spec());
  std::string kanon_report2 = blink::HashedKAnonKeyForAdNameReporting(
      g, g.ads.value()[1],
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt);
  std::string kanon_bid3 = blink::HashedKAnonKeyForAdBid(g, ad3_url.spec());
  std::string kanon_report3 = blink::HashedKAnonKeyForAdNameReporting(
      g, g.ads.value()[2],
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt);
  std::string kanon_component_1 = blink::HashedKAnonKeyForAdComponentBid(
      g.ad_components.value()[0].render_url());
  std::string kanon_component_2 = blink::HashedKAnonKeyForAdComponentBid(
      g.ad_components.value()[1].render_url());

  // Join a new interest group.
  {
    g.ads->clear();
    g.ad_components->clear();
    g.ads->emplace_back(ads[0]);
    g.ads->emplace_back(ads[1]);
    std::optional<InterestGroupKanonUpdateParameter> k_anon_update_data =
        storage->JoinInterestGroup(g, test_origin.GetURL());
    ASSERT_TRUE(k_anon_update_data.has_value());
    // The keys have never been updated.
    EXPECT_EQ(k_anon_update_data->update_time, base::Time::Min());
    // All  keys are new. All keys are returned.
    std::vector<std::string> all_kanon_keys = {kanon_bid1,     kanon_bid2,
                                               kanon_report1,  kanon_report1a,
                                               kanon_report1b, kanon_report2};
    EXPECT_THAT(k_anon_update_data->hashed_keys,
                testing::UnorderedElementsAreArray(all_kanon_keys));
    EXPECT_THAT(k_anon_update_data->newly_added_hashed_keys,
                testing::UnorderedElementsAreArray(all_kanon_keys));
  }

  // Update the k-anonymity for the interest group so that we can make
  // sure we get the correct update_time.
  base::Time update_time = base::Time::Now();
  storage->UpdateKAnonymity(group_key, {kanon_bid1}, update_time, true);

  // Join an existing interest group with the exact same ads.
  {
    std::optional<InterestGroupKanonUpdateParameter> k_anon_update_data =
        storage->JoinInterestGroup(g, test_origin.GetURL());
    ASSERT_TRUE(k_anon_update_data.has_value());
    EXPECT_EQ(k_anon_update_data->update_time, update_time);
    std::vector<std::string> all_kanon_keys = {kanon_bid1,     kanon_bid2,
                                               kanon_report1,  kanon_report1a,
                                               kanon_report1b, kanon_report2};
    // No new keys. The set of all keys is the same.
    EXPECT_THAT(k_anon_update_data->hashed_keys,
                testing::UnorderedElementsAreArray(all_kanon_keys));
    EXPECT_THAT(k_anon_update_data->newly_added_hashed_keys,
                testing::IsEmpty());
  }

  // Join an existing interest group with additional ads.
  {
    g.ads->emplace_back(ads[2]);
    g.ad_components = ad_components;
    std::optional<InterestGroupKanonUpdateParameter> k_anon_update_data =
        storage->JoinInterestGroup(g, test_origin.GetURL());
    ASSERT_TRUE(k_anon_update_data.has_value());
    EXPECT_EQ(k_anon_update_data->update_time, update_time);
    std::vector<std::string> all_kanon_keys = {
        kanon_bid1,        kanon_bid2,       kanon_bid3,    kanon_report1,
        kanon_report1a,    kanon_report1b,   kanon_report2, kanon_report3,
        kanon_component_1, kanon_component_2};
    // Expect that the new keys are represented.
    EXPECT_THAT(k_anon_update_data->hashed_keys,
                testing::UnorderedElementsAreArray(all_kanon_keys));
    EXPECT_THAT(
        k_anon_update_data->newly_added_hashed_keys,
        testing::UnorderedElementsAreArray(
            {kanon_bid3, kanon_report3, kanon_component_1, kanon_component_2}));
  }

  // Join an interest group containing a subset of the ads in the current group.
  {
    g.ads = {ads[0]};
    g.ad_components = {ad_components[1]};
    std::optional<InterestGroupKanonUpdateParameter> k_anon_update_data =
        storage->JoinInterestGroup(g, test_origin.GetURL());
    ASSERT_TRUE(k_anon_update_data.has_value());
    EXPECT_EQ(k_anon_update_data->update_time, update_time);
    std::vector<std::string> all_kanon_keys = {kanon_bid1, kanon_report1,
                                               kanon_report1a, kanon_report1b,
                                               kanon_component_2};
    // There are no new keys.
    EXPECT_THAT(k_anon_update_data->hashed_keys,
                testing::UnorderedElementsAreArray(all_kanon_keys));
    EXPECT_THAT(k_anon_update_data->newly_added_hashed_keys,
                testing::IsEmpty());
  }

  // Join an interest group with new keys and just one of the existing keys.
  {
    g.ads = {ads[0], ads[1], ads[2]};
    g.ad_components = {ad_components[0]};
    std::optional<InterestGroupKanonUpdateParameter> k_anon_update_data =
        storage->JoinInterestGroup(g, test_origin.GetURL());
    ASSERT_TRUE(k_anon_update_data.has_value());
    EXPECT_EQ(k_anon_update_data->update_time, update_time);
    std::vector<std::string> all_kanon_keys = {
        kanon_bid1,     kanon_report1, kanon_report1a,
        kanon_report1b, kanon_bid2,    kanon_report2,
        kanon_bid3,     kanon_report3, kanon_component_1};
    EXPECT_THAT(k_anon_update_data->hashed_keys,
                testing::UnorderedElementsAreArray(all_kanon_keys));
    EXPECT_THAT(k_anon_update_data->newly_added_hashed_keys,
                testing::UnorderedElementsAreArray({kanon_bid2, kanon_report2,
                                                    kanon_bid3, kanon_report3,
                                                    kanon_component_1}));
  }

  // Join an interest group with no ads.
  {
    g.ads->clear();
    g.ad_components->clear();
    std::optional<InterestGroupKanonUpdateParameter> k_anon_update_data =
        storage->JoinInterestGroup(g, test_origin.GetURL());
    ASSERT_TRUE(k_anon_update_data.has_value());
    EXPECT_EQ(k_anon_update_data->update_time, update_time);
    // There are no new keys.
    EXPECT_THAT(k_anon_update_data->hashed_keys, testing::IsEmpty());
    EXPECT_THAT(k_anon_update_data->newly_added_hashed_keys,
                testing::IsEmpty());
  }

  InterestGroupUpdate update;

  // Do an interest group update that has new ad components.
  {
    update.ad_components = ad_components;
    std::optional<InterestGroupKanonUpdateParameter> k_anon_update_data =
        storage->UpdateInterestGroup(group_key, update);
    ASSERT_TRUE(k_anon_update_data.has_value());
    EXPECT_EQ(k_anon_update_data->update_time, update_time);
    EXPECT_THAT(k_anon_update_data->hashed_keys,
                testing::UnorderedElementsAreArray(
                    {kanon_component_1, kanon_component_2}));
    EXPECT_THAT(k_anon_update_data->newly_added_hashed_keys,
                testing::UnorderedElementsAreArray(
                    {kanon_component_1, kanon_component_2}));
  }

  // Do an interest group update that has no new ads or ad components.
  {
    std::optional<InterestGroupKanonUpdateParameter> k_anon_update_data =
        storage->UpdateInterestGroup(group_key, update);
    ASSERT_TRUE(k_anon_update_data.has_value());
    EXPECT_EQ(k_anon_update_data->update_time, update_time);
    EXPECT_THAT(k_anon_update_data->hashed_keys,
                testing::UnorderedElementsAreArray(
                    {kanon_component_1, kanon_component_2}));
    EXPECT_THAT(k_anon_update_data->newly_added_hashed_keys,
                testing::IsEmpty());
  }

  // Do an interest group update that has new ads.
  {
    update.ads = {ads[0], ads[1]};
    std::optional<InterestGroupKanonUpdateParameter> k_anon_update_data =
        storage->UpdateInterestGroup(group_key, update);
    ASSERT_TRUE(k_anon_update_data.has_value());
    EXPECT_EQ(k_anon_update_data->update_time, update_time);
    EXPECT_THAT(k_anon_update_data->hashed_keys,
                testing::UnorderedElementsAreArray(
                    {kanon_bid1, kanon_bid2, kanon_report1, kanon_report1a,
                     kanon_report1b, kanon_report2, kanon_component_1,
                     kanon_component_2}));
    EXPECT_THAT(k_anon_update_data->newly_added_hashed_keys,
                testing::UnorderedElementsAreArray(
                    {kanon_bid1, kanon_bid2, kanon_report1, kanon_report1a,
                     kanon_report1b, kanon_report2}));
  }

  // Do an interest group update that doesn't have the ads or ad_components
  // fields.
  {
    update.ads.reset();
    update.ad_components.reset();
    std::optional<InterestGroupKanonUpdateParameter> k_anon_update_data =
        storage->UpdateInterestGroup(group_key, update);
    ASSERT_TRUE(k_anon_update_data.has_value());
    EXPECT_EQ(k_anon_update_data->update_time, update_time);
    EXPECT_THAT(k_anon_update_data->hashed_keys,
                testing::UnorderedElementsAreArray(
                    {kanon_bid1, kanon_bid2, kanon_report1, kanon_report1a,
                     kanon_report1b, kanon_report2, kanon_component_1,
                     kanon_component_2}));
    EXPECT_THAT(k_anon_update_data->newly_added_hashed_keys,
                testing::IsEmpty());
  }

  // Do an interest group update that updates the bidding URL. This will affect
  // the reporting and bidding k-anon keys.
  {
    update.bidding_url = GURL("https://owner.example.com/bid2");

    // Recalculate our keys with the new bidding URL.
    g.ads = {ads[0], ads[1]};
    g.bidding_url = update.bidding_url;
    kanon_bid1 = blink::HashedKAnonKeyForAdBid(g, ad1_url.spec());
    kanon_bid2 = blink::HashedKAnonKeyForAdBid(g, ad2_url.spec());
    kanon_report1 = blink::HashedKAnonKeyForAdNameReporting(
        g, ads[0],
        /*selected_buyer_and_seller_reporting_id=*/std::nullopt);
    kanon_report1a = blink::HashedKAnonKeyForAdNameReporting(
        g, ads[0], std::string("selectable_id1"));
    kanon_report1b = blink::HashedKAnonKeyForAdNameReporting(
        g, ads[0], std::string("selectable_id2"));
    kanon_report2 = blink::HashedKAnonKeyForAdNameReporting(
        g, ads[1],
        /*selected_buyer_and_seller_reporting_id=*/std::nullopt);

    std::optional<InterestGroupKanonUpdateParameter> k_anon_update_data =
        storage->UpdateInterestGroup(group_key, update);
    ASSERT_TRUE(k_anon_update_data.has_value());
    EXPECT_EQ(k_anon_update_data->update_time, update_time);
    EXPECT_THAT(k_anon_update_data->hashed_keys,
                testing::UnorderedElementsAreArray(
                    {kanon_bid1, kanon_bid2, kanon_report1, kanon_report1a,
                     kanon_report1b, kanon_report2, kanon_component_1,
                     kanon_component_2}));
    EXPECT_THAT(k_anon_update_data->newly_added_hashed_keys,
                testing::UnorderedElementsAreArray(
                    {kanon_bid1, kanon_bid2, kanon_report1, kanon_report1a,
                     kanon_report1b, kanon_report2}));
  }

  // Do an interest group update that updates the bidding URL, ads, and ad
  // components.
  {
    update.bidding_url = GURL("https://owner.example.com/bid1");
    update.ads = {ads[2]};
    update.ad_components = {ad_components[0]};

    // Recalculate our keys with the new bidding URL.
    g.ads = {ads[2]};
    g.bidding_url = update.bidding_url;
    kanon_bid3 = blink::HashedKAnonKeyForAdBid(g, ad3_url.spec());
    kanon_report3 = blink::HashedKAnonKeyForAdNameReporting(
        g, ads[2],
        /*selected_buyer_and_seller_reporting_id=*/std::nullopt);

    std::optional<InterestGroupKanonUpdateParameter> k_anon_update_data =
        storage->UpdateInterestGroup(group_key, update);
    ASSERT_TRUE(k_anon_update_data.has_value());
    EXPECT_EQ(k_anon_update_data->update_time, update_time);
    EXPECT_THAT(k_anon_update_data->hashed_keys,
                testing::UnorderedElementsAreArray(
                    {kanon_bid3, kanon_report3, kanon_component_1}));
    EXPECT_THAT(
        k_anon_update_data->newly_added_hashed_keys,
        testing::UnorderedElementsAreArray({kanon_bid3, kanon_report3}));
  }

  // Validate the interest groups's 'next_update_after' field.
  {
    std::vector<StorageInterestGroup> interest_groups =
        storage->GetInterestGroupsForOwner(test_origin);
    EXPECT_EQ(1u, interest_groups.size());

    EXPECT_EQ(interest_groups[0].next_update_after,
              base::Time::Now() +
                  InterestGroupStorage::kUpdateSucceededBackoffPeriod);
  }
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

// Test ClearOriginJoinedInterestGroups().
//
// Join the following interest groups:
// * With joining origin A, join 3 interest groups with owner B, 2 with an
//   `executionMode` of "group-by-origin".
// * With joining origin A, join 1 interest group with owner C.
// * With joining origin site C, join 1 interest group with owner B.
//
// Then call ClearOriginJoinedInterestGroups() from origin A with owner B
// a number of times, making sure that only the expected IGs are deleted
// each time.
TEST_F(InterestGroupStorageTest, ClearOriginJoinedInterestGroups) {
  const url::Origin kOriginA = url::Origin::Create(GURL("https://a.test"));
  const url::Origin kOriginB = url::Origin::Create(GURL("https://b.test"));
  const url::Origin kOriginC = url::Origin::Create(GURL("https://c.test"));
  const char kName1[] = "name1";
  const char kName2[] = "name2";
  const char kName3[] = "name3";
  const char kName4[] = "name4";

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  // Join 3 interest groups owned by kOriginB on kOriginA, with kName1, kName2,
  // and kName3. The latter two have "group-by-origin" execution mode.
  storage->JoinInterestGroup(NewInterestGroup(kOriginB, kName1),
                             kOriginA.GetURL());
  InterestGroup interest_group = NewInterestGroup(kOriginB, kName2);
  interest_group.execution_mode =
      blink::InterestGroup::ExecutionMode::kGroupedByOriginMode;
  storage->JoinInterestGroup(interest_group, kOriginA.GetURL());
  interest_group = NewInterestGroup(kOriginB, kName3);
  interest_group.execution_mode =
      blink::InterestGroup::ExecutionMode::kGroupedByOriginMode;
  storage->JoinInterestGroup(interest_group, kOriginA.GetURL());

  // Join an interest group owned by kOriginC from kOriginA. This should not be
  // left by when calling ClearOriginJoinedInterestGroups() from kOriginA for
  // kOriginB's interest groups.
  storage->JoinInterestGroup(NewInterestGroup(kOriginC, kName1),
                             kOriginA.GetURL());

  // Join an interest group owned by kOriginB from kOriginC. This should not be
  // left by when calling ClearOriginJoinedInterestGroups() from kOriginA for
  // kOriginB's interest groups.
  storage->JoinInterestGroup(NewInterestGroup(kOriginB, kName4),
                             kOriginC.GetURL());

  // Clear all of origin B's interest groups joined from origin B. This should
  // leave no interest groups.
  storage->ClearOriginJoinedInterestGroups(kOriginB, {},
                                           /*main_frame_origin=*/kOriginB);
  EXPECT_THAT(GetInterestGroupSummary(*storage),
              testing::UnorderedElementsAre(
                  // Origin B's groups that were joined on origin A.
                  "https://b.test;name1", "https://b.test;name2",
                  "https://b.test;name3",
                  // Other groups.
                  "https://c.test;name1", "https://b.test;name4"));

  // Leave all of origin's B's interest groups joined from origin A, except for
  // a list that contains all of the groups actually joined that way (plus an
  // extra group). No groups should be left.
  EXPECT_THAT(storage->ClearOriginJoinedInterestGroups(
                  kOriginB, {kName1, kName2, kName3, "not-present-group"},
                  /*main_frame_origin=*/kOriginA),
              testing::UnorderedElementsAre());
  EXPECT_THAT(GetInterestGroupSummary(*storage),
              testing::UnorderedElementsAre(
                  // Origin B's groups that were joined on origin A.
                  "https://b.test;name1", "https://b.test;name2",
                  "https://b.test;name3",
                  // Other groups.
                  "https://c.test;name1", "https://b.test;name4"));

  // Leave all of origin's B's interest groups joined from origin A, except for
  // kName1 and kName3. Only the kName2 group should be left. Despite kName2 and
  // kName3 groups both having "group-by-origin" execution mode, group kName3
  // should not have been left.
  EXPECT_THAT(
      storage->ClearOriginJoinedInterestGroups(kOriginB, {kName1, kName3},
                                               /*main_frame_origin=*/kOriginA),
      testing::UnorderedElementsAre(kName2));
  EXPECT_THAT(GetInterestGroupSummary(*storage),
              testing::UnorderedElementsAre(
                  // Origin B's groups that were joined on origin A.
                  "https://b.test;name1", "https://b.test;name3",
                  // Other groups.
                  "https://c.test;name1", "https://b.test;name4"));

  // Leave all of origin's B's interest groups joined from origin A.
  EXPECT_THAT(
      storage->ClearOriginJoinedInterestGroups(kOriginB, {},
                                               /*main_frame_origin=*/kOriginA),
      testing::UnorderedElementsAre(kName1, kName3));
  EXPECT_THAT(GetInterestGroupSummary(*storage),
              testing::UnorderedElementsAre("https://c.test;name1",
                                            "https://b.test;name4"));
}

// Make sure that ClearOriginJoinedInterestGroups() clears join, bid, and win
// history.
TEST_F(InterestGroupStorageTest, ClearOriginJoinedInterestGroupsClearsHistory) {
  const url::Origin kOrigin = url::Origin::Create(GURL("https://a.test"));
  const char kName[] = "name";
  const blink::InterestGroupKey kGroupKey(kOrigin, kName);

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  storage->JoinInterestGroup(NewInterestGroup(kOrigin, kName),
                             kOrigin.GetURL());

  // Increment each history count by 1.
  storage->JoinInterestGroup(NewInterestGroup(kOrigin, kName),
                             kOrigin.GetURL());
  storage->RecordInterestGroupBids({kGroupKey});
  storage->RecordInterestGroupWin(kGroupKey, "{\"url\": \"https://ad.test\"}");

  // Check the group is in the expected state.
  std::optional<content::StorageInterestGroup> group =
      storage->GetInterestGroup(kGroupKey);
  ASSERT_TRUE(group);
  EXPECT_EQ(2, group->bidding_browser_signals->join_count);
  EXPECT_EQ(1, group->bidding_browser_signals->bid_count);
  EXPECT_EQ(1u, group->bidding_browser_signals->prev_wins.size());

  // Clear the group.
  storage->ClearOriginJoinedInterestGroups(kOrigin, {},
                                           /*main_frame_origin=*/kOrigin);
  EXPECT_FALSE(storage->GetInterestGroup(kGroupKey));

  // Join the group again.
  storage->JoinInterestGroup(NewInterestGroup(kOrigin, kName),
                             kOrigin.GetURL());

  // Check that none of the history was retained.
  group = storage->GetInterestGroup(kGroupKey);
  ASSERT_TRUE(group);
  EXPECT_EQ(1, group->bidding_browser_signals->join_count);
  EXPECT_EQ(0, group->bidding_browser_signals->bid_count);
  EXPECT_EQ(0u, group->bidding_browser_signals->prev_wins.size());
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

  std::vector<InterestGroupUpdateParameter> update_infos =
      storage->GetInterestGroupsForUpdate(test_origin1,
                                          /*groups_limit=*/kSmallFetchGroups);

  GURL expected_update_url = test_origin1.GetURL().Resolve("/update_script.js");
  EXPECT_EQ(kSmallFetchGroups, update_infos.size());
  for (const auto& [ig_key, update_url, joining_origin] : update_infos) {
    EXPECT_EQ(test_origin1, ig_key.owner);
    EXPECT_EQ(update_url, expected_update_url);
    EXPECT_EQ(test_origin1, joining_origin);
  }

  update_infos =
      storage->GetInterestGroupsForUpdate(test_origin1,
                                          /*groups_limit=*/kLargeFetchGroups);

  EXPECT_EQ(kNumOrigin1Groups, update_infos.size());
  for (const auto& [ig_key, update_url, joining_origin] : update_infos) {
    EXPECT_EQ(test_origin1, ig_key.owner);
    EXPECT_EQ(update_url, expected_update_url);
    EXPECT_EQ(test_origin1, joining_origin);
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

TEST_F(InterestGroupStorageTest, RecordsDebugReportLockoutAndCooldown) {
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  const url::Origin test_origin2 =
      url::Origin::Create(GURL("https://seller.example.com"));
  std::vector<url::Origin> origins{test_origin, test_origin2};
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  std::optional<DebugReportLockoutAndCooldowns> cooldowns =
      storage->GetDebugReportLockoutAndCooldowns(origins);
  ASSERT_TRUE(cooldowns.has_value());
  EXPECT_FALSE(cooldowns->last_report_sent_time.has_value());
  EXPECT_TRUE(cooldowns->debug_report_cooldown_map.empty());

  std::optional<base::Time> lockout = storage->GetDebugReportLockout();
  ASSERT_FALSE(lockout.has_value());

  base::Time time = base::Time::Now();
  base::Time expected_time = base::Time::FromDeltaSinceWindowsEpoch(
      time.ToDeltaSinceWindowsEpoch().CeilToMultiple(base::Hours(1)));
  storage->RecordDebugReportLockout(time);
  cooldowns = storage->GetDebugReportLockoutAndCooldowns(origins);
  ASSERT_TRUE(cooldowns.has_value());
  ASSERT_TRUE(cooldowns->last_report_sent_time.has_value());
  EXPECT_EQ(expected_time, *cooldowns->last_report_sent_time);
  EXPECT_TRUE(cooldowns->debug_report_cooldown_map.empty());
  lockout = storage->GetDebugReportLockout();
  ASSERT_TRUE(lockout.has_value());
  EXPECT_EQ(expected_time, *lockout);

  storage->RecordDebugReportCooldown(test_origin, time,
                                     DebugReportCooldownType::kShortCooldown);
  cooldowns = storage->GetDebugReportLockoutAndCooldowns(origins);
  std::map<url::Origin, DebugReportCooldown> expected_cooldown_map;
  expected_cooldown_map[test_origin] = DebugReportCooldown(
      expected_time, DebugReportCooldownType::kShortCooldown);
  ASSERT_TRUE(cooldowns.has_value());
  ASSERT_TRUE(cooldowns->last_report_sent_time.has_value());
  EXPECT_EQ(expected_time, *cooldowns->last_report_sent_time);
  EXPECT_EQ(expected_cooldown_map, cooldowns->debug_report_cooldown_map);
  expected_cooldown_map.clear();

  // Ensure we get to a different hour, to get a different time.
  task_environment().FastForwardBy(base::Minutes(90));
  base::Time time2 = base::Time::Now();
  base::Time expected_time2 = base::Time::FromDeltaSinceWindowsEpoch(
      time2.ToDeltaSinceWindowsEpoch().CeilToMultiple(base::Hours(1)));
  storage->RecordDebugReportLockout(time2);
  storage->RecordDebugReportCooldown(
      test_origin, time2, DebugReportCooldownType::kRestrictedCooldown);
  storage->RecordDebugReportCooldown(test_origin2, time2,
                                     DebugReportCooldownType::kShortCooldown);
  cooldowns = storage->GetDebugReportLockoutAndCooldowns(origins);
  expected_cooldown_map[test_origin] = DebugReportCooldown(
      expected_time2, DebugReportCooldownType::kRestrictedCooldown);
  expected_cooldown_map[test_origin2] = DebugReportCooldown(
      expected_time2, DebugReportCooldownType::kShortCooldown);
  ASSERT_TRUE(cooldowns.has_value());
  ASSERT_TRUE(cooldowns->last_report_sent_time.has_value());
  EXPECT_EQ(expected_time2, *cooldowns->last_report_sent_time);
  EXPECT_EQ(expected_cooldown_map, cooldowns->debug_report_cooldown_map);
}

TEST_F(InterestGroupStorageTest, DeleteExpiredDebugReportCooldown) {
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  const url::Origin test_origin2 =
      url::Origin::Create(GURL("https://seller.example.com"));
  std::vector<url::Origin> origins{test_origin, test_origin2};
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  base::Time time = base::Time::Now();
  base::Time expected_time = base::Time::FromDeltaSinceWindowsEpoch(
      time.ToDeltaSinceWindowsEpoch().CeilToMultiple(base::Hours(1)));
  storage->RecordDebugReportCooldown(test_origin, time,
                                     DebugReportCooldownType::kShortCooldown);
  storage->RecordDebugReportCooldown(test_origin2, time,
                                     DebugReportCooldownType::kShortCooldown);
  std::optional<DebugReportLockoutAndCooldowns> cooldowns =
      storage->GetDebugReportLockoutAndCooldowns(origins);
  std::map<url::Origin, DebugReportCooldown> expected_cooldown_map;
  expected_cooldown_map[test_origin] = DebugReportCooldown(
      expected_time, DebugReportCooldownType::kShortCooldown);
  expected_cooldown_map[test_origin2] = DebugReportCooldown(
      expected_time, DebugReportCooldownType::kShortCooldown);
  ASSERT_TRUE(cooldowns.has_value());
  EXPECT_EQ(expected_cooldown_map, cooldowns->debug_report_cooldown_map);

  // Fast-forward past kFledgeDebugReportShortCooldown so that the cooldown will
  // expire. Fast-forward extra time to make sure the cooldown expires,
  // because the starting_time is ceiled to its nearest hour.
  task_environment().FastForwardBy(
      blink::features::kFledgeDebugReportShortCooldown.Get() + expected_time -
      time);
  // If maintenance has not been triggered yet, the cooldown table will not be
  // updated.
  cooldowns = storage->GetDebugReportLockoutAndCooldowns(origins);
  ASSERT_TRUE(cooldowns.has_value());
  EXPECT_EQ(expected_cooldown_map, cooldowns->debug_report_cooldown_map);
  // Trigger scheduling of the next maintenance.
  storage->GetAllInterestGroupOwners();
  // Allow enough idle time to trigger maintenance.
  task_environment().FastForwardBy(InterestGroupStorage::kIdlePeriod +
                                   base::Seconds(1));

  cooldowns = storage->GetDebugReportLockoutAndCooldowns(origins);
  ASSERT_TRUE(cooldowns.has_value());
  EXPECT_TRUE(cooldowns->debug_report_cooldown_map.empty());
}

//  TODO (b/356654297) Add tests for selectableBuyerAndSellerReportingIds,
//    when k-anon is implemented.
TEST_F(InterestGroupStorageTest, UpdatesAdKAnonymity) {
  url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  GURL ad1_url = GURL("https://owner.example.com/ad1");
  GURL ad2_url = GURL("https://owner.example.com/ad2");
  GURL ad3_url = GURL("https://owner.example.com/ad3");

  InterestGroup g = NewInterestGroup(test_origin, "name");
  g.ads.emplace();
  g.ads->emplace_back(
      ad1_url, "metadata1",
      /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/"brid1",
      /*buyer_and_seller_reporting_id=*/"shrid1",
      /*selectable_buyer_and_seller_reporting_ids=*/
      std::vector<std::string>{"selectable_id1", "selectable_id2"});
  g.ads->emplace_back(ad2_url, "metadata2",
                      /*size_group=*/std::nullopt,
                      /*buyer_reporting_id=*/"brid2",
                      /*buyer_and_seller_reporting_id=*/std::nullopt);
  g.ad_components.emplace();
  g.ad_components->push_back(
      blink::InterestGroup::Ad(ad1_url, "component_metadata1"));
  g.ad_components->push_back(
      blink::InterestGroup::Ad(ad3_url, "component_metadata3"));

  std::string kanon_bid1 = blink::HashedKAnonKeyForAdBid(g, ad1_url.spec());
  std::string kanon_report1 = blink::HashedKAnonKeyForAdNameReporting(
      g, g.ads.value()[0],
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt);
  std::string kanon_bid2 = blink::HashedKAnonKeyForAdBid(g, ad2_url.spec());
  std::string kanon_report2 = blink::HashedKAnonKeyForAdNameReporting(
      g, g.ads.value()[1],
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt);
  std::string kanon_component_1 = blink::HashedKAnonKeyForAdComponentBid(
      g.ad_components.value()[0].render_url());
  std::string kanon_component_2 = blink::HashedKAnonKeyForAdComponentBid(
      g.ad_components.value()[1].render_url());

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  std::vector<StorageInterestGroup> groups =
      storage->GetInterestGroupsForOwner(test_origin);

  EXPECT_EQ(0u, groups.size());

  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join"));

  groups = storage->GetInterestGroupsForOwner(test_origin);

  std::vector<std::string> expected_positive_keys = {};

  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].hashed_kanon_keys,
              testing::UnorderedElementsAreArray(expected_positive_keys));

  base::Time update_time = base::Time::Now();
  storage->UpdateKAnonymity(blink::InterestGroupKey(g.owner, g.name),
                            {kanon_bid1, kanon_report1}, update_time,
                            /*replace_existing_values*/ true);
  expected_positive_keys = {kanon_bid1, kanon_report1};

  groups = storage->GetInterestGroupsForOwner(test_origin);

  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].hashed_kanon_keys,
              testing::UnorderedElementsAreArray(expected_positive_keys));
  EXPECT_THAT(groups[0].last_k_anon_updated, update_time);

  task_environment().FastForwardBy(base::Seconds(1));

  // The new update for the interest group will override the old values because
  // we set replace_existing_values = true.
  update_time = base::Time::Now();
  storage->UpdateKAnonymity(blink::InterestGroupKey(g.owner, g.name),
                            {kanon_bid1, kanon_bid2, kanon_report2},
                            update_time,
                            /*replace_existing_values*/ true);
  expected_positive_keys = {kanon_bid1, kanon_bid2, kanon_report2};

  groups = storage->GetInterestGroupsForOwner(test_origin);

  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].hashed_kanon_keys,
              testing::UnorderedElementsAreArray(expected_positive_keys));
  EXPECT_THAT(groups[0].last_k_anon_updated, update_time);

  task_environment().FastForwardBy(base::Seconds(1));

  // Try doing a non-replacing update with the same update time as is already in
  // the database. It should succeed.
  storage->UpdateKAnonymity(blink::InterestGroupKey(g.owner, g.name),
                            {kanon_report1}, update_time,
                            /*replace_existing_values*/ false);
  expected_positive_keys.push_back(kanon_report1);
  groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].hashed_kanon_keys,
              testing::UnorderedElementsAreArray(expected_positive_keys));
  EXPECT_THAT(groups[0].last_k_anon_updated, update_time);

  task_environment().FastForwardBy(base::Seconds(1));

  // Try doing a non-replacing update with a later update time. It should
  // succeed.
  storage->UpdateKAnonymity(blink::InterestGroupKey(g.owner, g.name),
                            {kanon_component_1, kanon_bid2},
                            update_time + base::Seconds(10),
                            /*replace_existing_values*/ false);
  groups = storage->GetInterestGroupsForOwner(test_origin);
  expected_positive_keys.push_back(kanon_component_1);

  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].hashed_kanon_keys,
              testing::UnorderedElementsAreArray(expected_positive_keys));
  EXPECT_THAT(groups[0].last_k_anon_updated, update_time);

  task_environment().FastForwardBy(base::Seconds(1));

  // Try doing a non-replacing update with an earlier update time. We should
  // still get the same values as before.
  storage->UpdateKAnonymity(blink::InterestGroupKey(g.owner, g.name),
                            {kanon_component_2},
                            update_time - base::Seconds(10),
                            /*replace_existing_values*/ false);
  groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].hashed_kanon_keys,
              testing::UnorderedElementsAreArray(expected_positive_keys));
  EXPECT_THAT(groups[0].last_k_anon_updated, update_time);

  // An update with replace_existing_values = true and an earlier `update_time`
  // shouldn't result in any changed values.
  storage->UpdateKAnonymity(blink::InterestGroupKey(g.owner, g.name),
                            {kanon_component_2},
                            update_time - base::Seconds(10),
                            /*replace_existing_values*/ true);
  groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].hashed_kanon_keys,
              testing::UnorderedElementsAreArray(expected_positive_keys));
  EXPECT_THAT(groups[0].last_k_anon_updated, update_time);

  task_environment().FastForwardBy(base::Seconds(1));

  // A replacing update following non-replacing updates should update the
  // last_k_anon_updated and values for all k-anon.
  update_time = base::Time::Now();
  storage->UpdateKAnonymity(blink::InterestGroupKey(g.owner, g.name),
                            {kanon_bid1}, update_time,
                            /*replace_existing_values*/ true);
  expected_positive_keys = {kanon_bid1};
  groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].hashed_kanon_keys,
              testing::UnorderedElementsAreArray(expected_positive_keys));
  EXPECT_THAT(groups[0].last_k_anon_updated, update_time);
}

TEST_F(InterestGroupStorageTest,
       UpdatesAdKAnonymityWithMultipleInterestGroups) {
  url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  GURL ad1_url = GURL("https://owner.example.com/ad1");
  GURL ad2_url = GURL("https://owner.example.com/ad2");
  GURL ad3_url = GURL("https://owner.example.com/ad3");

  InterestGroup g1 = NewInterestGroup(test_origin, "name");
  g1.ads.emplace();
  g1.ads->emplace_back(ad1_url, "metadata1");
  g1.ad_components.emplace();
  g1.ad_components->emplace_back(ad1_url, "component_metadata1");
  g1.ad_components->emplace_back(ad3_url, "component_metadata3");
  g1.expiry = base::Time::Now() + InterestGroupStorage::kHistoryLength;

  InterestGroup g2 = g1;
  g2.ads->emplace_back(ad2_url, "metadata2");
  g2.name = "name 2";
  g2.expiry =
      base::Time::Now() + InterestGroupStorage::kHistoryLength + base::Hours(1);

  InterestGroup g3 = g1;
  g3.ad_components->clear();
  g3.name = "name 3";
  g3.expiry =
      base::Time::Now() + InterestGroupStorage::kHistoryLength + base::Hours(2);

  std::string k_anon_bid_key_1 =
      blink::HashedKAnonKeyForAdBid(g1, ad1_url.spec());
  std::string k_anon_bid_key_2 =
      blink::HashedKAnonKeyForAdBid(g2, ad2_url.spec());
  std::string k_anon_component_key_1 =
      blink::HashedKAnonKeyForAdComponentBid(ad1_url);
  std::string k_anon_component_key_3 =
      blink::HashedKAnonKeyForAdComponentBid(ad3_url);

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  // A true k-anonymity value should be returned with just one interest group.
  base::Time update_time1(base::Time::Now());
  storage->JoinInterestGroup(g1, GURL("example.com"));
  storage->UpdateKAnonymity(blink::InterestGroupKey(g1.owner, g1.name),
                            {k_anon_bid_key_1}, update_time1,
                            /*replace_existing_values*/ true);
  std::vector<StorageInterestGroup> returned_groups =
      storage->GetInterestGroupsForOwner(g1.owner);
  EXPECT_EQ(returned_groups.size(), 1u);
  EXPECT_THAT(returned_groups[0].hashed_kanon_keys,
              testing::UnorderedElementsAre(k_anon_bid_key_1));
  EXPECT_EQ(returned_groups[0].last_k_anon_updated, update_time1);

  task_environment().FastForwardBy(base::Hours(1));

  // The second interest group has not had a k-anon value update yet, so it
  // will not be returned with any k-anon values, despite it sharing a key
  // with the first group.
  storage->JoinInterestGroup(g2, GURL("example.com"));
  returned_groups = storage->GetInterestGroupsForOwner(g1.owner);
  {
    auto expected_interest_group_matcher = testing::UnorderedElementsAre(
        testing::AllOf(
            Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                  testing::UnorderedElementsAre(k_anon_bid_key_1)),
            Field("last_k_anon_updated",
                  &StorageInterestGroup::last_k_anon_updated, update_time1),
            Field(
                "interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(Field("owner", &InterestGroup::owner, g1.owner),
                               Field("name", &InterestGroup::name, g1.name)))),
        testing::AllOf(
            Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                  testing::IsEmpty()),
            Field(
                "interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(Field("owner", &InterestGroup::owner, g2.owner),
                               Field("name", &InterestGroup::name, g2.name)))));
    EXPECT_THAT(returned_groups, expected_interest_group_matcher);
  }

  // Updating a k-anon value for an ad only in the second interest group should
  // not affect the returned k-anonimity values for the first group.
  base::Time update_time2(base::Time::Now());
  storage->UpdateKAnonymity(blink::InterestGroupKey(g2.owner, g2.name),
                            {k_anon_bid_key_2}, update_time2,
                            /*replace_existing_values*/ true);
  returned_groups = storage->GetInterestGroupsForOwner(g1.owner);
  {
    auto expected_interest_group_matcher = testing::UnorderedElementsAre(
        testing::AllOf(
            Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                  testing::UnorderedElementsAre(k_anon_bid_key_1)),
            Field("last_k_anon_updated",
                  &StorageInterestGroup::last_k_anon_updated, update_time1),
            Field(
                "interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(Field("owner", &InterestGroup::owner, g1.owner),
                               Field("name", &InterestGroup::name, g1.name)))),
        testing::AllOf(
            Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                  testing::UnorderedElementsAre(k_anon_bid_key_2)),
            Field("last_k_anon_updated",
                  &StorageInterestGroup::last_k_anon_updated, update_time2),
            Field(
                "interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(Field("owner", &InterestGroup::owner, g2.owner),
                               Field("name", &InterestGroup::name, g2.name)))));
    EXPECT_THAT(returned_groups, expected_interest_group_matcher);
  }

  task_environment().FastForwardBy(base::Hours(1));

  // Updating a k-anon value for an ad in both interest groups should affect
  // only the interest group for which we are doing the update.
  base::Time update_time3(base::Time::Now());
  storage->UpdateKAnonymity(blink::InterestGroupKey(g1.owner, g1.name),
                            {k_anon_component_key_3}, update_time3,
                            /*replace_existing_values*/ true);
  returned_groups = storage->GetInterestGroupsForOwner(g1.owner);
  {
    auto expected_interest_group_matcher = testing::UnorderedElementsAre(
        testing::AllOf(
            Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                  testing::UnorderedElementsAre(k_anon_component_key_3)),
            Field("last_k_anon_updated",
                  &StorageInterestGroup::last_k_anon_updated, update_time3),
            Field(
                "interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(Field("owner", &InterestGroup::owner, g1.owner),
                               Field("name", &InterestGroup::name, g1.name)))),
        testing::AllOf(
            Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                  testing::UnorderedElementsAre(k_anon_bid_key_2)),
            Field("last_k_anon_updated",
                  &StorageInterestGroup::last_k_anon_updated, update_time2),
            Field(
                "interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(Field("owner", &InterestGroup::owner, g2.owner),
                               Field("name", &InterestGroup::name, g2.name)))));
    EXPECT_THAT(returned_groups, expected_interest_group_matcher);
  }

  // After joining a third interest group that shares k-anon keys with the
  // previously joined groups, the third interest group should be not be
  // returned with any k-anon keys set since the third group has not received a
  // k-anon update.
  storage->JoinInterestGroup(g3, GURL("example.com"));
  returned_groups = storage->GetInterestGroupsForOwner(g1.owner);
  {
    auto expected_interest_group_matcher = testing::UnorderedElementsAre(
        testing::AllOf(
            Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                  testing::UnorderedElementsAre(k_anon_component_key_3)),
            Field("last_k_anon_updated",
                  &StorageInterestGroup::last_k_anon_updated, update_time3),
            Field(
                "interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(Field("owner", &InterestGroup::owner, g1.owner),
                               Field("name", &InterestGroup::name, g1.name)))),
        testing::AllOf(
            Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                  testing::UnorderedElementsAre(k_anon_bid_key_2)),
            Field("last_k_anon_updated",
                  &StorageInterestGroup::last_k_anon_updated, update_time2),
            Field(
                "interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(Field("owner", &InterestGroup::owner, g2.owner),
                               Field("name", &InterestGroup::name, g2.name)))),
        testing::AllOf(
            Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                  testing::IsEmpty()),
            Field(
                "interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(Field("owner", &InterestGroup::owner, g3.owner),
                               Field("name", &InterestGroup::name, g3.name)))));
    EXPECT_THAT(returned_groups, expected_interest_group_matcher);
  }

  // Check that the k_anon_bid1 is unaffected by the expiration of the other
  // values (because it's attached to a newer interest group, g3).
  storage->UpdateKAnonymity(blink::InterestGroupKey(g3.owner, g3.name),
                            {k_anon_bid_key_1}, update_time3,
                            /*replace_existing_values*/ true);
  task_environment().FastForwardBy(InterestGroupStorage::kHistoryLength -
                                   base::Hours(1));

  returned_groups = storage->GetInterestGroupsForOwner(g1.owner);
  {
    auto expected_interest_group_matcher =
        testing::UnorderedElementsAre(testing::AllOf(
            Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                  testing::UnorderedElementsAre(k_anon_bid_key_1)),
            Field("last_k_anon_updated",
                  &StorageInterestGroup::last_k_anon_updated, update_time3),
            Field(
                "interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(Field("owner", &InterestGroup::owner, g3.owner),
                               Field("name", &InterestGroup::name, g3.name)))));
    EXPECT_EQ(returned_groups.size(), 1u);
    EXPECT_THAT(returned_groups, expected_interest_group_matcher);
  }
}

TEST_F(InterestGroupStorageTest, KAnonDataExpires) {
  GURL update_url("https://owner.example.com/groupUpdate");
  url::Origin test_origin = url::Origin::Create(update_url);
  const std::string name = "name";
  // We make the ad urls equal to the name key and update urls to verify the
  // database stores them separately.
  GURL ad1_url = GURL("https://owner.example.com/groupUpdate");
  GURL ad2_url = GURL("https://owner.example.com/name");

  InterestGroup g = NewInterestGroup(test_origin, name);
  blink::InterestGroupKey interest_group_key(g.owner, g.name);
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
  std::string ad1_bid_kanon = blink::HashedKAnonKeyForAdBid(g, ad1_url.spec());
  std::string ad1_report_kanon = blink::HashedKAnonKeyForAdNameReporting(
      g, g.ads.value()[0],
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt);
  std::string ad2_bid_kanon = blink::HashedKAnonKeyForAdComponentBid(ad2_url);
  storage->UpdateKAnonymity(interest_group_key,
                            {ad1_bid_kanon, ad1_report_kanon, ad2_bid_kanon},
                            update_kanon_time,
                            /*replace_existing_values*/ true);

  // Check k-anonymity data was correctly set.
  std::vector<StorageInterestGroup> groups =
      storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, groups.size());
  EXPECT_THAT(groups[0].hashed_kanon_keys,
              testing::UnorderedElementsAre(ad1_bid_kanon, ad1_report_kanon,
                                            ad2_bid_kanon));

  // Fast-forward past interest group expiration.
  task_environment().FastForwardBy(base::Days(2));

  // Interest group should no longer exist.
  groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(0u, groups.size());

  // Join again and expect empty kanon values.
  g.expiry = base::Time::Now() + base::Days(1);
  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join3"));

  groups = storage->GetInterestGroupsForOwner(test_origin);
  ASSERT_EQ(1u, groups.size());
  EXPECT_TRUE(groups[0].hashed_kanon_keys.empty());
}

TEST_F(InterestGroupStorageTest, StoresAllFields) {
  StoresAllFieldsTest();
}

#if !BUILDFLAG(IS_IOS)
// std::system is not available on iOS.
TEST_F(InterestGroupStorageTest, DumpAllIgFields) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch("dump-all-ig-fields")) {
    // This is not part of the proper test, but rather serves as a utility run
    // on developer workstations for generating new autogenSchemaV[n].sql files
    // from the current database -- these are used by MultiVersionUpgradeTest.
    {
      blink::InterestGroup full = ProduceAllFields();
      std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
      storage->JoinInterestGroup(full, kFullOrigin.GetURL());
    }

    base::FilePath out_sql_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &out_sql_path);
    out_sql_path = out_sql_path.AppendASCII(base::StringPrintf(
        "content/test/data/interest_group/autogenSchemaV%d.sql",
        InterestGroupStorage::GetCurrentVersionNumberForTesting()));
    // NOTE: This command will be run on POSIX and Windows workstations. To see
    // command line output for debugging, redirect it to a file.
    std::string dump_db_command = base::StringPrintf(
        "sqlite3 %s .dump > %s", db_path().MaybeAsASCII().c_str(),
        out_sql_path.MaybeAsASCII().c_str());
    LOG(INFO) << "--dump-all-ig-fields command (make sure sqlite3 is in $PATH "
                 "/ %PATH%): "
              << dump_db_command;
    LOG(INFO) << "sqlite3 can be installed from a package from your OS, or "
                 "built from the Chromium repo via the `sqlite_shell` GN "
                 "target -- just make sure to rename it to / have a symlink "
                 "called sqlite3 on the path.";
    if (base::CommandLine::ForCurrentProcess()->HasSwitch("dry-run")) {
      LOG(INFO) << "--dump-all-ig-fields command not run due to --dry-run";
    } else {
      LOG(INFO) << "Running --dump-all-ig-fields command";
      EXPECT_EQ(0, std::system(dump_db_command.c_str()));
    }
  }
}
#endif

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

  std::string k_anon_key = blink::HashedKAnonKeyForAdBid(g1, ad1_url.spec());

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
TEST_F(InterestGroupStorageTest, JoinTooManyRegularGroupNames) {
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

// Maintenance should prune the number of interest groups and interest group
// owners based on the set limit.
TEST_F(InterestGroupStorageTest, JoinTooManyNegativeGroupNames) {
  base::HistogramTester histograms;
  const size_t kExcessOwners = 10;
  const url::Origin test_origin =
      url::Origin::Create(GURL("https://owner.example.com"));
  const size_t max_negative_groups_per_owner =
      blink::features::kInterestGroupStorageMaxNegativeGroupsPerOwner.Get();
  const size_t num_groups = max_negative_groups_per_owner + kExcessOwners;
  std::vector<std::string> added_groups;

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  for (size_t i = 0; i < num_groups; i++) {
    const std::string group_name = base::NumberToString(i);
    // Allow time to pass so that they have different expiration times.
    // This makes which groups get removed deterministic as they are sorted by
    // expiration time.
    task_environment().FastForwardBy(base::Microseconds(1));

    storage->JoinInterestGroup(
        NewNegativeInterestGroup(test_origin, group_name),
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
  ASSERT_EQ(max_negative_groups_per_owner, interest_groups.size());
  histograms.ExpectBucketCount("Storage.InterestGroup.PerSiteCount",
                               max_negative_groups_per_owner, 1);
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
  const size_t kExcessGroups = 4;
  const url::Origin kTestOrigin =
      url::Origin::Create(GURL("https://owner.example.com"));
  const size_t kGroupSize = 800;
  const size_t groups_before_full =
      blink::features::kInterestGroupStorageMaxStoragePerOwner.Get() /
      kGroupSize;
  std::vector<std::string> added_groups;

  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  for (size_t i = 0; i < groups_before_full - 1; i++) {
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

  const std::string big_group_name =
      base::NumberToString(groups_before_full - 1);
  task_environment().FastForwardBy(base::Microseconds(1));
  blink::InterestGroup big_group =
      NewInterestGroup(kTestOrigin, big_group_name);
  // Let the group be just the size left to reach
  // `kInterestGroupStorageMaxStoragePerOwner` plus 1, so that this group will
  // be removed during maintenance. This also guarantees its size is greater
  // than `kGroupSize`, so that once this group is removed, one more group of
  // `kGroupSize` can be stored.
  size_t size_left_before_full =
      blink::features::kInterestGroupStorageMaxStoragePerOwner.Get() -
      kGroupSize * (groups_before_full - 1);
  big_group.user_bidding_signals =
      std::string(size_left_before_full - big_group.EstimateSize() + 1, 'P');
  EXPECT_GT(big_group.EstimateSize(), kGroupSize);
  storage->JoinInterestGroup(big_group, kTestOrigin.GetURL());
  added_groups.push_back(big_group_name);

  for (size_t i = groups_before_full; i < groups_before_full + kExcessGroups;
       i++) {
    const std::string group_name = base::NumberToString(i);
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
  std::vector<std::string> remaining_groups_expected;
  // Interest group `groups_before_full` - 1 is removed and one more interest
  // group `groups_before_full` - 2 can be kept, since the total size is still
  // within `kInterestGroupStorageMaxStoragePerOwner` with it.
  for (size_t i = groups_before_full + kExcessGroups - 1;
       i >= groups_before_full - 2; i--) {
    if (i != groups_before_full - 1) {
      remaining_groups_expected.push_back(base::NumberToString(i));
    }
  }
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

TEST_F(InterestGroupStorageTest, ExpiredGroupsNotReturned) {
  const char kName1[] = "name1";
  const char kName2[] = "name2";
  const char kName3[] = "name3";
  const url::Origin kOrigin = url::Origin::Create(GURL("https://owner.test"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  const base::TimeDelta kDelta = base::Seconds(1);

  base::Time start = base::Time::Now();
  base::Time later = start + kDelta;
  base::Time even_later = later + kDelta;

  // Already expired when joined.
  storage->JoinInterestGroup(
      blink::TestInterestGroupBuilder(kOrigin, kName1).SetExpiry(start).Build(),
      kOrigin.GetURL());

  // Expires when time reaches `later`.
  storage->JoinInterestGroup(
      blink::TestInterestGroupBuilder(kOrigin, kName2).SetExpiry(later).Build(),
      kOrigin.GetURL());

  // Expires when time reaches `even_later`.
  storage->JoinInterestGroup(blink::TestInterestGroupBuilder(kOrigin, kName3)
                                 .SetExpiry(even_later)
                                 .Build(),
                             kOrigin.GetURL());

  // All but the first group, which is already expired, should be retrieved.
  auto interest_groups = storage->GetInterestGroupsForOwner(kOrigin);
  ASSERT_EQ(2u, interest_groups.size());
  EXPECT_THAT(
      (std::vector<std::string>{interest_groups[0].interest_group.name,
                                interest_groups[1].interest_group.name}),
      testing::UnorderedElementsAre(kName2, kName3));

  // Wait until `later`. The second group should expire.
  task_environment().FastForwardBy(kDelta);
  interest_groups = storage->GetInterestGroupsForOwner(kOrigin);
  ASSERT_EQ(1u, interest_groups.size());
  ASSERT_EQ(interest_groups[0].interest_group.name, kName3);

  // Wait until `even_later`. All three interest groups should now be expired.
  task_environment().FastForwardBy(kDelta);
  interest_groups = storage->GetInterestGroupsForOwner(kOrigin);
  EXPECT_EQ(0u, interest_groups.size());
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
  for (const auto& origin : test_origins) {
    storage->JoinInterestGroup(NewInterestGroup(origin, "discard"),
                               origin.GetURL());
  }

  std::vector<url::Origin> origins = storage->GetAllInterestGroupOwners();
  EXPECT_EQ(3u, origins.size());

  std::vector<StorageInterestGroup> interest_groups =
      storage->GetInterestGroupsForOwner(keep_origin);
  EXPECT_EQ(2u, interest_groups.size());
  base::Time next_maintenance_time =
      base::Time::Now() + InterestGroupStorage::kIdlePeriod;

  // Maintenance should not have run yet as we are not idle.
  EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(),
            original_maintenance_time);

  task_environment().FastForwardBy(InterestGroupStorage::kIdlePeriod -
                                   base::Seconds(1));

  // Maintenance should not have run yet as we are not idle.
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

  // Maintenance should not have run since we have not been idle.
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

// Test that when an interest group expires, data about the expired group from
// the additional tables (`prev_wins`, `join_count`, `num_bids`) is not
// preserved if the interest group is joined again. This tests both the case
// where the expired group is destroyed by normal database maintenance, and the
// case where it's overwritten by a new group with the same name and owner
// before maintenance can be performed.
TEST_F(InterestGroupStorageTest, ExpirationDeletesMetadata) {
  base::HistogramTester histograms;

  enum class TestCase {
    // The expired group is destroyed by periodic database maintenance.
    kDestroyedByMaintenance,
    // The expired group is overwritten by a new group before database
    // maintenance has had a chance to destroy it.
    kOverwrittenByNewGroup
  };

  const url::Origin kOrigin = url::Origin::Create(GURL("https://owner.test"));
  const char kName[] = "name";
  const blink::InterestGroupKey kGroupKey(kOrigin, kName);
  const char kAdJson[] = "{url: 'https://ad.test/'}";

  for (auto test_case :
       {TestCase::kDestroyedByMaintenance, TestCase::kOverwrittenByNewGroup}) {
    SCOPED_TRACE(static_cast<int>(test_case));
    std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

    base::Time start = base::Time::Now();
    const base::TimeDelta kDelta = base::Seconds(1);

    // Join the group, and record a bid and win.
    storage->JoinInterestGroup(blink::TestInterestGroupBuilder(kOrigin, kName)
                                   .SetExpiry(start + kDelta)
                                   .Build(),
                               kOrigin.GetURL());
    storage->RecordInterestGroupBids({kGroupKey});
    storage->RecordInterestGroupWin(kGroupKey, kAdJson);

    // Check that the interest group can be retrieved, and all relevant fields
    // are correct.
    std::vector<StorageInterestGroup> interest_groups =
        storage->GetInterestGroupsForOwner(kOrigin);
    ASSERT_EQ(1u, interest_groups.size());
    EXPECT_EQ(kName, interest_groups[0].interest_group.name);
    EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
    EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->bid_count);
    ASSERT_EQ(1u, interest_groups[0].bidding_browser_signals->prev_wins.size());
    EXPECT_EQ(
        kAdJson,
        interest_groups[0].bidding_browser_signals->prev_wins[0]->ad_json);

    switch (test_case) {
      case TestCase::kDestroyedByMaintenance: {
        base::Time expected_maintenance_time =
            base::Time::Now() + InterestGroupStorage::kIdlePeriod;
        // Enough time to trigger maintenance.
        task_environment().FastForwardBy(InterestGroupStorage::kIdlePeriod +
                                         base::Seconds(1));
        // Verify that maintenance has run.
        EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(),
                  expected_maintenance_time);
        break;
      }

      case TestCase::kOverwrittenByNewGroup: {
        base::Time old_maintenance_time =
            storage->GetLastMaintenanceTimeForTesting();
        // Not enough time to trigger maintenance.
        task_environment().FastForwardBy(base::Seconds(1));
        // Maintenance should not have been performed.
        EXPECT_EQ(storage->GetLastMaintenanceTimeForTesting(),
                  old_maintenance_time);
        break;
      }
    }

    // Whether or not it's still in the database, GetInterestGroupsForOwner()
    // should not retrieve the expired group.
    interest_groups = storage->GetInterestGroupsForOwner(kOrigin);
    EXPECT_EQ(0u, interest_groups.size());

    // Re-join the interest group.
    storage->JoinInterestGroup(
        blink::TestInterestGroupBuilder(kOrigin, kName).Build(),
        kOrigin.GetURL());

    // Retrieve the group. Its `join_count`, `bid_count`, and `prev_wins` should
    // not reflect data from the first time the group was joined.
    interest_groups = storage->GetInterestGroupsForOwner(kOrigin);
    ASSERT_EQ(1u, interest_groups.size());
    EXPECT_EQ(kName, interest_groups[0].interest_group.name);
    EXPECT_EQ(1, interest_groups[0].bidding_browser_signals->join_count);
    EXPECT_EQ(0, interest_groups[0].bidding_browser_signals->bid_count);
    EXPECT_EQ(0u, interest_groups[0].bidding_browser_signals->prev_wins.size());

    // Leave the interest group so it doesn't affect the next test.
    storage->LeaveInterestGroup(kGroupKey, kOrigin);
  }
}

// Upgrades a v6 database dump to an expected current database.
// The v6 database dump was extracted from the InterestGroups database in
// a browser profile by using `sqlite3 dump <path-to-database>` and then
// cleaning up and formatting the output.
TEST_F(InterestGroupStorageTest, UpgradeFromV6) {
  // Create V6 database from dump
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
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
                  Field("name", &InterestGroup::name,
                        "groupNullUserBiddingSignals"),
                  Field("priority", &InterestGroup::priority, 0.0),
                  Field("enable_bidding_signals_prioritization",
                        &InterestGroup::enable_bidding_signals_prioritization,
                        false),
                  Field("priority_vector", &InterestGroup::priority_vector,
                        std::nullopt),
                  Field("priority_signals_overrides",
                        &InterestGroup::priority_signals_overrides,
                        std::nullopt),
                  Field("seller_capabilities",
                        &InterestGroup::seller_capabilities, std::nullopt),
                  Field("all_sellers_capabilities",
                        &InterestGroup::all_sellers_capabilities,
                        SellerCapabilitiesType()),
                  Field("bidding_url", &InterestGroup::bidding_url,
                        GURL("https://owner.example.com/bidder.js")),
                  Field("bidding_wasm_helper_url",
                        &InterestGroup::bidding_wasm_helper_url, std::nullopt),
                  Field("update_url", &InterestGroup::update_url,
                        GURL("https://owner.example.com/update")),
                  Field("trusted_bidding_signals_url",
                        &InterestGroup::trusted_bidding_signals_url,
                        GURL("https://owner.example.com/signals")),
                  Field(
                      "trusted_bidding_signals_keys",
                      &InterestGroup::trusted_bidding_signals_keys,
                      std::vector<std::string>{"groupNullUserBiddingSignals"}),
                  Field(
                      "trusted_bidding_signals_slot_size_mode",
                      &InterestGroup::trusted_bidding_signals_slot_size_mode,
                      InterestGroup::TrustedBiddingSignalsSlotSizeMode::kNone),
                  Field("max_trusted_bidding_signals_url_length",
                        &InterestGroup::max_trusted_bidding_signals_url_length,
                        0),
                  Field("trusted_bidding_signals_coordinator",
                        &InterestGroup::trusted_bidding_signals_coordinator,
                        std::nullopt),
                  Field("user_bidding_signals",
                        &InterestGroup::user_bidding_signals, std::nullopt),
                  Field("ads", &InterestGroup::ads,
                        testing::Property(
                            "value()",
                            &std::optional<
                                std::vector<blink::InterestGroup::Ad>>::value,
                            testing::ElementsAre(testing::AllOf(
                                Property("render_url",
                                         &InterestGroup::Ad::render_url,
                                         GURL("https://ads.example.com/1")),
                                Field("metadata", &InterestGroup::Ad::metadata,
                                      "[\"4\",\"5\",null,\"6\"]"))))),
                  Field("ad_components", &InterestGroup::ad_components,
                        std::nullopt),
                  Field("ad_sizes", &InterestGroup::ad_components,
                        std::nullopt),
                  Field("size_groups", &InterestGroup::ad_components,
                        std::nullopt))),
          Field("bidding_browser_signals",
                &StorageInterestGroup::bidding_browser_signals,
                testing::AllOf(
                    Pointee(Field(
                        "join_count",
                        &blink::mojom::BiddingBrowserSignals::join_count, 0)),
                    Pointee(Field(
                        "bid_count",
                        &blink::mojom::BiddingBrowserSignals::bid_count, 0)))),
          Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                testing::IsEmpty()),
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
                            base::Microseconds(13293932603076872))),
                  Field("owner", &InterestGroup::owner,
                        url::Origin::Create(GURL("https://owner.example.com"))),
                  Field("name", &InterestGroup::name, "group1"),
                  Field("priority", &InterestGroup::priority, 0.0),
                  Field("enable_bidding_signals_prioritization",
                        &InterestGroup::enable_bidding_signals_prioritization,
                        false),
                  Field("priority_vector", &InterestGroup::priority_vector,
                        std::nullopt),
                  Field("priority_signals_overrides",
                        &InterestGroup::priority_signals_overrides,
                        std::nullopt),
                  Field("seller_capabilities",
                        &InterestGroup::seller_capabilities, std::nullopt),
                  Field("all_sellers_capabilities",
                        &InterestGroup::all_sellers_capabilities,
                        SellerCapabilitiesType()),
                  Field("bidding_url", &InterestGroup::bidding_url,
                        GURL("https://owner.example.com/bidder.js")),
                  Field("bidding_wasm_helper_url",
                        &InterestGroup::bidding_wasm_helper_url, std::nullopt),
                  Field("update_url", &InterestGroup::update_url,
                        GURL("https://owner.example.com/update")),
                  Field("trusted_bidding_signals_url",
                        &InterestGroup::trusted_bidding_signals_url,
                        GURL("https://owner.example.com/signals")),
                  Field("trusted_bidding_signals_keys",
                        &InterestGroup::trusted_bidding_signals_keys,
                        std::vector<std::string>{"group1"}),
                  Field(
                      "trusted_bidding_signals_slot_size_mode",
                      &InterestGroup::trusted_bidding_signals_slot_size_mode,
                      InterestGroup::TrustedBiddingSignalsSlotSizeMode::kNone),
                  Field("max_trusted_bidding_signals_url_length",
                        &InterestGroup::max_trusted_bidding_signals_url_length,
                        0),
                  Field("trusted_bidding_signals_coordinator",
                        &InterestGroup::trusted_bidding_signals_coordinator,
                        std::nullopt),
                  Field("user_bidding_signals",
                        &InterestGroup::user_bidding_signals,
                        "[[\"1\",\"2\"]]"),
                  Field("ads", &InterestGroup::ads,
                        testing::Property(
                            "value()",
                            &std::optional<
                                std::vector<blink::InterestGroup::Ad>>::value,
                            testing::ElementsAre(testing::AllOf(
                                Property("render_url",
                                         &InterestGroup::Ad::render_url,
                                         GURL("https://ads.example.com/1")),
                                Field("metadata", &InterestGroup::Ad::metadata,
                                      "[\"4\",\"5\",null,\"6\"]"))))),
                  Field("ad_components", &InterestGroup::ad_components,
                        std::nullopt),
                  Field("ad_sizes", &InterestGroup::ad_components,
                        std::nullopt),
                  Field("size_groups", &InterestGroup::ad_components,
                        std::nullopt))),
          Field("bidding_browser_signals",
                &StorageInterestGroup::bidding_browser_signals,
                testing::AllOf(
                    Pointee(Field(
                        "join_count",
                        &blink::mojom::BiddingBrowserSignals::join_count, 5)),
                    Pointee(Field(
                        "bid_count",
                        &blink::mojom::BiddingBrowserSignals::bid_count, 4)))),
          Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                testing::IsEmpty()),
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
                        std::nullopt),
                  Field("priority_signals_overrides",
                        &InterestGroup::priority_signals_overrides,
                        std::nullopt),
                  Field("seller_capabilities",
                        &InterestGroup::seller_capabilities, std::nullopt),
                  Field("all_sellers_capabilities",
                        &InterestGroup::all_sellers_capabilities,
                        SellerCapabilitiesType()),
                  Field("bidding_url", &InterestGroup::bidding_url,
                        GURL("https://owner.example.com/bidder.js")),
                  Field("bidding_wasm_helper_url",
                        &InterestGroup::bidding_wasm_helper_url, std::nullopt),
                  Field("update_url", &InterestGroup::update_url,
                        GURL("https://owner.example.com/update")),
                  Field("trusted_bidding_signals_url",
                        &InterestGroup::trusted_bidding_signals_url,
                        GURL("https://owner.example.com/signals")),
                  Field("trusted_bidding_signals_keys",
                        &InterestGroup::trusted_bidding_signals_keys,
                        std::vector<std::string>{"group2"}),
                  Field(
                      "trusted_bidding_signals_slot_size_mode",
                      &InterestGroup::trusted_bidding_signals_slot_size_mode,
                      InterestGroup::TrustedBiddingSignalsSlotSizeMode::kNone),
                  Field("max_trusted_bidding_signals_url_length",
                        &InterestGroup::max_trusted_bidding_signals_url_length,
                        0),
                  Field("trusted_bidding_signals_coordinator",
                        &InterestGroup::trusted_bidding_signals_coordinator,
                        std::nullopt),
                  Field("user_bidding_signals",
                        &InterestGroup::user_bidding_signals,
                        "[[\"1\",\"3\"]]"),
                  Field("ads", &InterestGroup::ads,
                        testing::Property(
                            "value()",
                            &std::optional<
                                std::vector<blink::InterestGroup::Ad>>::value,
                            testing::ElementsAre(testing::AllOf(
                                Property("render_url",
                                         &InterestGroup::Ad::render_url,
                                         GURL("https://ads.example.com/1")),
                                Field("metadata", &InterestGroup::Ad::metadata,
                                      "[\"4\",\"5\",null,\"6\"]"))))),
                  Field("ad_components", &InterestGroup::ad_components,
                        std::nullopt),
                  Field("ad_sizes", &InterestGroup::ad_components,
                        std::nullopt),
                  Field("size_groups", &InterestGroup::ad_components,
                        std::nullopt))),
          Field("bidding_browser_signals",
                &StorageInterestGroup::bidding_browser_signals,
                testing::AllOf(
                    Pointee(Field(
                        "join_count",
                        &blink::mojom::BiddingBrowserSignals::join_count, 5)),
                    Pointee(Field(
                        "bid_count",
                        &blink::mojom::BiddingBrowserSignals::bid_count, 3)))),
          Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                testing::IsEmpty()),
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
                        std::nullopt),
                  Field("priority_signals_overrides",
                        &InterestGroup::priority_signals_overrides,
                        std::nullopt),
                  Field("seller_capabilities",
                        &InterestGroup::seller_capabilities, std::nullopt),
                  Field("all_sellers_capabilities",
                        &InterestGroup::all_sellers_capabilities,
                        SellerCapabilitiesType()),
                  Field("bidding_url", &InterestGroup::bidding_url,
                        GURL("https://owner.example.com/bidder.js")),
                  Field("bidding_wasm_helper_url",
                        &InterestGroup::bidding_wasm_helper_url, std::nullopt),
                  Field("update_url", &InterestGroup::update_url,
                        GURL("https://owner.example.com/update")),
                  Field("trusted_bidding_signals_url",
                        &InterestGroup::trusted_bidding_signals_url,
                        GURL("https://owner.example.com/signals")),
                  Field("trusted_bidding_signals_keys",
                        &InterestGroup::trusted_bidding_signals_keys,
                        std::vector<std::string>{"group3"}),
                  Field(
                      "trusted_bidding_signals_slot_size_mode",
                      &InterestGroup::trusted_bidding_signals_slot_size_mode,
                      InterestGroup::TrustedBiddingSignalsSlotSizeMode::kNone),
                  Field("max_trusted_bidding_signals_url_length",
                        &InterestGroup::max_trusted_bidding_signals_url_length,
                        0),
                  Field("trusted_bidding_signals_coordinator",
                        &InterestGroup::trusted_bidding_signals_coordinator,
                        std::nullopt),
                  Field("user_bidding_signals",
                        &InterestGroup::user_bidding_signals,
                        "[[\"3\",\"2\"]]"),
                  Field("ads", &InterestGroup::ads,
                        testing::Property(
                            "value()",
                            &std::optional<
                                std::vector<blink::InterestGroup::Ad>>::value,
                            testing::ElementsAre(testing::AllOf(
                                Property("render_url",
                                         &InterestGroup::Ad::render_url,
                                         GURL("https://ads.example.com/1")),
                                Field("metadata", &InterestGroup::Ad::metadata,
                                      "[\"4\",\"5\",null,\"6\"]"))))),
                  Field("ad_components", &InterestGroup::ad_components,
                        std::nullopt),
                  Field("ad_sizes", &InterestGroup::ad_components,
                        std::nullopt),
                  Field("size_groups", &InterestGroup::ad_components,
                        std::nullopt))),
          Field("bidding_browser_signals",
                &StorageInterestGroup::bidding_browser_signals,
                testing::AllOf(
                    Pointee(Field(
                        "join_count",
                        &blink::mojom::BiddingBrowserSignals::join_count, 4)),
                    Pointee(Field(
                        "bid_count",
                        &blink::mojom::BiddingBrowserSignals::bid_count, 4)))),
          Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                testing::IsEmpty()),
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
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
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

// Upgrades a v16 database dump to an expected current database.
// The v16 database dump was extracted from an updated version of
// the v6 data dump, then altered to have new k-anon keys --
// the format of k-anon keys has changed between the two versions
// (without migration of one key type to the next)
// so this new dump is necessary to test changes to the k-anon
// table.
TEST_F(InterestGroupStorageTest, UpgradeFromV16) {
  // Create V16 database from dump
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
  file_path =
      file_path.AppendASCII("content/test/data/interest_group/schemaV16.sql");
  ASSERT_TRUE(base::PathExists(file_path));
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(), file_path));

  std::string k_anon_bid = crypto::SHA256HashString(
      "AdBid\n"
      "https://owner.example.com/\n"
      "https://owner.example.com/bidder.js\n"
      "https://ads.example.com/1");
  auto expected_interest_group_matcher = testing::UnorderedElementsAre(
      testing::AllOf(
          Field("interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(Field("name", &InterestGroup::name, "group1"))),
          Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                testing::UnorderedElementsAre(k_anon_bid)),
          // The oldest update time for any key of this interest group should be
          // used. The oldest time is Time::Min() because not all keys for this
          // interest group were in the v16 schema.
          Field("last_k_anon_updated",
                &StorageInterestGroup::last_k_anon_updated, base::Time::Min())),
      testing::AllOf(
          Field("interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(Field("name", &InterestGroup::name, "group2"))),
          Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                testing::UnorderedElementsAre(k_anon_bid)),
          // The oldest update time for any key of this interest group should be
          // used.
          Field("last_k_anon_updated",
                &StorageInterestGroup::last_k_anon_updated,
                base::Time::Min() + base::Microseconds(3))),
      testing::AllOf(
          Field("interest_group", &StorageInterestGroup::interest_group,
                testing::AllOf(Field("name", &InterestGroup::name, "group3"))),
          Field("hashed_kanon_keys", &StorageInterestGroup::hashed_kanon_keys,
                testing::UnorderedElementsAre(k_anon_bid)),
          // The oldest update time for any key of this interest group should be
          // used. The oldest time is Time::Min() because not all keys for this
          // interest group were in the v16 schema.
          Field("last_k_anon_updated",
                &StorageInterestGroup::last_k_anon_updated,
                base::Time::Min())));

  // Upgrade and read.
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  ASSERT_TRUE(storage);

  std::vector<StorageInterestGroup> interest_groups =
      storage->GetAllInterestGroupsUnfilteredForTesting();

  EXPECT_THAT(interest_groups, expected_interest_group_matcher);

  // In the v16 table, there was a k-anon key that doesn't correspond with an
  // interest group in the interest group table -- make sure this was migrated
  // as well.
  std::string key_without_ig_in_ig_table = crypto::SHA256HashString(
      "AdBid\nhttps://owner.example2.com/\nhttps://owner.example2.com/"
      "bidder.js\nhttps://ads.example2.com/1");
  std::optional<base::Time> last_reported =
      storage->GetLastKAnonymityReported(key_without_ig_in_ig_table);
  EXPECT_EQ(last_reported, base::Time::Min() + base::Microseconds(8));
}

TEST_F(InterestGroupStorageTest, MultiVersionUpgradeTest) {
  constexpr char kMisssingFileError[] =
      "You can generate the missing .sql file for the current database "
      "version by running: "
      "`out/Default/content_unittests "
      "--gtest_filter=\"*InterestGroupStorage*Test*DumpAllIgFields\" "
      "--dump-all-ig-fields` after installing sqlite3 from your package "
      "manager -- you can also build the Chromium `sqlite_shell` GN "
      "target and rename / symlink it on your path as sqlite3. \n\n***Make "
      "sure to add the generated file to source control***.\n\n";
  for (int i = kOldestAllFieldsVersion;
       i <= InterestGroupStorage::GetCurrentVersionNumberForTesting() - 1;
       i++) {
    SCOPED_TRACE(i);

    base::FilePath file_path;
    base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
    file_path = file_path.AppendASCII(base::StringPrintf(
        "content/test/data/interest_group/autogenSchemaV%d.sql", i));
    ASSERT_TRUE(base::PathExists(file_path))
        << "Older .sql file " << file_path
        << " missing -- somehow it wasn't committed when the new "
           "version was introduced? Anyways, you can use `git reset --hard` to "
           "go back to a commit with that version to regenerate it. Once at "
           "that commit: \n\n"
        << kMisssingFileError;

    ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path(), file_path));

    blink::InterestGroup expected = ProduceAllFields(i);

    // Upgrade and read.
    {
      std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
      ASSERT_TRUE(storage);

      std::vector<StorageInterestGroup> interest_groups =
          storage->GetAllInterestGroupsUnfilteredForTesting();

      ASSERT_EQ(1u, interest_groups.size());
      const blink::InterestGroup& actual = interest_groups[0].interest_group;
      // Don't compare `expiry` as it changes every test run.
      expected.expiry = actual.expiry;
      IgExpectEqualsForTesting(expected, actual);
    }

    // Make sure the database still works if we open it again.
    {
      std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
      std::vector<StorageInterestGroup> interest_groups =
          storage->GetAllInterestGroupsUnfilteredForTesting();

      ASSERT_EQ(1u, interest_groups.size());
      const blink::InterestGroup& actual = interest_groups[0].interest_group;
      // Don't compare `expiry` as it changes every test run.
      expected.expiry = actual.expiry;
      IgExpectEqualsForTesting(expected, actual);

      bool version_changed_ig_fields;
      blink::InterestGroup next_version_expected =
          ProduceAllFields(i + 1, &version_changed_ig_fields);
      if (version_changed_ig_fields) {
        // Make sure IgExpect[Not]EqualsForTesting() gets updated to compare the
        // newly introduced field(s).
        next_version_expected.expiry = actual.expiry;
        IgExpectNotEqualsForTesting(next_version_expected, actual);
      }
    }

    // Delete the database in case we loop again, creating the database from
    // another .sql file.
    base::DeleteFile(db_path());
  }

  // Make sure the current version .sql dump gets produced when introducing new
  // versions -- it's up to the author to add it to the CL.
  base::FilePath file_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &file_path);
  file_path = file_path.AppendASCII(base::StringPrintf(
      "content/test/data/interest_group/autogenSchemaV%d.sql",
      InterestGroupStorage::GetCurrentVersionNumberForTesting()));
  ASSERT_TRUE(base::PathExists(file_path))
      << "Missing " << file_path << " -- " << kMisssingFileError;
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

  std::string k_anon_key_1 = blink::HashedKAnonKeyForAdBid(g, ad1_url.spec());
  std::string k_anon_key_2 = blink::HashedKAnonKeyForAdBid(g, ad2_url.spec());
  std::string k_anon_key_3 = blink::HashedKAnonKeyForAdComponentBid(ad3_url);

  std::optional<base::Time> last_report =
      storage->GetLastKAnonymityReported(k_anon_key_1);
  EXPECT_EQ(base::Time::Min(), last_report);  // Not in the database.

  // Setting a last reported time for a key that doesn't correspond with an
  // interest group should work.
  base::Time expected_last_report = base::Time::Now();
  storage->UpdateLastKAnonymityReported(k_anon_key_3);
  task_environment().FastForwardBy(base::Seconds(1));
  last_report = storage->GetLastKAnonymityReported(k_anon_key_3);
  EXPECT_EQ(expected_last_report, last_report);

  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join"));

  // After joining an interest group for a previously joined key, we should
  // still get the same time.
  last_report = storage->GetLastKAnonymityReported(k_anon_key_3);
  EXPECT_EQ(expected_last_report, last_report);

  last_report = storage->GetLastKAnonymityReported(k_anon_key_1);
  EXPECT_EQ(last_report, base::Time::Min());
  storage->UpdateLastKAnonymityReported(k_anon_key_1);
  expected_last_report = base::Time::Now();

  task_environment().FastForwardBy(base::Seconds(1));

  last_report = storage->GetLastKAnonymityReported(k_anon_key_1);
  EXPECT_EQ(last_report, expected_last_report);

  task_environment().FastForwardBy(base::Seconds(1));

  last_report = storage->GetLastKAnonymityReported(k_anon_key_2);
  EXPECT_EQ(last_report, base::Time::Min());
  storage->UpdateLastKAnonymityReported(k_anon_key_2);
  expected_last_report = base::Time::Now();

  task_environment().FastForwardBy(base::Seconds(1));

  last_report = storage->GetLastKAnonymityReported(k_anon_key_2);
  EXPECT_EQ(last_report, expected_last_report);

  task_environment().FastForwardBy(base::Seconds(1));

  storage->UpdateLastKAnonymityReported(k_anon_key_3);
  expected_last_report = base::Time::Now();

  task_environment().FastForwardBy(base::Seconds(1));

  last_report = storage->GetLastKAnonymityReported(k_anon_key_3);
  EXPECT_EQ(last_report, expected_last_report);

  task_environment().FastForwardBy(base::Seconds(1));

  std::string group_name_key = blink::HashedKAnonKeyForAdNameReporting(
      g, g.ads->at(0),
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt);
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
  IgExpectEqualsForTesting(/*actual=*/storage_interest_groups[0].interest_group,
                           /*expected=*/original_group);

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
  blink::InterestGroupKey interest_group_key(g.owner, g.name);
  g.ads.emplace();
  g.ads->push_back(blink::InterestGroup::Ad(ad1_url, "metadata1"));
  g.ads->push_back(blink::InterestGroup::Ad(ad2_url, "metadata2"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();

  std::string k_anon_key_1 = blink::HashedKAnonKeyForAdBid(g, ad1_url.spec());
  std::string k_anon_key_2 = blink::HashedKAnonKeyForAdBid(g, ad2_url.spec());

  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join"));

  std::vector<StorageInterestGroup> groups =
      storage->GetInterestGroupsForOwner(test_origin);

  storage->UpdateLastKAnonymityReported(k_anon_key_1);
  storage->UpdateLastKAnonymityReported(k_anon_key_2);

  EXPECT_NE(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_1));
  EXPECT_NE(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_2));

  task_environment().FastForwardBy(
      InterestGroupStorage::kAdditionalKAnonStoragePeriod);

  g.ads->pop_back();  // Erasing an ad from an interest group should not have an
                      // effect on how long we store its last reported time.
  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join"));

  // The k-anon values should remain a day after their last-updated time.
  EXPECT_NE(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_1));
  EXPECT_NE(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_2));

  task_environment().FastForwardBy(InterestGroupStorage::kIdlePeriod);

  EXPECT_EQ(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_1));
  EXPECT_EQ(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_2));

  // Update the reported time for both keys, with k_anon_key_2
  // expiring after k_anon_key_1.
  storage->UpdateLastKAnonymityReported(k_anon_key_1);
  task_environment().FastForwardBy(base::Hours(2));
  storage->UpdateLastKAnonymityReported(k_anon_key_2);

  task_environment().FastForwardBy(
      InterestGroupStorage::kAdditionalKAnonStoragePeriod - base::Hours(2));

  EXPECT_NE(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_1));
  EXPECT_NE(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_2));

  task_environment().FastForwardBy(InterestGroupStorage::kIdlePeriod);

  EXPECT_EQ(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_1));
  EXPECT_NE(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_2));

  // UpdateLastKAnonymityReported should re-activate a k_anon key.
  storage->UpdateLastKAnonymityReported(k_anon_key_1);

  EXPECT_NE(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_1));
  EXPECT_NE(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_2));

  // After the interest group expires, we don't need to keep any k-anonymity
  // data unless it's been reported <1 day ago.
  storage->JoinInterestGroup(g, GURL("https://owner.example.com/join"));

  task_environment().FastForwardBy(InterestGroupStorage::kHistoryLength);
  storage->UpdateLastKAnonymityReported(k_anon_key_1);
  EXPECT_EQ(1u, storage->GetAllInterestGroupsUnfilteredForTesting().size());
  task_environment().FastForwardBy(InterestGroupStorage::kIdlePeriod);
  EXPECT_EQ(0u, storage->GetAllInterestGroupsUnfilteredForTesting().size());

  EXPECT_NE(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_1));
  EXPECT_EQ(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_2));

  task_environment().FastForwardBy(
      InterestGroupStorage::kAdditionalKAnonStoragePeriod);
  EXPECT_NE(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_1));
  task_environment().FastForwardBy(InterestGroupStorage::kIdlePeriod);

  EXPECT_EQ(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_1));
  EXPECT_EQ(base::Time::Min(),
            storage->GetLastKAnonymityReported(k_anon_key_2));
}

TEST_F(InterestGroupStorageTest, SetGetBiddingAndAuctionKeys) {
  const url::Origin origin_a =
      url::Origin::Create(GURL("https://a.example.com"));
  const url::Origin origin_b =
      url::Origin::Create(GURL("https://b.example.com"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  // No keys should be returned before any values are set.
  std::vector<BiddingAndAuctionServerKey> a_loaded_keys, b_loaded_keys;
  base::Time a_expiration, b_expiration;
  std::tie(a_expiration, a_loaded_keys) =
      storage->GetBiddingAndAuctionServerKeys(origin_a);
  std::tie(b_expiration, b_loaded_keys) =
      storage->GetBiddingAndAuctionServerKeys(origin_b);
  EXPECT_TRUE(a_loaded_keys.empty());
  EXPECT_TRUE(b_loaded_keys.empty());

  // The set values should be returned.
  std::vector<BiddingAndAuctionServerKey> a_keys{{"1", 1}, {"2", 2}};
  base::Time expiration = base::Time::Now() + base::Seconds(5);
  storage->SetBiddingAndAuctionServerKeys(origin_a, a_keys, expiration);
  std::tie(a_expiration, a_loaded_keys) =
      storage->GetBiddingAndAuctionServerKeys(origin_a);
  std::tie(b_expiration, b_loaded_keys) =
      storage->GetBiddingAndAuctionServerKeys(origin_b);
  EXPECT_EQ(a_loaded_keys.size(), 2u);
  EXPECT_NE(a_loaded_keys[0].id, a_loaded_keys[1].id);
  for (const BiddingAndAuctionServerKey& key : a_loaded_keys) {
    EXPECT_TRUE((key.id == 1 && key.key == "1") ||
                (key.id == 2 && key.key == "2"));
  }
  EXPECT_TRUE(b_loaded_keys.empty());
  EXPECT_EQ(expiration, a_expiration);

  // Setting values for a different origin shouldn't affect the previously
  // set values.
  std::vector<BiddingAndAuctionServerKey> b_keys{{"3", 3}};
  storage->SetBiddingAndAuctionServerKeys(origin_b, b_keys, expiration);
  std::tie(a_expiration, a_loaded_keys) =
      storage->GetBiddingAndAuctionServerKeys(origin_a);
  std::tie(b_expiration, b_loaded_keys) =
      storage->GetBiddingAndAuctionServerKeys(origin_b);
  EXPECT_EQ(a_loaded_keys.size(), 2u);
  EXPECT_NE(a_loaded_keys[0].id, a_loaded_keys[1].id);
  for (const BiddingAndAuctionServerKey& key : a_loaded_keys) {
    EXPECT_TRUE((key.id == 1 && key.key == "1") ||
                (key.id == 2 && key.key == "2"));
  }
  EXPECT_EQ(b_loaded_keys.size(), 1u);
  EXPECT_EQ("3", b_loaded_keys[0].key);
  EXPECT_EQ(3, b_loaded_keys[0].id);
  EXPECT_EQ(expiration, a_expiration);
  EXPECT_EQ(expiration, b_expiration);

  // Resetting the keys should overwrite the previous keys.
  a_keys = {{"1", 1}};
  task_environment().FastForwardBy(base::Seconds(2));
  expiration = base::Time::Now() + base::Days(7);
  storage->SetBiddingAndAuctionServerKeys(origin_a, a_keys, expiration);
  std::tie(a_expiration, a_loaded_keys) =
      storage->GetBiddingAndAuctionServerKeys(origin_a);
  std::tie(b_expiration, b_loaded_keys) =
      storage->GetBiddingAndAuctionServerKeys(origin_b);
  EXPECT_EQ(a_loaded_keys.size(), 1u);
  EXPECT_EQ("1", a_loaded_keys[0].key);
  EXPECT_EQ(1, a_loaded_keys[0].id);
  EXPECT_EQ(b_loaded_keys.size(), 1u);
  EXPECT_EQ("3", b_loaded_keys[0].key);
  EXPECT_EQ(3, b_loaded_keys[0].id);
  EXPECT_EQ(expiration, a_expiration);
  EXPECT_NE(expiration, b_expiration);

  // Only get unexpired values.
  task_environment().FastForwardBy(base::Seconds(3));
  std::tie(a_expiration, a_loaded_keys) =
      storage->GetBiddingAndAuctionServerKeys(origin_a);
  std::tie(b_expiration, b_loaded_keys) =
      storage->GetBiddingAndAuctionServerKeys(origin_b);
  EXPECT_EQ(a_loaded_keys.size(), 1u);
  EXPECT_EQ("1", a_loaded_keys[0].key);
  EXPECT_EQ(1, a_loaded_keys[0].id);
  EXPECT_TRUE(b_loaded_keys.empty());
  EXPECT_EQ(expiration, a_expiration);

  // DB maintenance should not delete unexpired values.
  EXPECT_EQ(base::Time::Min(), storage->GetLastMaintenanceTimeForTesting());
  task_environment().FastForwardBy(InterestGroupStorage::kIdlePeriod);
  EXPECT_NE(base::Time::Min(), storage->GetLastMaintenanceTimeForTesting());
  std::tie(a_expiration, a_loaded_keys) =
      storage->GetBiddingAndAuctionServerKeys(origin_a);
  std::tie(b_expiration, b_loaded_keys) =
      storage->GetBiddingAndAuctionServerKeys(origin_b);
  EXPECT_EQ(a_loaded_keys.size(), 1u);
  EXPECT_EQ("1", a_loaded_keys[0].key);
  EXPECT_EQ(1, a_loaded_keys[0].id);
  EXPECT_EQ(expiration, a_expiration);
  EXPECT_TRUE(b_loaded_keys.empty());
}

TEST_F(InterestGroupStorageTest, SetGetBiddingAndAuctionKeysNonUtf8) {
  const url::Origin origin = url::Origin::Create(GURL("https://b.example.com"));
  std::unique_ptr<InterestGroupStorage> storage = CreateStorage();
  std::string key(32, 0x00);
  std::vector<BiddingAndAuctionServerKey> keys{{key, 2}};
  storage->SetBiddingAndAuctionServerKeys(origin, keys,
                                          base::Time::Now() + base::Days(1));
  std::vector<BiddingAndAuctionServerKey> loaded_keys;
  base::Time expiration;
  std::tie(expiration, loaded_keys) =
      storage->GetBiddingAndAuctionServerKeys(origin);
  EXPECT_EQ(loaded_keys.size(), 1u);
  EXPECT_EQ(loaded_keys[0].key, key);
  EXPECT_EQ(loaded_keys[0].id, 2);
}

}  // namespace
}  // namespace content

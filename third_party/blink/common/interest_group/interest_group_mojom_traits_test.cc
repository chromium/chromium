// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/interest_group_mojom_traits.h"

#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/common/interest_group/test/interest_group_test_utils.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

namespace {

using ::blink::IgExpectEqualsForTesting;
using ::blink::IgExpectNotEqualsForTesting;

const char kOrigin1[] = "https://origin1.test/";
const char kOrigin2[] = "https://origin2.test/";

const char kName1[] = "Name 1";
const char kName2[] = "Name 2";

// Two URLs that share kOrigin1.
const char kUrl1[] = "https://origin1.test/url1";
const char kUrl2[] = "https://origin1.test/url2";

// Creates an InterestGroup with an owner and a name,which are mandatory fields.
InterestGroup CreateInterestGroup() {
  InterestGroup interest_group;
  interest_group.owner = url::Origin::Create(GURL(kOrigin1));
  interest_group.name = kName1;
  return interest_group;
}

// SerializesAndDeserializes the provided interest group, expecting
// deserialization to succeed. Expects the deserialization to succeed, and to be
// the same as the original group. Also makes sure the input InterestGroup is
// not equal to the output of CreateInterestGroup(), to verify that
// IgExpect[Not]EqualsForTesting() is checking whatever was modified in the
// input group.
//
// Arguments is not const because SerializeAndDeserialize() doesn't take a
// const input value, as serializing some object types is destructive.
void SerializeAndDeserializeAndCompare(InterestGroup& interest_group) {
  IgExpectNotEqualsForTesting(/*actual=*/interest_group,
                              /*not_expected=*/CreateInterestGroup());
  ASSERT_FALSE(testing::Test::HasFailure());

  InterestGroup interest_group_clone;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<blink::mojom::InterestGroup>(
      interest_group, interest_group_clone));
  IgExpectEqualsForTesting(/*actual=*/interest_group_clone,
                           /*expected=*/interest_group);
}

// A variant of SerializeAndDeserializeAndCompare() that expects serialization
// to fail.
//
// **NOTE**: Most validation of invalid fields should be checked in
// validate_blink_interest_group_test.cc, as it checks both against
// validate_blink_interest_group.cc (which runs in the renderer) and
// InterestGroup::IsValid() (which runs in the browser process). This method is
// useful for cases where validation is performed by WebIDL instead of custom
// renderer-side logic, but InterestGroup::IsValid() still needs to be checked.
void SerializeAndDeserializeExpectFailure(InterestGroup& interest_group,
                                          std::string_view tag = "") {
  IgExpectNotEqualsForTesting(/*actual=*/interest_group,
                              /*not_expected=*/CreateInterestGroup());
  ASSERT_FALSE(testing::Test::HasFailure());

  InterestGroup interest_group_clone;
  EXPECT_FALSE(mojo::test::SerializeAndDeserialize<blink::mojom::InterestGroup>(
      interest_group, interest_group_clone))
      << tag;
}

}  // namespace

// This file has tests for the deserialization success case. Failure cases are
// currently tested alongside ValidateBlinkInterestGroup(), since their failure
// cases should be the same.

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeExpiry) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.expiry = base::Time::Now();
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeOwner) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.owner = url::Origin::Create(GURL(kOrigin2));
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeName) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.name = kName2;
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializePriority) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.priority = 5.0;
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeEnableBiddingSignalsPrioritization) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.enable_bidding_signals_prioritization = true;
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializePriorityVector) {
  InterestGroup interest_group = CreateInterestGroup();

  interest_group.priority_vector = {{{"signals", 1.23}}};
  SerializeAndDeserializeAndCompare(interest_group);

  interest_group.priority_vector = {
      {{"signals1", 1}, {"signals2", 3}, {"signals3", -5}}};
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializePrioritySignalsOverride) {
  InterestGroup interest_group = CreateInterestGroup();
  // `priority_vector` is currently always set when `priority_signals_override`
  // is.
  interest_group.priority_vector.emplace();

  interest_group.priority_signals_overrides = {{{"signals", 0.51}}};
  SerializeAndDeserializeAndCompare(interest_group);

  interest_group.priority_signals_overrides = {
      {{"signals1", 1}, {"signals2", 3}, {"signals3", -5}}};
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeNonFinite) {
  double test_cases[] = {
      std::numeric_limits<double>::quiet_NaN(),
      std::numeric_limits<double>::signaling_NaN(),
      std::numeric_limits<double>::infinity(),
      -std::numeric_limits<double>::infinity(),
  };
  size_t i = 0u;
  for (double test_case : test_cases) {
    SCOPED_TRACE(i++);

    InterestGroup interest_group_bad_priority = CreateInterestGroup();
    interest_group_bad_priority.priority = test_case;
    SerializeAndDeserializeExpectFailure(interest_group_bad_priority,
                                         "priority");

    InterestGroup interest_group_bad_priority_vector = CreateInterestGroup();
    interest_group_bad_priority_vector.priority_vector = {{"foo", test_case}};
    SerializeAndDeserializeExpectFailure(interest_group_bad_priority_vector,
                                         "priority_vector");

    InterestGroup blink_interest_group_bad_priority_signals_overrides =
        CreateInterestGroup();
    blink_interest_group_bad_priority_signals_overrides
        .priority_signals_overrides = {{"foo", test_case}};
    SerializeAndDeserializeExpectFailure(
        blink_interest_group_bad_priority_signals_overrides,
        "priority_signals_overrides");
  }
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeSellerCapabilities) {
  InterestGroup interest_group = CreateInterestGroup();

  interest_group.seller_capabilities = {
      {{url::Origin::Create(GURL(kOrigin1)), {}}}};
  SerializeAndDeserializeAndCompare(interest_group);

  interest_group.seller_capabilities = {
      {{url::Origin::Create(GURL(kOrigin1)), {}},
       {url::Origin::Create(GURL(kOrigin2)), {}}}};
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeAllSellersCapabilities) {
  InterestGroup interest_group = CreateInterestGroup();

  interest_group.all_sellers_capabilities.Put(
      SellerCapabilities::kInterestGroupCounts);
  SerializeAndDeserializeAndCompare(interest_group);

  interest_group.all_sellers_capabilities.Put(
      SellerCapabilities::kLatencyStats);
  SerializeAndDeserializeAndCompare(interest_group);

  interest_group.all_sellers_capabilities.Put(
      SellerCapabilities::kInterestGroupCounts);
  interest_group.all_sellers_capabilities.Put(
      SellerCapabilities::kLatencyStats);
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeBiddingUrl) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_url = GURL(kUrl1);
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeWasmHelperUrl) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.bidding_wasm_helper_url = GURL(kUrl1);
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeUpdateUrl) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.update_url = GURL(kUrl1);
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeTrustedBiddingSignalsUrl) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.trusted_bidding_signals_url = GURL(kUrl1);
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeCrossOriginTrustedBiddingSignalsUrl) {
  // Like with everything here, the negative test is in
  // validate_blink_interest_group_test.cc
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      blink::features::kFledgePermitCrossOriginTrustedSignals);

  InterestGroup interest_group = CreateInterestGroup();
  interest_group.trusted_bidding_signals_url =
      GURL("https://cross-origin.test/");
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeTrustedBiddingSignalsKeys) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->emplace_back("foo");
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeTrustedBiddingSignalsSlotSizeMode) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.trusted_bidding_signals_slot_size_mode =
      InterestGroup::TrustedBiddingSignalsSlotSizeMode::kSlotSize;
  SerializeAndDeserializeAndCompare(interest_group);
  interest_group.trusted_bidding_signals_slot_size_mode =
      InterestGroup::TrustedBiddingSignalsSlotSizeMode::kAllSlotsRequestedSizes;
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeMaxTrustedBiddingSignalsURLLength) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.max_trusted_bidding_signals_url_length = 8000;
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeTrustedBiddingSignalsCoordinator) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.trusted_bidding_signals_coordinator =
      url::Origin::Create(GURL("https://example.test"));
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeInvalidTrustedBiddingSignalsCoordinator) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.trusted_bidding_signals_coordinator =
      url::Origin::Create(GURL("http://example.test"));
  SerializeAndDeserializeExpectFailure(interest_group,
                                       "trustedBiddingSignalsCoordinator");
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeUserBiddingSignals) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.user_bidding_signals = "[]";
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeAds) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.ads.emplace();
  interest_group.ads->emplace_back(GURL(kUrl1),
                                   /*metadata=*/std::nullopt);
  interest_group.ads->emplace_back(GURL(kUrl2),
                                   /*metadata=*/"[]");
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeAdsWithReportingIds) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.ads.emplace();
  interest_group.ads->emplace_back(GURL(kUrl1),
                                   /*metadata=*/std::nullopt,
                                   /*size_group=*/std::nullopt);
  (*interest_group.ads)[0].buyer_reporting_id = "buyer_id_1";
  (*interest_group.ads)[0].buyer_and_seller_reporting_id = "both_id_1";
  (*interest_group.ads)[0].selectable_buyer_and_seller_reporting_ids = {
      "selectable_id1", "selectable_id2"};
  interest_group.ads->emplace_back(GURL(kUrl2),
                                   /*metadata=*/"[]",
                                   /*size_group=*/std::nullopt);
  (*interest_group.ads)[1].buyer_reporting_id = "buyer_id_2";
  (*interest_group.ads)[1].buyer_and_seller_reporting_id = "both_id_2";
  (*interest_group.ads)[1].selectable_buyer_and_seller_reporting_ids = {
      "selectable_id3", "selectable_id4"};

  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, AdComponentsWithBuyerReportingIdInvalid) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.ad_components.emplace();
  interest_group.ad_components->emplace_back(GURL(kUrl1),
                                             /*metadata=*/std::nullopt,
                                             /*size_group=*/std::nullopt);
  (*interest_group.ad_components)[0].buyer_reporting_id = "buyer_id_1";
  EXPECT_FALSE(interest_group.IsValid());
}

TEST(InterestGroupMojomTraitsTest,
     AdComponentsWithBuyerAndSellerReportingIdInvalid) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.ad_components.emplace();
  interest_group.ad_components->emplace_back(GURL(kUrl1),
                                             /*metadata=*/std::nullopt,
                                             /*size_group=*/std::nullopt);
  (*interest_group.ad_components)[0].buyer_and_seller_reporting_id =
      "both_id_1";
  EXPECT_FALSE(interest_group.IsValid());
}

TEST(InterestGroupMojomTraitsTest,
     AdComponentsWithSelectableReportingIdInvalid) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.ad_components.emplace();
  interest_group.ad_components->emplace_back(GURL(kUrl1),
                                             /*metadata=*/std::nullopt,
                                             /*size_group=*/std::nullopt);
  (*interest_group.ad_components)[0].selectable_buyer_and_seller_reporting_ids =
      {"selectable_id1", "selectable_id2"};
  EXPECT_FALSE(interest_group.IsValid());
}

TEST(InterestGroupMojomTraitsTest, AdComponentsWithNoReportingIdsIsValid) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.ad_components.emplace();
  interest_group.ad_components->emplace_back(GURL(kUrl1),
                                             /*metadata=*/std::nullopt,
                                             /*size_group=*/std::nullopt);
  EXPECT_TRUE(interest_group.IsValid());
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeAdsWithSizeGroups) {
  InterestGroup interest_group = CreateInterestGroup();
  // All three of the following mappings must be valid in order for the
  // serialization and deserialization to succeed, when there is an ad with a
  // size group assigned.
  // 1. Ad --> size group
  // 2. Size groups --> sizes
  // 3. Size --> blink::AdSize
  interest_group.ads.emplace();
  interest_group.ads->emplace_back(GURL(kUrl1),
                                   /*metadata=*/std::nullopt,
                                   /*size_group=*/"group_1");
  interest_group.ads->emplace_back(GURL(kUrl2),
                                   /*metadata=*/"[]", /*size_group=*/"group_2");
  interest_group.ad_sizes.emplace();
  interest_group.ad_sizes->emplace(
      "size_1", blink::AdSize(300, blink::AdSize::LengthUnit::kPixels, 150,
                              blink::AdSize::LengthUnit::kPixels));
  interest_group.ad_sizes->emplace(
      "size_2", blink::AdSize(640, blink::AdSize::LengthUnit::kPixels, 480,
                              blink::AdSize::LengthUnit::kPixels));
  std::vector<std::string> size_list = {"size_1", "size_2"};
  interest_group.size_groups.emplace();
  interest_group.size_groups->emplace("group_1", size_list);
  interest_group.size_groups->emplace("group_2", size_list);
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeAdsWithAdRenderId) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.ads.emplace();
  interest_group.ads->emplace_back(
      GURL(kUrl1),
      /*metadata=*/std::nullopt,
      /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/std::nullopt,
      /*buyer_and_seller_reporting_id=*/std::nullopt,
      /*selectable_buyer_and_seller_reporting_ids=*/std::nullopt,
      /*ad_render_id=*/"foo");
  interest_group.ads->emplace_back(
      GURL(kUrl2),
      /*metadata=*/"[]",
      /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/std::nullopt,
      /*buyer_and_seller_reporting_id=*/std::nullopt,
      /*selectable_buyer_and_seller_reporting_ids=*/std::nullopt,
      /*ad_render_id=*/"bar");
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeAdsWithAllowedReportingOrigins) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.ads.emplace();
  std::vector<url::Origin> allowed_reporting_origins_1 = {
      url::Origin::Create(GURL(kOrigin1))};
  std::vector<url::Origin> allowed_reporting_origins_2 = {
      url::Origin::Create(GURL(kOrigin2))};
  interest_group.ads->emplace_back(
      GURL(kUrl1),
      /*metadata=*/std::nullopt,
      /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/std::nullopt,
      /*buyer_and_seller_reporting_id=*/std::nullopt,
      /*selectable_buyer_and_seller_reporting_ids=*/std::nullopt,
      /*ad_render_id=*/std::nullopt, allowed_reporting_origins_1);
  interest_group.ads->emplace_back(
      GURL(kUrl2),
      /*metadata=*/"[]",
      /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/std::nullopt,
      /*buyer_and_seller_reporting_id=*/std::nullopt,
      /*selectable_buyer_and_seller_reporting_ids=*/std::nullopt,
      /*ad_render_id=*/std::nullopt, allowed_reporting_origins_2);
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeAdComponents) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.ad_components.emplace();
  interest_group.ad_components->emplace_back(GURL(kUrl1),
                                             /*metadata=*/std::nullopt);
  interest_group.ad_components->emplace_back(GURL(kUrl2), /*metadata=*/"[]");
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeAdComponentsWithSize) {
  InterestGroup interest_group = CreateInterestGroup();
  // All three of the following mappings must be valid in order for the
  // serialization and deserialization to succeed, when there is an ad component
  // with a size group assigned.
  // 1. Ad component --> size group
  // 2. Size groups --> sizes
  // 3. Size --> blink::AdSize
  interest_group.ad_components.emplace();
  interest_group.ad_components->emplace_back(GURL(kUrl1),
                                             /*metadata=*/std::nullopt,
                                             /*size_group=*/"group_1");
  interest_group.ad_components->emplace_back(GURL(kUrl2),
                                             /*metadata=*/"[]",
                                             /*size_group=*/"group_2");
  interest_group.ad_sizes.emplace();
  interest_group.ad_sizes->emplace(
      "size_1", blink::AdSize(300, blink::AdSize::LengthUnit::kPixels, 150,
                              blink::AdSize::LengthUnit::kPixels));
  interest_group.ad_sizes->emplace(
      "size_2", blink::AdSize(640, blink::AdSize::LengthUnit::kPixels, 480,
                              blink::AdSize::LengthUnit::kPixels));
  std::vector<std::string> size_list = {"size_1", "size_2"};
  interest_group.size_groups.emplace();
  interest_group.size_groups->emplace("group_1", size_list);
  interest_group.size_groups->emplace("group_2", size_list);
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeAdComponentsWithAdRenderId) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.ad_components.emplace();
  interest_group.ad_components->emplace_back(
      GURL(kUrl1),
      /*metadata=*/std::nullopt,
      /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/std::nullopt,
      /*buyer_and_seller_reporting_id=*/std::nullopt,
      /*selectable_buyer_and_seller_reporting_ids=*/std::nullopt,
      /*ad_render_id=*/"foo");
  interest_group.ad_components->emplace_back(
      GURL(kUrl2), /*metadata=*/"[]",
      /*size_group=*/std::nullopt,
      /*buyer_reporting_id=*/std::nullopt,
      /*buyer_and_seller_reporting_id=*/std::nullopt,
      /*selectable_buyer_and_seller_reporting_ids=*/std::nullopt,
      /*ad_render_id=*/"bar");
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeAdSizes) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.ad_sizes.emplace();
  interest_group.ad_sizes->emplace(
      "size_1", blink::AdSize(300, blink::AdSize::LengthUnit::kPixels, 150,
                              blink::AdSize::LengthUnit::kPixels));
  interest_group.ad_sizes->emplace(
      "size_2", blink::AdSize(640, blink::AdSize::LengthUnit::kPixels, 480,
                              blink::AdSize::LengthUnit::kPixels));
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeSizeGroups) {
  InterestGroup interest_group = CreateInterestGroup();
  // The size names must be in adSizes. Otherwise, the sizeGroups will fail
  // validation.
  interest_group.ad_sizes.emplace();
  interest_group.ad_sizes->emplace(
      "size_1", blink::AdSize(300, blink::AdSize::LengthUnit::kPixels, 150,
                              blink::AdSize::LengthUnit::kPixels));
  interest_group.ad_sizes->emplace(
      "size_2", blink::AdSize(640, blink::AdSize::LengthUnit::kPixels, 480,
                              blink::AdSize::LengthUnit::kPixels));
  std::vector<std::string> size_list = {"size_1", "size_2"};
  interest_group.size_groups.emplace();
  interest_group.size_groups->emplace("group_1", size_list);
  interest_group.size_groups->emplace("group_2", size_list);
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeAuctionServerRequestFlags) {
  InterestGroup interest_group = CreateInterestGroup();

  interest_group.auction_server_request_flags = {
      blink::AuctionServerRequestFlagsEnum::kIncludeFullAds};
  SerializeAndDeserializeAndCompare(interest_group);

  interest_group.auction_server_request_flags = {
      blink::AuctionServerRequestFlagsEnum::kOmitAds};
  SerializeAndDeserializeAndCompare(interest_group);

  interest_group.auction_server_request_flags = {
      blink::AuctionServerRequestFlagsEnum::kOmitUserBiddingSignals};
  SerializeAndDeserializeAndCompare(interest_group);

  interest_group.auction_server_request_flags = {
      blink::AuctionServerRequestFlagsEnum::kOmitAds,
      blink::AuctionServerRequestFlagsEnum::kIncludeFullAds,
      blink::AuctionServerRequestFlagsEnum::kOmitUserBiddingSignals};
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeAdditionalBidKey) {
  constexpr blink::InterestGroup::AdditionalBidKey kAdditionalBidKey = {
      0x7d, 0x4d, 0x0e, 0x7f, 0x61, 0x53, 0xa6, 0x9b, 0x62, 0x42, 0xb5,
      0x22, 0xab, 0xbe, 0xe6, 0x85, 0xfd, 0xa4, 0x42, 0x0f, 0x88, 0x34,
      0xb1, 0x08, 0xc3, 0xbd, 0xae, 0x36, 0x9e, 0xf5, 0x49, 0xfa};
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.additional_bid_key = kAdditionalBidKey;
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest,
     SerializeAndDeserializeAggregationCoordinatorOrigin) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.aggregation_coordinator_origin =
      url::Origin::Create(GURL("https://example.com"));
  SerializeAndDeserializeAndCompare(interest_group);
}

}  // namespace blink

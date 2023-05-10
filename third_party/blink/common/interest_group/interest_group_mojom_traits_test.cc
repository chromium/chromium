// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/interest_group_mojom_traits.h"

#include "base/time/time.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

namespace {

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
// IsEqualForTesting() is checking whatever was modified in the input group.
//
// Arguments is not const because SerializeAndDeserialize() doesn't take a
// const input value, as serializing some object types is destructive.
void SerializeAndDeserializeAndCompare(InterestGroup& interest_group) {
  ASSERT_FALSE(interest_group.IsEqualForTesting(CreateInterestGroup()));

  InterestGroup interest_group_clone;
  ASSERT_TRUE(mojo::test::SerializeAndDeserialize<blink::mojom::InterestGroup>(
      interest_group, interest_group_clone));
  EXPECT_TRUE(interest_group.IsEqualForTesting(interest_group_clone));
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
     SerializeAndDeserializeAllSellerCapabilities) {
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
     SerializeAndDeserializeTrustedBiddingSignalsKeys) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.trusted_bidding_signals_keys.emplace();
  interest_group.trusted_bidding_signals_keys->emplace_back("foo");
  SerializeAndDeserializeAndCompare(interest_group);
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
                                   /*metadata=*/absl::nullopt,
                                   /*size_group=*/absl::nullopt,
                                   /*ad_render_id=*/absl::nullopt);
  interest_group.ads->emplace_back(GURL(kUrl2),
                                   /*metadata=*/"[]",
                                   /*size_group=*/absl::nullopt,
                                   /*ad_render_id=*/absl::nullopt);
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeAdsWithReportingIds) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.ads.emplace();
  interest_group.ads->emplace_back(GURL(kUrl1),
                                   /*metadata=*/absl::nullopt,
                                   /*size_group=*/absl::nullopt);
  (*interest_group.ads)[0].buyer_reporting_id = "buyer_id_1";
  (*interest_group.ads)[0].buyer_and_seller_reporting_id = "both_id_1";
  interest_group.ads->emplace_back(GURL(kUrl2),
                                   /*metadata=*/"[]",
                                   /*size_group=*/absl::nullopt);
  (*interest_group.ads)[1].buyer_reporting_id = "buyer_id_2";
  (*interest_group.ads)[1].buyer_and_seller_reporting_id = "both_id_2";

  SerializeAndDeserializeAndCompare(interest_group);
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
                                   /*metadata=*/absl::nullopt,
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
  interest_group.ads->emplace_back(GURL(kUrl1),
                                   /*metadata=*/absl::nullopt,
                                   /*size_group=*/absl::nullopt,
                                   /*ad_render_id=*/"foo");
  interest_group.ads->emplace_back(GURL(kUrl2),
                                   /*metadata=*/"[]",
                                   /*size_group=*/absl::nullopt,
                                   /*ad_render_id=*/"bar");
  SerializeAndDeserializeAndCompare(interest_group);
}

TEST(InterestGroupMojomTraitsTest, SerializeAndDeserializeAdComponents) {
  InterestGroup interest_group = CreateInterestGroup();
  interest_group.ad_components.emplace();
  interest_group.ad_components->emplace_back(GURL(kUrl1),
                                             /*metadata=*/absl::nullopt,
                                             /*size_group=*/absl::nullopt,
                                             /*ad_render_id=*/absl::nullopt);
  interest_group.ad_components->emplace_back(GURL(kUrl2), /*metadata=*/"[]",
                                             /*size_group=*/absl::nullopt,
                                             /*ad_render_id=*/absl::nullopt);
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
                                             /*metadata=*/absl::nullopt,
                                             /*size_group=*/"group_1",
                                             /*ad_render_id=*/absl::nullopt);
  interest_group.ad_components->emplace_back(GURL(kUrl2),
                                             /*metadata=*/"[]",
                                             /*size_group=*/"group_2",
                                             /*ad_render_id=*/absl::nullopt);
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
  interest_group.ad_components->emplace_back(GURL(kUrl1),
                                             /*metadata=*/absl::nullopt,
                                             /*size_group=*/absl::nullopt,
                                             /*ad_render_id=*/"foo");
  interest_group.ad_components->emplace_back(GURL(kUrl2), /*metadata=*/"[]",
                                             /*size_group=*/absl::nullopt,
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

}  // namespace blink

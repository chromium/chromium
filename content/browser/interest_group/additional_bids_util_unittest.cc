// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/additional_bids_util.h"

#include <limits>
#include <string>

#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
namespace {

class AdditionalBidsUtilTest : public testing::Test {
 protected:
  base::Value::Dict MakeMinimalValid() {
    base::Value::Dict ig_dict;
    ig_dict.Set("name", "trainfans");
    ig_dict.Set("biddingLogicURL", "https://rollingstock.test/logic.js");
    ig_dict.Set("owner", "https://rollingstock.test/");

    base::Value::Dict bid_dict;
    bid_dict.Set("bid", 10.0);
    bid_dict.Set("render", "https://en.wikipedia.test/wiki/Train");

    base::Value::Dict additional_bid_dict;
    additional_bid_dict.Set("auctionNonce", kAuctionNonce.AsLowercaseString());
    additional_bid_dict.Set("seller", "https://seller.test");
    additional_bid_dict.Set("topLevelSeller", "https://top-organizer.test");
    additional_bid_dict.Set("interestGroup", std::move(ig_dict));
    additional_bid_dict.Set("bid", std::move(bid_dict));
    return additional_bid_dict;
  }

  const base::Uuid kAuctionNonce{base::Uuid::GenerateRandomV4()};
  const url::Origin kSeller = url::Origin::Create(GURL("https://seller.test"));
  const url::Origin kTopSeller =
      url::Origin::Create(GURL("https://top-organizer.test"));
};

TEST_F(AdditionalBidsUtilTest, FailNotDict) {
  base::Value input(5);

  auto result =
      DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce, kSeller,
                          /*top_level_seller=*/absl::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' is not a "
      "dictionary.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailNoNonce) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Remove("auctionNonce");
  base::Value input(std::move(additional_bid_dict));

  auto result =
      DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce, kSeller,
                          /*top_level_seller=*/absl::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or incorrect nonce.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailInvalidNonce) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Set("auctionNonce", "not-a-nonce");
  base::Value input(std::move(additional_bid_dict));

  auto result =
      DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce, kSeller,
                          /*top_level_seller=*/absl::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or incorrect nonce.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailMissingSeller) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Remove("seller");
  base::Value input(std::move(additional_bid_dict));

  auto result =
      DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce, kSeller,
                          /*top_level_seller=*/absl::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or incorrect seller.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailInvalidSeller) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Set("seller", "http://notseller.test");
  base::Value input(std::move(additional_bid_dict));

  auto result =
      DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce, kSeller,
                          /*top_level_seller=*/absl::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or incorrect seller.",
      result.error());
}

// Specifying topLevelSeller in a bid in a non-component auction is a problem.
TEST_F(AdditionalBidsUtilTest, FailInvalidTopLevelSeller) {
  base::Value input(MakeMinimalValid());
  auto result =
      DecodeAdditionalBid(/*auction=*/nullptr, input, kAuctionNonce, kSeller,
                          /*top_level_seller=*/absl::nullopt);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to specifying topLevelSeller in a non-component auction.",
      result.error());
}

// Not specifying topLevelSeller in component auction bid is also a problem.
TEST_F(AdditionalBidsUtilTest, FailInvalidTopLevelSeller2) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Remove("topLevelSeller");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or incorrect topLevelSeller.",
      result.error());
}

// An incorrect topLevelSeller in a bid in a component auction is also a
// problem.
TEST_F(AdditionalBidsUtilTest, FailInvalidTopLevelSeller3) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Set("topLevelSeller", "https://wrong-organizer.test");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or incorrect topLevelSeller.",
      result.error());
}

// Missing IG dictionary.
TEST_F(AdditionalBidsUtilTest, FailNoIGDictionary) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Remove("interestGroup");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or invalid interest group info.",
      result.error());
}

// Missing IG name.
TEST_F(AdditionalBidsUtilTest, FailInvalidIG) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.RemoveByDottedPath("interestGroup.name");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or invalid interest group info.",
      result.error());
}

// Missing IG bidding script.
TEST_F(AdditionalBidsUtilTest, FailInvalidIG2) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.RemoveByDottedPath("interestGroup.biddingLogicURL");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or invalid interest group info.",
      result.error());
}

// Missing IG owner.
TEST_F(AdditionalBidsUtilTest, FailInvalidIG3) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.RemoveByDottedPath("interestGroup.owner");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or invalid interest group info.",
      result.error());
}

// Non-https IG owner.
TEST_F(AdditionalBidsUtilTest, FailInvalidIG4) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("interestGroup.owner",
                                      "http://rollingstock.test/");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or invalid interest group info.",
      result.error());
}

// Domain mismatch between owner and bidding script.
TEST_F(AdditionalBidsUtilTest, FailInvalidIG5) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("interestGroup.owner",
                                      "https://trainstuff.test/");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to invalid origin of biddingLogicURL.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailMissingBid) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.Remove("bid");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing bid info.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailMissingBidCreative) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.RemoveByDottedPath("bid.render");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or invalid creative URL.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailMissingBidValue) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.RemoveByDottedPath("bid.bid");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or invalid bid value.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, FailInvalidBidValue) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.bid", 0.0);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to missing or invalid bid value.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, MinimalValid) {
  base::Value input(MakeMinimalValid());

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->bid_state);
  ASSERT_TRUE(result->bid);
  const InterestGroupAuction::BidState* bid_state = result->bid_state.get();
  const InterestGroupAuction::Bid* bid = result->bid.get();

  EXPECT_TRUE(bid_state->made_bid);
  ASSERT_TRUE(bid_state->bidder);
  EXPECT_EQ("trainfans", bid_state->bidder->interest_group.name);
  EXPECT_EQ("https://rollingstock.test",
            bid_state->bidder->interest_group.owner.Serialize());
  ASSERT_TRUE(bid_state->bidder->interest_group.bidding_url.has_value());
  EXPECT_EQ("https://rollingstock.test/logic.js",
            bid_state->bidder->interest_group.bidding_url->spec());

  ASSERT_TRUE(bid_state->bidder->interest_group.ads.has_value());
  ASSERT_EQ(1u, bid_state->bidder->interest_group.ads->size());
  EXPECT_EQ("https://en.wikipedia.test/wiki/Train",
            bid_state->bidder->interest_group.ads.value()[0].render_url.spec());

  EXPECT_EQ(InterestGroupAuction::Bid::BidRole::kBothKAnonModes, bid->bid_role);
  EXPECT_EQ("null", bid->ad_metadata);
  EXPECT_EQ(10.0, bid->bid);
  EXPECT_EQ(absl::nullopt, bid->bid_currency);
  EXPECT_EQ(absl::nullopt, bid->ad_cost);
  EXPECT_EQ(blink::AdDescriptor(GURL("https://en.wikipedia.test/wiki/Train")),
            bid->ad_descriptor);
  EXPECT_EQ(0u, bid->ad_component_descriptors.size());
  EXPECT_EQ(absl::nullopt, bid->modeling_signals);
  EXPECT_EQ(&bid_state->bidder->interest_group, bid->interest_group);
  EXPECT_EQ(&bid_state->bidder->interest_group.ads.value()[0], bid->bid_ad);
  EXPECT_EQ(bid_state, bid->bid_state);
}

TEST_F(AdditionalBidsUtilTest, InvalidBidCurrencyType) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.bidCurrency", 5);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to invalid bidCurrency.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, InvalidBidCurrencySyntax) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.bidCurrency", "Dollars");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to invalid bidCurrency.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, ValidBidCurrency) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.bidCurrency", "USD");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  EXPECT_TRUE(result.has_value());
  ASSERT_TRUE(result->bid);
  ASSERT_TRUE(result->bid->bid_currency);
  EXPECT_EQ("USD", result->bid->bid_currency->currency_code());
}

TEST_F(AdditionalBidsUtilTest, InvalidAdCost) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.adCost", "big");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to invalid adCost.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, ValidAdCost) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.adCost", 15.5);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->bid);
  ASSERT_TRUE(result->bid->ad_cost);
  EXPECT_EQ(15.5, *result->bid->ad_cost);
}

// We have a tradition of ignoring modeling signals if they're out of range,
// so this follows.
TEST_F(AdditionalBidsUtilTest, InvalidModelingSignals) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.modelingSignals", 4096);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->bid);
  EXPECT_FALSE(result->bid->modeling_signals);
}

TEST_F(AdditionalBidsUtilTest, InvalidModelingSignals2) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.modelingSignals", -0.001);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->bid);
  EXPECT_FALSE(result->bid->modeling_signals);
}

// Bad-type modeling signals still an error, however.
TEST_F(AdditionalBidsUtilTest, BadTypeModelingSignals) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.modelingSignals", "string");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to non-numeric modelingSignals.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, ValidModelingSignals) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.modelingSignals", 0);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->bid);
  ASSERT_TRUE(result->bid->modeling_signals);
  EXPECT_EQ(*result->bid->modeling_signals, 0);
}

TEST_F(AdditionalBidsUtilTest, ValidModelingSignals2) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.modelingSignals", 2.5);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->bid);
  ASSERT_TRUE(result->bid->modeling_signals);
  EXPECT_EQ(*result->bid->modeling_signals, 2);
}

TEST_F(AdditionalBidsUtilTest, ValidModelingSignals3) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.modelingSignals", 4095.5);
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->bid);
  ASSERT_TRUE(result->bid->modeling_signals);
  EXPECT_EQ(*result->bid->modeling_signals, 4095);
}

TEST_F(AdditionalBidsUtilTest, InvalidAdComponents) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  additional_bid_dict.SetByDottedPath("bid.adComponents", "oops");
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to invalid adComponents.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, InvalidAdComponentsEntry) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  base::Value::List ad_components_list;
  ad_components_list.Append(10);
  additional_bid_dict.SetByDottedPath("bid.adComponents",
                                      std::move(ad_components_list));
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(
      "Additional bid on auction with seller 'https://seller.test' rejected "
      "due to invalid entry in adComponents.",
      result.error());
}

TEST_F(AdditionalBidsUtilTest, ValidAdComponents) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  base::Value::List ad_components_list;
  ad_components_list.Append("https://en.wikipedia.test/wiki/Locomotive");
  ad_components_list.Append("https://en.wikipedia.test/wiki/High-speed_rail");
  additional_bid_dict.SetByDottedPath("bid.adComponents",
                                      std::move(ad_components_list));
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->bid);
  ASSERT_TRUE(result->bid_state);

  // Components should be both in the ad and the synthesized IG.
  ASSERT_EQ(2u, result->bid->ad_component_descriptors.size());
  EXPECT_EQ(
      blink::AdDescriptor(GURL("https://en.wikipedia.test/wiki/Locomotive")),
      result->bid->ad_component_descriptors[0]);
  EXPECT_EQ(blink::AdDescriptor(
                GURL("https://en.wikipedia.test/wiki/High-speed_rail")),
            result->bid->ad_component_descriptors[1]);

  ASSERT_TRUE(
      result->bid_state->bidder->interest_group.ad_components.has_value());
  ASSERT_EQ(2u,
            result->bid_state->bidder->interest_group.ad_components->size());
  EXPECT_EQ("https://en.wikipedia.test/wiki/Locomotive",
            result->bid_state->bidder->interest_group.ad_components.value()[0]
                .render_url.spec());
  EXPECT_EQ("https://en.wikipedia.test/wiki/High-speed_rail",
            result->bid_state->bidder->interest_group.ad_components.value()[1]
                .render_url.spec());
}

TEST_F(AdditionalBidsUtilTest, ValidAdComponentsEmpty) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  base::Value::List ad_components_list;
  additional_bid_dict.SetByDottedPath("bid.adComponents",
                                      std::move(ad_components_list));
  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->bid);
  ASSERT_TRUE(result->bid_state);

  EXPECT_EQ(0u, result->bid->ad_component_descriptors.size());
  ASSERT_TRUE(
      result->bid_state->bidder->interest_group.ad_components.has_value());
  EXPECT_EQ(0u,
            result->bid_state->bidder->interest_group.ad_components->size());
}

TEST_F(AdditionalBidsUtilTest, ValidAdMetadata) {
  base::Value::Dict additional_bid_dict = MakeMinimalValid();
  base::Value::Dict metadata_dict;
  metadata_dict.Set("a", "hello");
  metadata_dict.Set("b", 1.0);
  additional_bid_dict.SetByDottedPath("bid.ad", std::move(metadata_dict));

  base::Value input(std::move(additional_bid_dict));

  auto result = DecodeAdditionalBid(
      /*auction=*/nullptr, input, kAuctionNonce, kSeller,
      base::optional_ref<const url::Origin>(kTopSeller));
  EXPECT_TRUE(result.has_value());
  ASSERT_TRUE(result->bid);
  EXPECT_EQ(R"({"a":"hello","b":1.0})", result->bid->ad_metadata);
}

}  // namespace
}  // namespace content

// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/auction_config_mojom_traits.h"

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

bool operator==(const AuctionConfig& a, const AuctionConfig& b);

bool operator==(const AuctionConfig::NonSharedParams& a,
                const AuctionConfig::NonSharedParams& b) {
  return std::tie(a.interest_group_buyers, a.auction_signals, a.seller_signals,
                  a.seller_timeout, a.per_buyer_signals, a.per_buyer_timeouts,
                  a.all_buyers_timeout, a.per_buyer_group_limits,
                  a.all_buyers_group_limit, a.per_buyer_priority_signals,
                  a.all_buyers_priority_signals, a.component_auctions) ==
         std::tie(b.interest_group_buyers, b.auction_signals, b.seller_signals,
                  b.seller_timeout, b.per_buyer_signals, b.per_buyer_timeouts,
                  b.all_buyers_timeout, b.per_buyer_group_limits,
                  b.all_buyers_group_limit, b.per_buyer_priority_signals,
                  b.all_buyers_priority_signals, b.component_auctions);
}

bool operator==(const AuctionConfig& a, const AuctionConfig& b) {
  return std::tie(a.seller, a.decision_logic_url, a.trusted_scoring_signals_url,
                  a.non_shared_params, a.seller_experiment_group_id,
                  a.all_buyer_experiment_group_id,
                  a.per_buyer_experiment_group_ids) ==
         std::tie(b.seller, b.decision_logic_url, b.trusted_scoring_signals_url,
                  b.non_shared_params, b.seller_experiment_group_id,
                  b.all_buyer_experiment_group_id,
                  b.per_buyer_experiment_group_ids);
}

namespace {

// Creates a minimal valid AuctionConfig, with a seller and the passed in
// decision logic URL. Seller is derived from `decision_logic_url`.
AuctionConfig CreateBasicConfig(
    const GURL& decision_logic_url = GURL("https://seller.test/foo")) {
  AuctionConfig auction_config;
  auction_config.seller = url::Origin::Create(decision_logic_url);
  auction_config.decision_logic_url = decision_logic_url;
  return auction_config;
}

// Creates an AuctionConfig with all fields except `component_auctions`
// populated.
AuctionConfig CreateFullConfig() {
  AuctionConfig auction_config = CreateBasicConfig();

  auction_config.trusted_scoring_signals_url = GURL("https://seller.test/bar");
  auction_config.seller_experiment_group_id = 1;
  auction_config.all_buyer_experiment_group_id = 2;

  url::Origin buyer = url::Origin::Create(GURL("https://buyer.test"));
  auction_config.per_buyer_experiment_group_ids[buyer] = 3;

  AuctionConfig::NonSharedParams& non_shared_params =
      auction_config.non_shared_params;
  non_shared_params.interest_group_buyers.emplace();
  non_shared_params.interest_group_buyers->push_back(buyer);
  non_shared_params.auction_signals = "[4]";
  non_shared_params.seller_signals = "[5]";
  non_shared_params.seller_timeout = base::Seconds(6);
  non_shared_params.per_buyer_signals.emplace();
  (*non_shared_params.per_buyer_signals)[buyer] = "[7]";
  non_shared_params.per_buyer_timeouts.emplace();
  (*non_shared_params.per_buyer_timeouts)[buyer] = base::Seconds(8);
  non_shared_params.all_buyers_timeout = base::Seconds(9);
  non_shared_params.per_buyer_group_limits[buyer] = 10;
  non_shared_params.all_buyers_group_limit = 11;
  non_shared_params.per_buyer_priority_signals.emplace();
  (*non_shared_params.per_buyer_priority_signals)[buyer] = {
      {"hats", 1.5}, {"for", 0}, {"sale", -2}};
  non_shared_params.all_buyers_priority_signals = {
      {"goats", -1.5}, {"for", 5}, {"sale", 0}};

  return auction_config;
}

// Attempts to serialize and then deserialize `auction_config`, returning true
// if deserialization succeeded. On success, also checks that the resulting
// config matches the original config.
bool SerializeAndDeserialize(const AuctionConfig& auction_config) {
  AuctionConfig auction_config_clone;
  bool success =
      mojo::test::SerializeAndDeserialize<blink::mojom::AuctionAdConfig>(
          auction_config, auction_config_clone);

  if (success) {
    EXPECT_EQ(auction_config, auction_config_clone);
    // This *should* be implied by the above, but let's check...
    EXPECT_EQ(auction_config.non_shared_params,
              auction_config_clone.non_shared_params);
  }
  return success;
}

TEST(AuctionConfigMojomTraitsTest, Empty) {
  AuctionConfig auction_config;
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, Basic) {
  AuctionConfig auction_config = CreateBasicConfig();
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, SellerNotHttps) {
  AuctionConfig auction_config = CreateBasicConfig(GURL("http://seller.test"));
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, SellerDecisionUrlMismatch) {
  AuctionConfig auction_config = CreateBasicConfig(GURL("http://seller.test"));
  // Different origin than seller, but same scheme.
  auction_config.decision_logic_url = GURL("https://not.seller.test/foo");
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));

  auction_config = CreateBasicConfig(GURL("https://seller.test"));
  // This blob URL should be considered same-origin to the seller, but the
  // scheme is wrong.
  auction_config.decision_logic_url = GURL("blob:https://seller.test/foo");
  ASSERT_EQ(auction_config.seller,
            url::Origin::Create(auction_config.decision_logic_url));
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, SellerScoringSignalsUrlMismatch) {
  AuctionConfig auction_config = CreateBasicConfig(GURL("http://seller.test"));
  // Different origin than seller, but same scheme.
  auction_config.trusted_scoring_signals_url =
      GURL("https://not.seller.test/foo");
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));

  auction_config = CreateBasicConfig(GURL("https://seller.test"));
  // This blob URL should be considered same-origin to the seller, but the
  // scheme is wrong.
  auction_config.trusted_scoring_signals_url =
      GURL("blob:https://seller.test/foo");
  ASSERT_EQ(auction_config.seller,
            url::Origin::Create(*auction_config.trusted_scoring_signals_url));
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, FullConfig) {
  AuctionConfig auction_config = CreateFullConfig();
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest,
     perBuyerPrioritySignalsCannotOverrideBrowserSignals) {
  const url::Origin kBuyer = url::Origin::Create(GURL("https://buyer.test"));

  AuctionConfig auction_config = CreateBasicConfig();
  auction_config.non_shared_params.interest_group_buyers.emplace();
  auction_config.non_shared_params.interest_group_buyers->push_back(kBuyer);
  auction_config.non_shared_params.per_buyer_priority_signals.emplace();
  (*auction_config.non_shared_params.per_buyer_priority_signals)[kBuyer] = {
      {"browserSignals.hats", 1}};

  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest,
     allBuyersPrioritySignalsCannotOverrideBrowserSignals) {
  AuctionConfig auction_config = CreateBasicConfig();
  auction_config.non_shared_params.all_buyers_priority_signals = {
      {"browserSignals.goats", 2}};
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, BuyerNotHttps) {
  AuctionConfig auction_config = CreateBasicConfig();
  auction_config.non_shared_params.interest_group_buyers.emplace();
  auction_config.non_shared_params.interest_group_buyers->push_back(
      url::Origin::Create(GURL("http://buyer.test")));
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, BuyerNotHttpsMultipleBuyers) {
  AuctionConfig auction_config = CreateBasicConfig();
  auction_config.non_shared_params.interest_group_buyers.emplace();
  auction_config.non_shared_params.interest_group_buyers->push_back(
      url::Origin::Create(GURL("https://buyer1.test")));
  auction_config.non_shared_params.interest_group_buyers->push_back(
      url::Origin::Create(GURL("http://buyer2.test")));
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, ComponentAuctionUrlHttps) {
  AuctionConfig auction_config = CreateBasicConfig();
  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateBasicConfig(GURL("http://seller.test")));
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, ComponentAuctionTooDeep) {
  AuctionConfig auction_config = CreateBasicConfig();
  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateBasicConfig());
  auction_config.non_shared_params.component_auctions[0]
      .non_shared_params.component_auctions.emplace_back(CreateBasicConfig());
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, ComponentAuctionSuccessSingleBasic) {
  AuctionConfig auction_config = CreateBasicConfig();
  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateBasicConfig());
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, ComponentAuctionSuccessMutipleFull) {
  AuctionConfig auction_config = CreateFullConfig();
  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateFullConfig());
  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateFullConfig());
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

}  // namespace

}  // namespace blink

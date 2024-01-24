// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/auction_config_mojom_traits.h"

#include <string>
#include <tuple>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "base/uuid.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/common/interest_group/auction_config_test_util.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/seller_capabilities.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

bool operator==(
    const AuctionConfig::NonSharedParams::AuctionReportBuyersConfig& a,
    const AuctionConfig::NonSharedParams::AuctionReportBuyersConfig& b) {
  return std::tie(a.bucket, a.scale) == std::tie(b.bucket, b.scale);
}

bool operator==(const DirectFromSellerSignals& a,
                const DirectFromSellerSignals& b) {
  return std::tie(a.prefix, a.per_buyer_signals, a.seller_signals,
                  a.auction_signals) == std::tie(b.prefix, b.per_buyer_signals,
                                                 b.seller_signals,
                                                 b.auction_signals);
}

bool operator==(const AuctionConfig::BuyerTimeouts& a,
                const AuctionConfig::BuyerTimeouts& b) {
  return std::tie(a.all_buyers_timeout, a.per_buyer_timeouts) ==
         std::tie(b.all_buyers_timeout, b.per_buyer_timeouts);
}

bool operator==(const AdCurrency& a, const AdCurrency& b) {
  return a.currency_code() == b.currency_code();
}

bool operator==(const AuctionConfig::BuyerCurrencies& a,
                const AuctionConfig::BuyerCurrencies& b) {
  return std::tie(a.all_buyers_currency, a.per_buyer_currencies) ==
         std::tie(b.all_buyers_currency, b.per_buyer_currencies);
}

template <class T>
bool operator==(const AuctionConfig::MaybePromise<T>& a,
                const AuctionConfig::MaybePromise<T>& b) {
  return a.tag() == b.tag() && a.value() == b.value();
}

bool operator==(const AuctionConfig& a, const AuctionConfig& b);

bool operator==(const AuctionConfig::NonSharedParams& a,
                const AuctionConfig::NonSharedParams& b) {
  return std::tie(a.interest_group_buyers, a.auction_signals, a.seller_signals,
                  a.seller_timeout, a.per_buyer_signals, a.buyer_timeouts,
                  a.buyer_cumulative_timeouts, a.seller_currency,
                  a.buyer_currencies, a.per_buyer_group_limits,
                  a.all_buyers_group_limit, a.per_buyer_priority_signals,
                  a.all_buyers_priority_signals, a.auction_report_buyer_keys,
                  a.auction_report_buyers, a.requested_size,
                  a.all_slots_requested_sizes, a.required_seller_capabilities,
                  a.auction_nonce, a.component_auctions) ==
         std::tie(b.interest_group_buyers, b.auction_signals, b.seller_signals,
                  b.seller_timeout, b.per_buyer_signals, b.buyer_timeouts,
                  b.buyer_cumulative_timeouts, b.seller_currency,
                  b.buyer_currencies, b.per_buyer_group_limits,
                  b.all_buyers_group_limit, b.per_buyer_priority_signals,
                  b.all_buyers_priority_signals, b.auction_report_buyer_keys,
                  b.auction_report_buyers, b.requested_size,
                  b.all_slots_requested_sizes, b.required_seller_capabilities,
                  b.auction_nonce, b.component_auctions);
}

bool operator==(const AuctionConfig& a, const AuctionConfig& b) {
  return std::tie(a.seller, a.decision_logic_url, a.trusted_scoring_signals_url,
                  a.max_trusted_scoring_signals_url_length, a.non_shared_params,
                  a.direct_from_seller_signals,
                  a.expects_direct_from_seller_signals_header_ad_slot,
                  a.seller_experiment_group_id, a.all_buyer_experiment_group_id,
                  a.per_buyer_experiment_group_ids,
                  a.expects_additional_bids) ==
         std::tie(b.seller, b.decision_logic_url, b.trusted_scoring_signals_url,
                  b.max_trusted_scoring_signals_url_length, b.non_shared_params,
                  b.direct_from_seller_signals,
                  b.expects_direct_from_seller_signals_header_ad_slot,
                  b.seller_experiment_group_id, b.all_buyer_experiment_group_id,
                  b.per_buyer_experiment_group_ids, b.expects_additional_bids);
}

namespace {

// Cases for direct_from_seller_signals test parameterization.

constexpr char kPerBuyerSignals[] = "per-buyer-signals";
constexpr char kSellerSignals[] = "seller-signals";
constexpr char kAuctionSignals[] = "auction-signals";

constexpr char kBundleUrl[] = "bundle-url";
constexpr char kPrefix[] = "prefix";

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

template <class MojoType, class PromiseValue>
bool SerializeAndDeserialize(
    const AuctionConfig::MaybePromise<PromiseValue>& in) {
  AuctionConfig::MaybePromise<PromiseValue> out;
  bool success = mojo::test::SerializeAndDeserialize<MojoType>(in, out);
  if (success) {
    EXPECT_EQ(in, out);
  }
  return success;
}

bool SerializeAndDeserialize(const AuctionConfig::BuyerTimeouts& in) {
  AuctionConfig::BuyerTimeouts out;
  bool success = mojo::test::SerializeAndDeserialize<
      blink::mojom::AuctionAdConfigBuyerTimeouts>(in, out);
  if (success) {
    EXPECT_EQ(in, out);
  }
  return success;
}

bool SerializeAndDeserialize(const AuctionConfig::BuyerCurrencies& in) {
  AuctionConfig::BuyerCurrencies out;
  bool success = mojo::test::SerializeAndDeserialize<
      blink::mojom::AuctionAdConfigBuyerCurrencies>(in, out);
  if (success) {
    EXPECT_EQ(in, out);
  }
  return success;
}

bool SerializeAndDeserialize(const AdCurrency& in) {
  AdCurrency out;
  bool success =
      mojo::test::SerializeAndDeserialize<blink::mojom::AdCurrency>(in, out);
  if (success) {
    EXPECT_EQ(in.currency_code(), out.currency_code());
  }
  return success;
}

bool SerializeAndDeserialize(const AuctionConfig::ServerResponseConfig& in) {
  AuctionConfig::ServerResponseConfig out;
  bool success = mojo::test::SerializeAndDeserialize<
      blink::mojom::AuctionAdServerResponseConfig>(in, out);
  if (success) {
    EXPECT_EQ(in.request_id, out.request_id);
  }
  return success;
}

TEST(AuctionConfigMojomTraitsTest, Empty) {
  AuctionConfig auction_config;
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, Basic) {
  AuctionConfig auction_config = CreateBasicAuctionConfig();
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, SellerNotHttps) {
  AuctionConfig auction_config =
      CreateBasicAuctionConfig(GURL("http://seller.test"));
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, SellerDecisionUrlMismatch) {
  AuctionConfig auction_config =
      CreateBasicAuctionConfig(GURL("http://seller.test"));
  // Different origin than seller, but same scheme.
  auction_config.decision_logic_url = GURL("https://not.seller.test/foo");
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));

  auction_config = CreateBasicAuctionConfig(GURL("https://seller.test"));
  // This blob URL should be considered same-origin to the seller, but the
  // scheme is wrong.
  auction_config.decision_logic_url = GURL("blob:https://seller.test/foo");
  ASSERT_EQ(auction_config.seller,
            url::Origin::Create(*auction_config.decision_logic_url));
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, SellerScoringSignalsUrlMismatch) {
  AuctionConfig auction_config =
      CreateBasicAuctionConfig(GURL("http://seller.test"));
  // Different origin than seller, but same scheme.
  auction_config.trusted_scoring_signals_url =
      GURL("https://not.seller.test/foo");
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));

  auction_config = CreateBasicAuctionConfig(GURL("https://seller.test"));
  // This blob URL should be considered same-origin to the seller, but the
  // scheme is wrong.
  auction_config.trusted_scoring_signals_url =
      GURL("blob:https://seller.test/foo");
  ASSERT_EQ(auction_config.seller,
            url::Origin::Create(*auction_config.trusted_scoring_signals_url));
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, FullConfig) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest,
     perBuyerPrioritySignalsCannotOverrideBrowserSignals) {
  const url::Origin kBuyer = url::Origin::Create(GURL("https://buyer.test"));

  AuctionConfig auction_config = CreateBasicAuctionConfig();
  auction_config.non_shared_params.interest_group_buyers.emplace();
  auction_config.non_shared_params.interest_group_buyers->push_back(kBuyer);
  auction_config.non_shared_params.per_buyer_priority_signals.emplace();
  (*auction_config.non_shared_params.per_buyer_priority_signals)[kBuyer] = {
      {"browserSignals.hats", 1}};

  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest,
     allBuyersPrioritySignalsCannotOverrideBrowserSignals) {
  AuctionConfig auction_config = CreateBasicAuctionConfig();
  auction_config.non_shared_params.all_buyers_priority_signals = {
      {"browserSignals.goats", 2}};
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, BuyerNotHttps) {
  AuctionConfig auction_config = CreateBasicAuctionConfig();
  auction_config.non_shared_params.interest_group_buyers.emplace();
  auction_config.non_shared_params.interest_group_buyers->push_back(
      url::Origin::Create(GURL("http://buyer.test")));
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, BuyerNotHttpsMultipleBuyers) {
  AuctionConfig auction_config = CreateBasicAuctionConfig();
  auction_config.non_shared_params.interest_group_buyers.emplace();
  auction_config.non_shared_params.interest_group_buyers->push_back(
      url::Origin::Create(GURL("https://buyer1.test")));
  auction_config.non_shared_params.interest_group_buyers->push_back(
      url::Origin::Create(GURL("http://buyer2.test")));
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, ComponentAuctionUrlHttps) {
  AuctionConfig auction_config = CreateBasicAuctionConfig();
  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateBasicAuctionConfig(GURL("http://seller.test")));
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, ComponentAuctionTooDeep) {
  AuctionConfig auction_config = CreateBasicAuctionConfig();
  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateBasicAuctionConfig());
  auction_config.non_shared_params.component_auctions[0]
      .non_shared_params.component_auctions.emplace_back(
          CreateBasicAuctionConfig());
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, ComponentAuctionWithNonce) {
  AuctionConfig auction_config = CreateBasicAuctionConfig();
  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateBasicAuctionConfig());
  auction_config.non_shared_params.component_auctions[0]
      .non_shared_params.auction_nonce = base::Uuid::GenerateRandomV4();
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest,
     TopLevelAuctionHasBuyersAndComponentAuction) {
  AuctionConfig auction_config = CreateBasicAuctionConfig();
  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateBasicAuctionConfig());
  auction_config.non_shared_params.interest_group_buyers.emplace();
  auction_config.non_shared_params.interest_group_buyers->emplace_back(
      url::Origin::Create(GURL("https://buyer.test")));
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, ComponentAuctionSuccessSingleBasic) {
  AuctionConfig auction_config = CreateBasicAuctionConfig();
  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateBasicAuctionConfig());
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, ComponentAuctionSuccessMultipleFull) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  // The top-level auction cannot have buyers in a component auction.
  auction_config.non_shared_params.interest_group_buyers = {};
  auction_config.direct_from_seller_signals.mutable_value_for_testing()
      .value()
      .per_buyer_signals.clear();
  // Or additional bids.
  auction_config.expects_additional_bids = false;

  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateFullAuctionConfig());
  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateFullAuctionConfig());

  EXPECT_TRUE(SerializeAndDeserialize(auction_config));

  // Turning `expects_additional_bids` on at top-level makes it fail.
  auction_config.expects_additional_bids = true;
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, DuplicateAllSlotsRequestedSizes) {
  const AdSize kSize1 = AdSize(70.5, AdSize::LengthUnit::kScreenWidth, 70.6,
                               AdSize::LengthUnit::kScreenHeight);
  const AdSize kSize2 = AdSize(100, AdSize::LengthUnit::kPixels, 110,
                               AdSize::LengthUnit::kPixels);

  AuctionConfig auction_config = CreateBasicAuctionConfig();
  // An empty list is not allowed.
  auction_config.non_shared_params.all_slots_requested_sizes.emplace();
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));

  // Add one AdSize. List should be allowed.
  auction_config.non_shared_params.all_slots_requested_sizes->emplace_back(
      kSize1);
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));

  // Set `requested_size` to a different AdSize. List should not be allowed,
  // since it doesn't include `requested_size`.
  auction_config.non_shared_params.requested_size = kSize2;
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));

  // Set `requested_size` to the same AdSize. List should be allowed.
  auction_config.non_shared_params.requested_size = kSize1;
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));

  // Add the same AdSize again, list should no longer be allowed.
  auction_config.non_shared_params.all_slots_requested_sizes->emplace_back(
      kSize1);
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));

  // Replace the second AdSize with a different value, the list should still be
  // allowed again.
  auction_config.non_shared_params.all_slots_requested_sizes->back() = kSize2;
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));

  // Set the `requested_size` to the size of the second AdList. The list should
  // still be allowed.
  auction_config.non_shared_params.requested_size = kSize2;
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));

  // Add the second AdSize a second time, and the list should not be allowed
  // again.
  auction_config.non_shared_params.all_slots_requested_sizes->emplace_back(
      kSize2);
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest,
     DirectFromSellerSignalsPrefixWithQueryString) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  auction_config.direct_from_seller_signals.mutable_value_for_testing()
      ->prefix = GURL("https://seller.test/json?queryPart");
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, DirectFromSellerSignalsBuyerNotPresent) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  DirectFromSellerSignalsSubresource& buyer2_subresource =
      auction_config.direct_from_seller_signals.mutable_value_for_testing()
          ->per_buyer_signals[url::Origin::Create(GURL("https://buyer2.test"))];
  buyer2_subresource.bundle_url = GURL("https://seller.test/bundle");
  buyer2_subresource.token = base::UnguessableToken::Create();
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest,
     DirectFromSellerSignalsNoDirectFromSellerSignals) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  auction_config.direct_from_seller_signals =
      AuctionConfig::MaybePromiseDirectFromSellerSignals::FromValue(
          absl::nullopt);
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, DirectFromSellerSignalsNoPerBuyerSignals) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  auction_config.direct_from_seller_signals.mutable_value_for_testing()
      ->per_buyer_signals.clear();
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, DirectFromSellerSignalsNoSellerSignals) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  auction_config.direct_from_seller_signals.mutable_value_for_testing()
      ->seller_signals = absl::nullopt;
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, DirectFromSellerSignalsNoAuctionSignals) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  auction_config.direct_from_seller_signals.mutable_value_for_testing()
      ->auction_signals = absl::nullopt;
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, DirectFromSellerSignalsHeaderAdSlot) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  auction_config.direct_from_seller_signals =
      AuctionConfig::MaybePromiseDirectFromSellerSignals::FromValue(
          absl::nullopt);
  auction_config.expects_direct_from_seller_signals_header_ad_slot = true;
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest,
     DirectFromSellerSignalsCantHaveBothBundlesAndHeaderAdSlot) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  auction_config.expects_direct_from_seller_signals_header_ad_slot = true;
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest,
     DirectFromSellerSignalsCantHaveBothBundlesAndHeaderAdSlotPromise) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  auction_config.direct_from_seller_signals =
      AuctionConfig::MaybePromiseDirectFromSellerSignals::FromPromise();
  auction_config.expects_direct_from_seller_signals_header_ad_slot = true;
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, MaybePromiseJson) {
  {
    AuctionConfig::MaybePromiseJson json =
        AuctionConfig::MaybePromiseJson::FromValue("{A: 42}");
    EXPECT_TRUE(
        SerializeAndDeserialize<blink::mojom::AuctionAdConfigMaybePromiseJson>(
            json));
  }

  {
    AuctionConfig::MaybePromiseJson nothing =
        AuctionConfig::MaybePromiseJson::FromValue(absl::nullopt);
    EXPECT_TRUE(
        SerializeAndDeserialize<blink::mojom::AuctionAdConfigMaybePromiseJson>(
            nothing));
  }

  {
    AuctionConfig::MaybePromiseJson promise =
        AuctionConfig::MaybePromiseJson::FromPromise();
    EXPECT_TRUE(
        SerializeAndDeserialize<blink::mojom::AuctionAdConfigMaybePromiseJson>(
            promise));
  }
}

TEST(AuctionConfigMojomTraitsTest, MaybePromisePerBuyerSignals) {
  {
    absl::optional<base::flat_map<url::Origin, std::string>> value;
    value.emplace();
    value->emplace(url::Origin::Create(GURL("https://example.com")), "42");
    AuctionConfig::MaybePromisePerBuyerSignals signals =
        AuctionConfig::MaybePromisePerBuyerSignals::FromValue(std::move(value));
    EXPECT_TRUE(
        SerializeAndDeserialize<
            blink::mojom::AuctionAdConfigMaybePromisePerBuyerSignals>(signals));
  }

  {
    AuctionConfig::MaybePromisePerBuyerSignals signals =
        AuctionConfig::MaybePromisePerBuyerSignals::FromPromise();
    EXPECT_TRUE(
        SerializeAndDeserialize<
            blink::mojom::AuctionAdConfigMaybePromisePerBuyerSignals>(signals));
  }
}

TEST(AuctionConfigMojomTraitsTest, BuyerTimeouts) {
  {
    AuctionConfig::BuyerTimeouts value;
    value.all_buyers_timeout.emplace(base::Milliseconds(10));
    value.per_buyer_timeouts.emplace();
    value.per_buyer_timeouts->emplace(
        url::Origin::Create(GURL("https://example.com")),
        base::Milliseconds(50));
    value.per_buyer_timeouts->emplace(
        url::Origin::Create(GURL("https://example.org")),
        base::Milliseconds(20));
    EXPECT_TRUE(SerializeAndDeserialize(value));
  }
  {
    AuctionConfig::BuyerTimeouts value;
    EXPECT_TRUE(SerializeAndDeserialize(value));
  }
}

TEST(AuctionConfigMojomTraitsTest, MaybePromiseBuyerTimeouts) {
  {
    AuctionConfig::BuyerTimeouts value;
    value.all_buyers_timeout.emplace(base::Milliseconds(10));
    value.per_buyer_timeouts.emplace();
    value.per_buyer_timeouts->emplace(
        url::Origin::Create(GURL("https://example.com")),
        base::Milliseconds(50));
    AuctionConfig::MaybePromiseBuyerTimeouts timeouts =
        AuctionConfig::MaybePromiseBuyerTimeouts::FromValue(std::move(value));
    EXPECT_TRUE(
        SerializeAndDeserialize<
            blink::mojom::AuctionAdConfigMaybePromiseBuyerTimeouts>(timeouts));
  }

  {
    AuctionConfig::MaybePromiseBuyerTimeouts timeouts =
        AuctionConfig::MaybePromiseBuyerTimeouts::FromPromise();
    EXPECT_TRUE(
        SerializeAndDeserialize<
            blink::mojom::AuctionAdConfigMaybePromiseBuyerTimeouts>(timeouts));
  }
}

TEST(AuctionConfigMojomTraitsTest, BuyerCurrencies) {
  {
    AuctionConfig::BuyerCurrencies value;
    value.all_buyers_currency = blink::AdCurrency::From("EUR");
    value.per_buyer_currencies.emplace();
    value.per_buyer_currencies->emplace(
        url::Origin::Create(GURL("https://example.co.uk")),
        blink::AdCurrency::From("GBP"));
    value.per_buyer_currencies->emplace(
        url::Origin::Create(GURL("https://example.ca")),
        blink::AdCurrency::From("CAD"));
    EXPECT_TRUE(SerializeAndDeserialize(value));
  }
  {
    AuctionConfig::BuyerCurrencies value;
    EXPECT_TRUE(SerializeAndDeserialize(value));
  }
}

TEST(AuctionConfigMojomTraitsTest, AdCurrency) {
  {
    AdCurrency value = AdCurrency::From("EUR");
    EXPECT_TRUE(SerializeAndDeserialize(value));
  }
  {
    AdCurrency value;
    value.SetCurrencyCodeForTesting("eur");
    EXPECT_FALSE(SerializeAndDeserialize(value));
  }
  {
    AdCurrency value;
    value.SetCurrencyCodeForTesting("EURO");
    EXPECT_FALSE(SerializeAndDeserialize(value));
  }
}

TEST(AuctionConfigMojomTraitsTest, MaybePromiseDirectFromSellerSignals) {
  {
    AuctionConfig::MaybePromiseDirectFromSellerSignals signals =
        CreateFullAuctionConfig().direct_from_seller_signals;
    EXPECT_TRUE(
        SerializeAndDeserialize<
            blink::mojom::AuctionAdConfigMaybePromiseDirectFromSellerSignals>(
            signals));
  }

  {
    AuctionConfig::MaybePromiseDirectFromSellerSignals signals =
        AuctionConfig::MaybePromiseDirectFromSellerSignals::FromPromise();
    EXPECT_TRUE(
        SerializeAndDeserialize<
            blink::mojom::AuctionAdConfigMaybePromiseDirectFromSellerSignals>(
            signals));
  }
}

TEST(AuctionConfigMojomTraitsTest, ServerResponseConfig) {
  {
    AuctionConfig::ServerResponseConfig config;
    config.request_id = base::Uuid::GenerateRandomV4();
    EXPECT_TRUE(SerializeAndDeserialize(config));
  }
}

// Can't have `expects_additional_bids` without a nonce.
TEST(AuctionConfigMojomTraitsTest, AdditionalBidsNoNonce) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  ASSERT_TRUE(auction_config.expects_additional_bids);
  auction_config.non_shared_params.auction_nonce.reset();
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));

  auction_config.expects_additional_bids = false;
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

// Can't have `expects_additional_bids` with no interestGroupBuyers.
TEST(AuctionConfigMojomTraitsTest, AdditionalBidsNoInterestGroupBuyers) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  // These rely on interestGroupBuyers, so we have to clear these for this test.
  auction_config.direct_from_seller_signals.mutable_value_for_testing().reset();

  ASSERT_TRUE(auction_config.expects_additional_bids);
  auction_config.non_shared_params.interest_group_buyers.reset();
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));

  auction_config.expects_additional_bids = false;
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

// Can't have `expects_additional_bids` with empty interestGroupBuyers.
TEST(AuctionConfigMojomTraitsTest, AdditionalBidsEmptyInterestGroupBuyers) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  // These rely on interestGroupBuyers, so we have to clear these for this test.
  auction_config.direct_from_seller_signals.mutable_value_for_testing().reset();

  ASSERT_TRUE(auction_config.expects_additional_bids);
  auction_config.non_shared_params.interest_group_buyers->clear();
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));

  auction_config.expects_additional_bids = false;
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

class AuctionConfigMojomTraitsDirectFromSellerSignalsTest
    : public ::testing::TestWithParam<std::tuple<const char*, const char*>> {
 public:
  GURL& GetMutableURL(AuctionConfig& auction_config) const {
    const std::string which_path = WhichPath();
    DCHECK(!auction_config.direct_from_seller_signals.is_promise());
    if (which_path == kPrefix) {
      return auction_config.direct_from_seller_signals
          .mutable_value_for_testing()
          ->prefix;
    } else {
      EXPECT_EQ(which_path, kBundleUrl);
      const std::string which_bundle = WhichBundle();
      if (which_bundle == kPerBuyerSignals) {
        return auction_config.direct_from_seller_signals
            .mutable_value_for_testing()
            ->per_buyer_signals
            .at(url::Origin::Create(GURL("https://buyer.test")))
            .bundle_url;
      } else if (which_bundle == kSellerSignals) {
        return auction_config.direct_from_seller_signals
            .mutable_value_for_testing()
            ->seller_signals->bundle_url;
      } else {
        EXPECT_EQ(which_bundle, kAuctionSignals);
        return auction_config.direct_from_seller_signals
            .mutable_value_for_testing()
            ->auction_signals->bundle_url;
      }
    }
  }

  std::string GetURLPath() const {
    const std::string which_path = WhichPath();
    if (which_path == kBundleUrl) {
      return "/bundle";
    } else {
      EXPECT_EQ(which_path, kPrefix);
      return "/json";
    }
  }

 private:
  std::string WhichBundle() const { return std::get<0>(GetParam()); }
  std::string WhichPath() const { return std::get<1>(GetParam()); }
};

TEST_P(AuctionConfigMojomTraitsDirectFromSellerSignalsTest, NotHttps) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  GetMutableURL(auction_config) = GURL("http://seller.test" + GetURLPath());
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST_P(AuctionConfigMojomTraitsDirectFromSellerSignalsTest, WrongOrigin) {
  AuctionConfig auction_config = CreateFullAuctionConfig();
  GetMutableURL(auction_config) = GURL("https://seller2.test" + GetURLPath());
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

INSTANTIATE_TEST_SUITE_P(All,
                         AuctionConfigMojomTraitsDirectFromSellerSignalsTest,
                         ::testing::Combine(::testing::Values(kPerBuyerSignals,
                                                              kSellerSignals,
                                                              kAuctionSignals),
                                            ::testing::Values(kBundleUrl,
                                                              kPrefix)));

}  // namespace

}  // namespace blink

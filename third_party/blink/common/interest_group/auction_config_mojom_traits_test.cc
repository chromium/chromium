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
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
                  a.auction_report_buyers, a.required_seller_capabilities,
                  a.component_auctions) ==
         std::tie(b.interest_group_buyers, b.auction_signals, b.seller_signals,
                  b.seller_timeout, b.per_buyer_signals, b.buyer_timeouts,
                  b.buyer_cumulative_timeouts, b.seller_currency,
                  b.buyer_currencies, b.per_buyer_group_limits,
                  b.all_buyers_group_limit, b.per_buyer_priority_signals,
                  b.all_buyers_priority_signals, b.auction_report_buyer_keys,
                  b.auction_report_buyers, b.required_seller_capabilities,
                  b.component_auctions);
}

bool operator==(const AuctionConfig& a, const AuctionConfig& b) {
  return std::tie(a.seller, a.decision_logic_url, a.trusted_scoring_signals_url,
                  a.non_shared_params, a.direct_from_seller_signals,
                  a.seller_experiment_group_id, a.all_buyer_experiment_group_id,
                  a.per_buyer_experiment_group_ids) ==
         std::tie(b.seller, b.decision_logic_url, b.trusted_scoring_signals_url,
                  b.non_shared_params, b.direct_from_seller_signals,
                  b.seller_experiment_group_id, b.all_buyer_experiment_group_id,
                  b.per_buyer_experiment_group_ids);
}

namespace {

constexpr char kSellerOriginStr[] = "https://seller.test";

// Cases for direct_from_seller_signals test parameterization.

constexpr char kPerBuyerSignals[] = "per-buyer-signals";
constexpr char kSellerSignals[] = "seller-signals";
constexpr char kAuctionSignals[] = "auction-signals";

constexpr char kBundleUrl[] = "bundle-url";
constexpr char kPrefix[] = "prefix";

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
  const url::Origin seller = url::Origin::Create(GURL(kSellerOriginStr));
  AuctionConfig auction_config = CreateBasicConfig();

  auction_config.trusted_scoring_signals_url = GURL("https://seller.test/bar");
  auction_config.seller_experiment_group_id = 1;
  auction_config.all_buyer_experiment_group_id = 2;

  const url::Origin buyer = url::Origin::Create(GURL("https://buyer.test"));
  auction_config.per_buyer_experiment_group_ids[buyer] = 3;

  AuctionConfig::NonSharedParams& non_shared_params =
      auction_config.non_shared_params;
  non_shared_params.interest_group_buyers.emplace();
  non_shared_params.interest_group_buyers->push_back(buyer);
  non_shared_params.auction_signals =
      AuctionConfig::MaybePromiseJson::FromValue("[4]");
  non_shared_params.seller_signals =
      AuctionConfig::MaybePromiseJson::FromValue("[5]");
  non_shared_params.seller_timeout = base::Seconds(6);

  absl::optional<base::flat_map<url::Origin, std::string>> per_buyer_signals;
  per_buyer_signals.emplace();
  (*per_buyer_signals)[buyer] = "[7]";
  non_shared_params.per_buyer_signals =
      blink::AuctionConfig::MaybePromisePerBuyerSignals::FromValue(
          std::move(per_buyer_signals));

  AuctionConfig::BuyerTimeouts buyer_timeouts;
  buyer_timeouts.per_buyer_timeouts.emplace();
  (*buyer_timeouts.per_buyer_timeouts)[buyer] = base::Seconds(8);
  buyer_timeouts.all_buyers_timeout = base::Seconds(9);
  non_shared_params.buyer_timeouts =
      AuctionConfig::MaybePromiseBuyerTimeouts::FromValue(
          std::move(buyer_timeouts));

  AuctionConfig::BuyerCurrencies buyer_currencies;
  buyer_currencies.per_buyer_currencies.emplace();
  (*buyer_currencies.per_buyer_currencies)[buyer] = AdCurrency::From("CAD");
  buyer_currencies.all_buyers_currency = AdCurrency::From("USD");
  non_shared_params.buyer_currencies =
      AuctionConfig::MaybePromiseBuyerCurrencies::FromValue(
          std::move(buyer_currencies));

  non_shared_params.seller_currency = AdCurrency::From("EUR");

  AuctionConfig::BuyerTimeouts buyer_cumulative_timeouts;
  buyer_cumulative_timeouts.per_buyer_timeouts.emplace();
  (*buyer_cumulative_timeouts.per_buyer_timeouts)[buyer] = base::Seconds(432);
  buyer_cumulative_timeouts.all_buyers_timeout = base::Seconds(234);
  non_shared_params.buyer_cumulative_timeouts =
      AuctionConfig::MaybePromiseBuyerTimeouts::FromValue(
          std::move(buyer_cumulative_timeouts));

  non_shared_params.per_buyer_group_limits[buyer] = 10;
  non_shared_params.all_buyers_group_limit = 11;
  non_shared_params.per_buyer_priority_signals.emplace();
  (*non_shared_params.per_buyer_priority_signals)[buyer] = {
      {"hats", 1.5}, {"for", 0}, {"sale", -2}};
  non_shared_params.all_buyers_priority_signals = {
      {"goats", -1.5}, {"for", 5}, {"sale", 0}};
  non_shared_params.auction_report_buyer_keys = {absl::MakeUint128(1, 1),
                                                 absl::MakeUint128(1, 2)};
  non_shared_params.auction_report_buyers = {
      {AuctionConfig::NonSharedParams::BuyerReportType::kInterestGroupCount,
       {absl::MakeUint128(0, 0), 1.0}},
      {AuctionConfig::NonSharedParams::BuyerReportType::
           kTotalSignalsFetchLatency,
       {absl::MakeUint128(0, 1), 2.0}}};
  non_shared_params.required_seller_capabilities = {
      SellerCapabilities::kLatencyStats};

  DirectFromSellerSignalsSubresource
      direct_from_seller_signals_per_buyer_signals_buyer;
  direct_from_seller_signals_per_buyer_signals_buyer.bundle_url =
      GURL("https://seller.test/bundle");
  direct_from_seller_signals_per_buyer_signals_buyer.token =
      base::UnguessableToken::Create();

  DirectFromSellerSignalsSubresource direct_from_seller_seller_signals;
  direct_from_seller_seller_signals.bundle_url =
      GURL("https://seller.test/bundle");
  direct_from_seller_seller_signals.token = base::UnguessableToken::Create();

  DirectFromSellerSignalsSubresource direct_from_seller_auction_signals;
  direct_from_seller_auction_signals.bundle_url =
      GURL("https://seller.test/bundle");
  direct_from_seller_auction_signals.token = base::UnguessableToken::Create();

  DirectFromSellerSignals direct_from_seller_signals;
  direct_from_seller_signals.prefix = GURL("https://seller.test/json");
  direct_from_seller_signals.per_buyer_signals.insert(
      {buyer, std::move(direct_from_seller_signals_per_buyer_signals_buyer)});
  direct_from_seller_signals.seller_signals =
      std::move(direct_from_seller_seller_signals);
  direct_from_seller_signals.auction_signals =
      std::move(direct_from_seller_auction_signals);

  auction_config.direct_from_seller_signals =
      AuctionConfig::MaybePromiseDirectFromSellerSignals::FromValue(
          std::move(direct_from_seller_signals));

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

TEST(AuctionConfigMojomTraitsTest,
     TopLevelAuctionHasBuyersAndComponentAuction) {
  AuctionConfig auction_config = CreateBasicConfig();
  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateBasicConfig());
  auction_config.non_shared_params.interest_group_buyers.emplace();
  auction_config.non_shared_params.interest_group_buyers->emplace_back(
      url::Origin::Create(GURL("https://buyer.test")));
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, ComponentAuctionSuccessSingleBasic) {
  AuctionConfig auction_config = CreateBasicConfig();
  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateBasicConfig());
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, ComponentAuctionSuccessMultipleFull) {
  AuctionConfig auction_config = CreateFullConfig();
  // The top-level auction cannot have buyers in a component auction.
  auction_config.non_shared_params.interest_group_buyers = {};
  auction_config.direct_from_seller_signals.mutable_value_for_testing()
      .value()
      .per_buyer_signals.clear();

  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateFullConfig());
  auction_config.non_shared_params.component_auctions.emplace_back(
      CreateFullConfig());

  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest,
     DirectFromSellerSignalsPrefixWithQueryString) {
  AuctionConfig auction_config = CreateFullConfig();
  auction_config.direct_from_seller_signals.mutable_value_for_testing()
      ->prefix = GURL("https://seller.test/json?queryPart");
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, DirectFromSellerSignalsBuyerNotPresent) {
  AuctionConfig auction_config = CreateFullConfig();
  DirectFromSellerSignalsSubresource& buyer2_subresource =
      auction_config.direct_from_seller_signals.mutable_value_for_testing()
          ->per_buyer_signals[url::Origin::Create(GURL("https://buyer2.test"))];
  buyer2_subresource.bundle_url = GURL("https://seller.test/bundle");
  buyer2_subresource.token = base::UnguessableToken::Create();
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest,
     DirectFromSellerSignalsNoDirectFromSellerSignals) {
  AuctionConfig auction_config = CreateFullConfig();
  auction_config.direct_from_seller_signals =
      AuctionConfig::MaybePromiseDirectFromSellerSignals::FromValue(
          absl::nullopt);
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, DirectFromSellerSignalsNoPerBuyerSignals) {
  AuctionConfig auction_config = CreateFullConfig();
  auction_config.direct_from_seller_signals.mutable_value_for_testing()
      ->per_buyer_signals.clear();
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, DirectFromSellerSignalsNoSellerSignals) {
  AuctionConfig auction_config = CreateFullConfig();
  auction_config.direct_from_seller_signals.mutable_value_for_testing()
      ->seller_signals = absl::nullopt;
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
}

TEST(AuctionConfigMojomTraitsTest, DirectFromSellerSignalsNoAuctionSignals) {
  AuctionConfig auction_config = CreateFullConfig();
  auction_config.direct_from_seller_signals.mutable_value_for_testing()
      ->auction_signals = absl::nullopt;
  EXPECT_TRUE(SerializeAndDeserialize(auction_config));
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
        CreateFullConfig().direct_from_seller_signals;
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
  AuctionConfig auction_config = CreateFullConfig();
  GetMutableURL(auction_config) = GURL("http://seller.test" + GetURLPath());
  EXPECT_FALSE(SerializeAndDeserialize(auction_config));
}

TEST_P(AuctionConfigMojomTraitsDirectFromSellerSignalsTest, WrongOrigin) {
  AuctionConfig auction_config = CreateFullConfig();
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

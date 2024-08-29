// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/common/interest_group/auction_config_test_util.h"

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink {

namespace {
constexpr char kSellerOriginStr[] = "https://seller.test";
}  // namespace

AuctionConfig CreateBasicAuctionConfig(const GURL& decision_logic_url) {
  AuctionConfig auction_config;
  auction_config.seller = url::Origin::Create(decision_logic_url);
  auction_config.decision_logic_url = decision_logic_url;
  return auction_config;
}

AuctionConfig CreateFullAuctionConfig() {
  const url::Origin seller = url::Origin::Create(GURL(kSellerOriginStr));
  AuctionConfig auction_config = CreateBasicAuctionConfig();

  auction_config.trusted_scoring_signals_url = GURL("https://seller.test/bar");
  auction_config.seller_experiment_group_id = 1;
  auction_config.all_buyer_experiment_group_id = 2;

  const url::Origin buyer = url::Origin::Create(GURL("https://buyer.test"));
  auction_config.per_buyer_experiment_group_ids[buyer] = 3;

  const std::vector<blink::AuctionConfig::AdKeywordReplacement>
      deprecated_render_url_replacements = {
          blink::AuctionConfig::AdKeywordReplacement(
              {"${SELLER}", "ExampleSSP"})};
  auction_config.non_shared_params.deprecated_render_url_replacements =
      blink::AuctionConfig::MaybePromiseDeprecatedRenderURLReplacements::
          FromValue(deprecated_render_url_replacements);

  AuctionConfig::NonSharedParams& non_shared_params =
      auction_config.non_shared_params;
  non_shared_params.interest_group_buyers.emplace();
  non_shared_params.interest_group_buyers->push_back(buyer);
  non_shared_params.auction_signals =
      AuctionConfig::MaybePromiseJson::FromValue("[4]");
  non_shared_params.seller_signals =
      AuctionConfig::MaybePromiseJson::FromValue("[5]");
  non_shared_params.seller_timeout = base::Seconds(6);

  std::optional<base::flat_map<url::Origin, std::string>> per_buyer_signals;
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

  AuctionConfig::BuyerTimeouts buyer_cumulative_timeouts;
  buyer_cumulative_timeouts.per_buyer_timeouts.emplace();
  (*buyer_cumulative_timeouts.per_buyer_timeouts)[buyer] = base::Seconds(432);
  buyer_cumulative_timeouts.all_buyers_timeout = base::Seconds(234);
  non_shared_params.buyer_cumulative_timeouts =
      AuctionConfig::MaybePromiseBuyerTimeouts::FromValue(
          std::move(buyer_cumulative_timeouts));

  non_shared_params.reporting_timeout = base::Seconds(7);
  non_shared_params.seller_currency = AdCurrency::From("EUR");

  AuctionConfig::BuyerCurrencies buyer_currencies;
  buyer_currencies.per_buyer_currencies.emplace();
  (*buyer_currencies.per_buyer_currencies)[buyer] = AdCurrency::From("CAD");
  buyer_currencies.all_buyers_currency = AdCurrency::From("USD");
  non_shared_params.buyer_currencies =
      AuctionConfig::MaybePromiseBuyerCurrencies::FromValue(
          std::move(buyer_currencies));

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
  non_shared_params.auction_report_buyer_debug_mode_config.emplace();
  non_shared_params.auction_report_buyer_debug_mode_config->is_enabled = true;
  non_shared_params.auction_report_buyer_debug_mode_config->debug_key =
      0x8000000000000000u;

  non_shared_params.requested_size = AdSize(
      100, AdSize::LengthUnit::kPixels, 70, AdSize::LengthUnit::kScreenHeight);
  non_shared_params.all_slots_requested_sizes = {
      AdSize(100, AdSize::LengthUnit::kPixels, 70,
             AdSize::LengthUnit::kScreenHeight),
      AdSize(55.5, AdSize::LengthUnit::kScreenWidth, 50.5,
             AdSize::LengthUnit::kPixels),
  };
  non_shared_params.per_buyer_multi_bid_limits[buyer] = 10;
  non_shared_params.all_buyers_multi_bid_limit = 5;
  non_shared_params.required_seller_capabilities = {
      SellerCapabilities::kLatencyStats};

  non_shared_params.auction_nonce = base::Uuid::GenerateRandomV4();
  non_shared_params.max_trusted_scoring_signals_url_length = 2560;
  non_shared_params.trusted_scoring_signals_coordinator =
      url::Origin::Create(GURL("https://example.test"));

  non_shared_params.seller_real_time_reporting_type = AuctionConfig::
      NonSharedParams::RealTimeReportingType::kDefaultLocalReporting;
  non_shared_params.per_buyer_real_time_reporting_types.emplace();
  (*non_shared_params.per_buyer_real_time_reporting_types)[buyer] =
      AuctionConfig::NonSharedParams::RealTimeReportingType::
          kDefaultLocalReporting;

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

  auction_config.expects_additional_bids = true;

  auction_config.aggregation_coordinator_origin =
      url::Origin::Create(GURL("https://example.com"));

  return auction_config;
}

}  // namespace blink

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/auction_config_mojom_traits.h"

#include <cmath>
#include <optional>
#include <string>

#include "base/containers/contains.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom-shared.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace mojo {

namespace {

// Validates no key in `buyer_priority_signals` starts with "browserSignals.",
// which are reserved for values set by the browser.
bool AreBuyerPrioritySignalsValid(
    const base::flat_map<std::string, double>& buyer_priority_signals) {
  for (const auto& priority_signal : buyer_priority_signals) {
    if (base::StartsWith(priority_signal.first, "browserSignals.")) {
      return false;
    }
    if (!std::isfinite(priority_signal.second)) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool StructTraits<blink::mojom::DirectFromSellerSignalsSubresourceDataView,
                  blink::DirectFromSellerSignalsSubresource>::
    Read(blink::mojom::DirectFromSellerSignalsSubresourceDataView data,
         blink::DirectFromSellerSignalsSubresource* out) {
  if (!data.ReadBundleUrl(&out->bundle_url) || !data.ReadToken(&out->token)) {
    return false;
  }

  return true;
}

bool StructTraits<blink::mojom::DirectFromSellerSignalsDataView,
                  blink::DirectFromSellerSignals>::
    Read(blink::mojom::DirectFromSellerSignalsDataView data,
         blink::DirectFromSellerSignals* out) {
  if (!data.ReadPrefix(&out->prefix) ||
      !data.ReadPerBuyerSignals(&out->per_buyer_signals) ||
      !data.ReadSellerSignals(&out->seller_signals) ||
      !data.ReadAuctionSignals(&out->auction_signals)) {
    return false;
  }

  return true;
}

template <class View, class Wrapper>
bool AdConfigMaybePromiseTraitsHelper<View, Wrapper>::Read(View in,
                                                           Wrapper* out) {
  switch (in.tag()) {
    case View::Tag::kPromise:
      *out = Wrapper::FromPromise();
      return true;

    case View::Tag::kValue: {
      typename Wrapper::ValueType payload;
      if (!in.ReadValue(&payload)) {
        return false;
      }
      *out = Wrapper::FromValue(std::move(payload));
      return true;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

template struct BLINK_COMMON_EXPORT AdConfigMaybePromiseTraitsHelper<
    blink::mojom::AuctionAdConfigMaybePromiseJsonDataView,
    blink::AuctionConfig::MaybePromiseJson>;

template struct BLINK_COMMON_EXPORT AdConfigMaybePromiseTraitsHelper<
    blink::mojom::AuctionAdConfigMaybePromisePerBuyerSignalsDataView,
    blink::AuctionConfig::MaybePromisePerBuyerSignals>;

template struct BLINK_COMMON_EXPORT AdConfigMaybePromiseTraitsHelper<
    blink::mojom::AuctionAdConfigMaybePromiseBuyerTimeoutsDataView,
    blink::AuctionConfig::MaybePromiseBuyerTimeouts>;

template struct BLINK_COMMON_EXPORT AdConfigMaybePromiseTraitsHelper<
    blink::mojom::AuctionAdConfigMaybePromiseBuyerCurrenciesDataView,
    blink::AuctionConfig::MaybePromiseBuyerCurrencies>;

template struct BLINK_COMMON_EXPORT AdConfigMaybePromiseTraitsHelper<
    blink::mojom::AuctionAdConfigMaybePromiseDirectFromSellerSignalsDataView,
    blink::AuctionConfig::MaybePromiseDirectFromSellerSignals>;

template struct BLINK_COMMON_EXPORT AdConfigMaybePromiseTraitsHelper<
    blink::mojom::
        AuctionAdConfigMaybePromiseDeprecatedRenderURLReplacementsDataView,
    blink::AuctionConfig::MaybePromiseDeprecatedRenderURLReplacements>;

bool StructTraits<blink::mojom::AuctionAdConfigBuyerTimeoutsDataView,
                  blink::AuctionConfig::BuyerTimeouts>::
    Read(blink::mojom::AuctionAdConfigBuyerTimeoutsDataView data,
         blink::AuctionConfig::BuyerTimeouts* out) {
  if (!data.ReadPerBuyerTimeouts(&out->per_buyer_timeouts) ||
      !data.ReadAllBuyersTimeout(&out->all_buyers_timeout)) {
    return false;
  }
  if (out->per_buyer_timeouts) {
    for (const auto& timeout : *out->per_buyer_timeouts) {
      if (timeout.second.is_negative()) {
        return false;
      }
    }
  }
  if (out->all_buyers_timeout && out->all_buyers_timeout->is_negative()) {
    return false;
  }
  return true;
}

bool StructTraits<blink::mojom::AdKeywordReplacementDataView,
                  blink::AuctionConfig::AdKeywordReplacement>::
    Read(blink::mojom::AdKeywordReplacementDataView data,
         blink::AuctionConfig::AdKeywordReplacement* out) {
  if (!data.ReadMatch(&out->match) ||
      !data.ReadReplacement(&out->replacement)) {
    return false;
  }
  return out->IsValid();
}

bool StructTraits<blink::mojom::AdCurrencyDataView, blink::AdCurrency>::Read(
    blink::mojom::AdCurrencyDataView data,
    blink::AdCurrency* out) {
  std::string currency_code;
  if (!data.ReadCurrencyCode(&currency_code)) {
    return false;
  }
  if (!blink::IsValidAdCurrencyCode(currency_code)) {
    return false;
  }
  *out = blink::AdCurrency::From(currency_code);
  return true;
}

bool StructTraits<blink::mojom::AuctionAdConfigBuyerCurrenciesDataView,
                  blink::AuctionConfig::BuyerCurrencies>::
    Read(blink::mojom::AuctionAdConfigBuyerCurrenciesDataView data,
         blink::AuctionConfig::BuyerCurrencies* out) {
  if (!data.ReadPerBuyerCurrencies(&out->per_buyer_currencies) ||
      !data.ReadAllBuyersCurrency(&out->all_buyers_currency)) {
    return false;
  }
  return true;
}

bool StructTraits<
    blink::mojom::AuctionReportBuyersConfigDataView,
    blink::AuctionConfig::NonSharedParams::AuctionReportBuyersConfig>::
    Read(
        blink::mojom::AuctionReportBuyersConfigDataView data,
        blink::AuctionConfig::NonSharedParams::AuctionReportBuyersConfig* out) {
  if (!data.ReadBucket(&out->bucket)) {
    return false;
  }
  out->scale = data.scale();
  if (!std::isfinite(out->scale)) {
    return false;
  }
  return true;
}

bool StructTraits<
    blink::mojom::AuctionReportBuyerDebugModeConfigDataView,
    blink::AuctionConfig::NonSharedParams::AuctionReportBuyerDebugModeConfig>::
    Read(blink::mojom::AuctionReportBuyerDebugModeConfigDataView data,
         blink::AuctionConfig::NonSharedParams::
             AuctionReportBuyerDebugModeConfig* out) {
  out->is_enabled = data.is_enabled();
  out->debug_key = data.debug_key();
  return true;
}

bool StructTraits<blink::mojom::AuctionAdServerResponseConfigDataView,
                  blink::AuctionConfig::ServerResponseConfig>::
    Read(blink::mojom::AuctionAdServerResponseConfigDataView data,
         blink::AuctionConfig::ServerResponseConfig* out) {
  return data.ReadRequestId(&out->request_id);
}

bool StructTraits<blink::mojom::AuctionAdConfigNonSharedParamsDataView,
                  blink::AuctionConfig::NonSharedParams>::
    Read(blink::mojom::AuctionAdConfigNonSharedParamsDataView data,
         blink::AuctionConfig::NonSharedParams* out) {
  if (!data.ReadInterestGroupBuyers(&out->interest_group_buyers) ||
      !data.ReadAuctionSignals(&out->auction_signals) ||
      !data.ReadSellerSignals(&out->seller_signals) ||
      !data.ReadSellerTimeout(&out->seller_timeout) ||
      !data.ReadPerBuyerSignals(&out->per_buyer_signals) ||
      !data.ReadBuyerTimeouts(&out->buyer_timeouts) ||
      !data.ReadReportingTimeout(&out->reporting_timeout) ||
      !data.ReadSellerCurrency(&out->seller_currency) ||
      !data.ReadBuyerCurrencies(&out->buyer_currencies) ||
      !data.ReadBuyerCumulativeTimeouts(&out->buyer_cumulative_timeouts) ||
      !data.ReadPerBuyerGroupLimits(&out->per_buyer_group_limits) ||
      !data.ReadPerBuyerPrioritySignals(&out->per_buyer_priority_signals) ||
      !data.ReadAllBuyersPrioritySignals(&out->all_buyers_priority_signals) ||
      !data.ReadAuctionReportBuyerKeys(&out->auction_report_buyer_keys) ||
      !data.ReadAuctionReportBuyers(&out->auction_report_buyers) ||
      !data.ReadAuctionReportBuyerDebugModeConfig(
          &out->auction_report_buyer_debug_mode_config) ||
      !data.ReadRequiredSellerCapabilities(
          &out->required_seller_capabilities) ||
      !data.ReadRequestedSize(&out->requested_size) ||
      !data.ReadAllSlotsRequestedSizes(&out->all_slots_requested_sizes) ||
      !data.ReadPerBuyerMultiBidLimits(&out->per_buyer_multi_bid_limits) ||
      !data.ReadAuctionNonce(&out->auction_nonce) ||
      !data.ReadSellerRealTimeReportingType(
          &out->seller_real_time_reporting_type) ||
      !data.ReadPerBuyerRealTimeReportingTypes(
          &out->per_buyer_real_time_reporting_types) ||
      !data.ReadComponentAuctions(&out->component_auctions) ||
      !data.ReadDeprecatedRenderUrlReplacements(
          &out->deprecated_render_url_replacements) ||
      !data.ReadTrustedScoringSignalsCoordinator(
          &out->trusted_scoring_signals_coordinator)) {
    return false;
  }

  if (out->seller_timeout && out->seller_timeout->is_negative()) {
    return false;
  }

  if (out->reporting_timeout && out->reporting_timeout->is_negative()) {
    return false;
  }

  // Negative length limit is invalid.
  if (data.max_trusted_scoring_signals_url_length() < 0) {
    return false;
  }
  out->max_trusted_scoring_signals_url_length =
      data.max_trusted_scoring_signals_url_length();

  // Coodinator must be HTTPS. This also excludes opaque origins, for which
  // scheme() returns an empty string.
  if (out->trusted_scoring_signals_coordinator.has_value() &&
      out->trusted_scoring_signals_coordinator->scheme() != url::kHttpsScheme) {
    return false;
  }

  out->all_buyers_group_limit = data.all_buyers_group_limit();

  // Only one of `interest_group_buyers` or `component_auctions` may be
  // non-empty.
  if (out->interest_group_buyers && out->interest_group_buyers->size() > 0 &&
      out->component_auctions.size() > 0) {
    return false;
  }

  if (out->interest_group_buyers) {
    for (const auto& buyer : *out->interest_group_buyers) {
      // Buyers must be HTTPS.
      if (buyer.scheme() != url::kHttpsScheme) {
        return false;
      }
    }
  }

  if (out->per_buyer_priority_signals) {
    for (const auto& per_buyer_priority_signals :
         *out->per_buyer_priority_signals) {
      if (!AreBuyerPrioritySignalsValid(per_buyer_priority_signals.second)) {
        return false;
      }
    }
  }
  if (out->all_buyers_priority_signals &&
      !AreBuyerPrioritySignalsValid(*out->all_buyers_priority_signals)) {
    return false;
  }

  out->all_buyers_multi_bid_limit = data.all_buyers_multi_bid_limit();

  for (const auto& component_auction : out->component_auctions) {
    // TODO(1457241): Add support for multi-level auctions including server-side
    // auctions.
    // Component auctions may not have their own nested component auctions.
    if (!component_auction.non_shared_params.component_auctions.empty()) {
      return false;
    }
  }

  if (out->all_slots_requested_sizes) {
    // `all_slots_requested_sizes` must not be empty
    if (out->all_slots_requested_sizes->empty()) {
      return false;
    }

    base::flat_set<blink::AdSize> ad_sizes(
        out->all_slots_requested_sizes->begin(),
        out->all_slots_requested_sizes->end());
    // Each entry in `all_slots_requested_sizes` must be distinct.
    if (out->all_slots_requested_sizes->size() != ad_sizes.size()) {
      return false;
    }

    // If `all_slots_requested_sizes` is set, `requested_size` must be in it.
    if (out->requested_size &&
        !base::Contains(ad_sizes, *out->requested_size)) {
      return false;
    }
  }

  return true;
}

bool StructTraits<blink::mojom::AuctionAdConfigDataView, blink::AuctionConfig>::
    Read(blink::mojom::AuctionAdConfigDataView data,
         blink::AuctionConfig* out) {
  if (!data.ReadSeller(&out->seller) ||
      !data.ReadServerResponse(&out->server_response) ||
      !data.ReadDecisionLogicUrl(&out->decision_logic_url) ||
      !data.ReadTrustedScoringSignalsUrl(&out->trusted_scoring_signals_url) ||
      !data.ReadAuctionAdConfigNonSharedParams(&out->non_shared_params) ||
      !data.ReadDirectFromSellerSignals(&out->direct_from_seller_signals) ||
      !data.ReadPerBuyerExperimentGroupIds(
          &out->per_buyer_experiment_group_ids) ||
      !data.ReadAggregationCoordinatorOrigin(
          &out->aggregation_coordinator_origin)) {
    return false;
  }

  out->expects_additional_bids = data.expects_additional_bids();
  // An auction that expects additional bids must have an auction nonce provided
  // on the config.
  if (out->expects_additional_bids &&
      !out->non_shared_params.auction_nonce.has_value()) {
    return false;
  }

  // An auction that expects additional bids must have the anticipated buyers of
  // those additional bids included in `interest_group_buyers`. This is
  // necessary so that the negative interest groups of those buyers are loaded,
  // and, as such, the auction rejects any additional bid whose buyer is not
  // included in `interest_group_buyers`. This asserts the weaker expectation
  // that the config must provide at least some `interest_group_buyers`.
  if (out->expects_additional_bids &&
      (!out->non_shared_params.interest_group_buyers.has_value() ||
       out->non_shared_params.interest_group_buyers->empty())) {
    return false;
  }

  out->expects_direct_from_seller_signals_header_ad_slot =
      data.expects_direct_from_seller_signals_header_ad_slot();

  out->seller_experiment_group_id = data.seller_experiment_group_id();
  out->all_buyer_experiment_group_id = data.all_buyer_experiment_group_id();

  // Seller must be HTTPS. This also excludes opaque origins, for which scheme()
  // returns an empty string.
  if (out->seller.scheme() != url::kHttpsScheme) {
    return false;
  }

  // We need at least 1 of server response and decision logic url.
  if (!out->server_response && !out->decision_logic_url) {
    return false;
  }
  // We always need decision logic for multi-level auctions.
  if (!out->non_shared_params.component_auctions.empty() &&
      !out->decision_logic_url) {
    return false;
  }
  for (const auto& component_auction :
       out->non_shared_params.component_auctions) {
    // We need at least 1 of server response and decision logic url.
    if (!component_auction.server_response &&
        !component_auction.decision_logic_url) {
      return false;
    }
  }

  // Should not have `expects_additional_bids` on non-leaf auctions.
  if (!out->non_shared_params.component_auctions.empty() &&
      out->expects_additional_bids) {
    return false;
  }

  // If present and valid, `decision_logic_url` and
  // `trusted_scoring_signals_url` must share the seller's origin, and must be
  // HTTPS. Need to explicitly check the scheme because some non-HTTPS URLs may
  // have HTTPS origins (e.g., blob URLs). Trusted signals URLs also have
  // additional restrictions (no query, etc).
  //
  // Invalid GURLs are allowed through because even though the renderer code
  // doesn't let invalid GURLs through, trying to pass a too-long GURL through
  // Mojo results in an invalid GURL on the other side. Rather than preventing
  // that from happening through, GURL consumers generally just let such GURLs
  // flow to the network stack, where they return network errors, so we copy
  // that behavior with auctions. Even when one component of an auction has
  // invalid GURLs due to this, other components may not, and it's possible
  // there's a winner.
  if ((out->decision_logic_url && out->decision_logic_url->is_valid() &&
       !out->IsHttpsAndMatchesSellerOrigin(*out->decision_logic_url)) ||
      (out->trusted_scoring_signals_url &&
       out->trusted_scoring_signals_url->is_valid() &&
       !out->IsValidTrustedScoringSignalsURL(
           *out->trusted_scoring_signals_url))) {
    return false;
  }

  if ((out->direct_from_seller_signals.is_promise() ||
       out->direct_from_seller_signals.value() != std::nullopt) &&
      out->expects_direct_from_seller_signals_header_ad_slot) {
    // `direct_from_seller_signals` and
    // `expects_direct_from_seller_signals_header_ad_slot` may not be both used
    // in the same component auction, top-level auction, or non-component
    // auction.
    return false;
  }

  if (!out->direct_from_seller_signals.is_promise() &&
      !out->IsDirectFromSellerSignalsValid(
          out->direct_from_seller_signals.value())) {
    return false;
  }

  return true;
}

}  // namespace mojo

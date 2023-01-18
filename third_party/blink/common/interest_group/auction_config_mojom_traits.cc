// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/auction_config_mojom_traits.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace mojo {

namespace {

// Helper to check if `url` is HTTPS and has the specified origin. Used to
// validate seller URLs can be used with the seller's origin.
bool IsHttpsAndMatchesOrigin(const GURL& seller_url,
                             const url::Origin& seller_origin) {
  return seller_url.scheme() == url::kHttpsScheme &&
         url::Origin::Create(seller_url) == seller_origin;
}

// Validates no key in `buyer_priority_signals` starts with "browserSignals.",
// which are reserved for values set by the browser.
bool AreBuyerPrioritySignalsValid(
    const base::flat_map<std::string, double>& buyer_priority_signals) {
  for (const auto& priority_signal : buyer_priority_signals) {
    if (base::StartsWith(priority_signal.first, "browserSignals."))
      return false;
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

bool UnionTraits<blink::mojom::AuctionAdConfigMaybePromiseJsonDataView,
                 blink::AuctionConfig::MaybePromiseJson>::
    Read(blink::mojom::AuctionAdConfigMaybePromiseJsonDataView in,
         blink::AuctionConfig::MaybePromiseJson* out) {
  switch (in.tag()) {
    case blink::mojom::AuctionAdConfigMaybePromiseJsonDataView::Tag::kNothing:
      *out = blink::AuctionConfig::MaybePromiseJson::FromNothing();
      return true;

    case blink::mojom::AuctionAdConfigMaybePromiseJsonDataView::Tag::kPromise:
      *out = blink::AuctionConfig::MaybePromiseJson::FromPromise();
      return true;

    case blink::mojom::AuctionAdConfigMaybePromiseJsonDataView::Tag::kJson: {
      std::string json_payload;
      if (!in.ReadJson(&json_payload)) {
        return false;
      }
      *out = blink::AuctionConfig::MaybePromiseJson::FromJson(
          std::move(json_payload));
      return true;
    }
  }
  NOTREACHED();
  return false;
}

bool UnionTraits<
    blink::mojom::AuctionAdConfigMaybePromisePerBuyerSignalsDataView,
    blink::AuctionConfig::MaybePromisePerBuyerSignals>::
    Read(blink::mojom::AuctionAdConfigMaybePromisePerBuyerSignalsDataView in,
         blink::AuctionConfig::MaybePromisePerBuyerSignals* out) {
  switch (in.tag()) {
    case blink::mojom::AuctionAdConfigMaybePromisePerBuyerSignalsDataView::Tag::
        kPromise:
      *out = blink::AuctionConfig::MaybePromisePerBuyerSignals::FromPromise();
      return true;

    case blink::mojom::AuctionAdConfigMaybePromisePerBuyerSignalsDataView::Tag::
        kPerBuyerSignals: {
      absl::optional<base::flat_map<url::Origin, std::string>> payload;
      if (!in.ReadPerBuyerSignals(&payload)) {
        return false;
      }
      *out = blink::AuctionConfig::MaybePromisePerBuyerSignals::FromValue(
          std::move(payload));
      return true;
    }
  }
  NOTREACHED();
  return false;
}

bool StructTraits<blink::mojom::AuctionAdConfigBuyerTimeoutsDataView,
                  blink::AuctionConfig::BuyerTimeouts>::
    Read(blink::mojom::AuctionAdConfigBuyerTimeoutsDataView data,
         blink::AuctionConfig::BuyerTimeouts* out) {
  if (!data.ReadPerBuyerTimeouts(&out->per_buyer_timeouts) ||
      !data.ReadAllBuyersTimeout(&out->all_buyers_timeout)) {
    return false;
  }
  return true;
}

bool UnionTraits<blink::mojom::AuctionAdConfigMaybePromiseBuyerTimeoutsDataView,
                 blink::AuctionConfig::MaybePromiseBuyerTimeouts>::
    Read(blink::mojom::AuctionAdConfigMaybePromiseBuyerTimeoutsDataView in,
         blink::AuctionConfig::MaybePromiseBuyerTimeouts* out) {
  switch (in.tag()) {
    case blink::mojom::AuctionAdConfigMaybePromiseBuyerTimeoutsDataView::Tag::
        kPromise:
      *out = blink::AuctionConfig::MaybePromiseBuyerTimeouts::FromPromise();
      return true;

    case blink::mojom::AuctionAdConfigMaybePromiseBuyerTimeoutsDataView::Tag::
        kValue: {
      blink::AuctionConfig::BuyerTimeouts payload;
      if (!in.ReadValue(&payload)) {
        return false;
      }
      *out = blink::AuctionConfig::MaybePromiseBuyerTimeouts::FromValue(
          std::move(payload));
      return true;
    }
  }
  NOTREACHED();
  return false;
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
  return true;
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
      !data.ReadPerBuyerGroupLimits(&out->per_buyer_group_limits) ||
      !data.ReadPerBuyerPrioritySignals(&out->per_buyer_priority_signals) ||
      !data.ReadAllBuyersPrioritySignals(&out->all_buyers_priority_signals) ||
      !data.ReadAuctionReportBuyerKeys(&out->auction_report_buyer_keys) ||
      !data.ReadAuctionReportBuyers(&out->auction_report_buyers) ||
      !data.ReadComponentAuctions(&out->component_auctions)) {
    return false;
  }

  out->all_buyers_group_limit = data.all_buyers_group_limit();

  if (out->interest_group_buyers) {
    for (const auto& buyer : *out->interest_group_buyers) {
      // Buyers must be HTTPS.
      if (buyer.scheme() != url::kHttpsScheme)
        return false;
    }
  }

  if (out->per_buyer_priority_signals) {
    for (const auto& per_buyer_priority_signals :
         *out->per_buyer_priority_signals) {
      if (!AreBuyerPrioritySignalsValid(per_buyer_priority_signals.second))
        return false;
    }
  }
  if (out->all_buyers_priority_signals &&
      !AreBuyerPrioritySignalsValid(*out->all_buyers_priority_signals)) {
    return false;
  }

  for (const auto& component_auction : out->component_auctions) {
    // Component auctions may not have their own nested component auctions.
    if (!component_auction.non_shared_params.component_auctions.empty())
      return false;
  }

  return true;
}

bool StructTraits<blink::mojom::AuctionAdConfigDataView, blink::AuctionConfig>::
    Read(blink::mojom::AuctionAdConfigDataView data,
         blink::AuctionConfig* out) {
  if (!data.ReadSeller(&out->seller) ||
      !data.ReadDecisionLogicUrl(&out->decision_logic_url) ||
      !data.ReadTrustedScoringSignalsUrl(&out->trusted_scoring_signals_url) ||
      !data.ReadAuctionAdConfigNonSharedParams(&out->non_shared_params) ||
      !data.ReadDirectFromSellerSignals(&out->direct_from_seller_signals) ||
      !data.ReadPerBuyerExperimentGroupIds(
          &out->per_buyer_experiment_group_ids)) {
    return false;
  }

  if (data.has_seller_experiment_group_id())
    out->seller_experiment_group_id = data.seller_experiment_group_id();

  if (data.has_all_buyer_experiment_group_id())
    out->all_buyer_experiment_group_id = data.all_buyer_experiment_group_id();

  // Seller must be HTTPS. This also excludes opaque origins, for which scheme()
  // returns an empty string.
  if (out->seller.scheme() != url::kHttpsScheme)
    return false;

  // `decision_logic_url` and, if present, `trusted_scoring_signals_url` must
  // share the seller's origin, and must be HTTPS. Need to explicitly check the
  // scheme because some non-HTTPS URLs may have HTTPS origins (e.g., blob
  // URLs).
  if (!IsHttpsAndMatchesOrigin(out->decision_logic_url, out->seller) ||
      (out->trusted_scoring_signals_url &&
       !IsHttpsAndMatchesOrigin(*out->trusted_scoring_signals_url,
                                out->seller))) {
    return false;
  }

  absl::optional<blink::DirectFromSellerSignals> direct_from_seller_signals =
      out->direct_from_seller_signals;
  if (direct_from_seller_signals) {
    const GURL& prefix = direct_from_seller_signals->prefix;
    // The prefix can't have a query because the browser process appends its own
    // query suffix.
    if (prefix.has_query())
      return false;
    // NOTE: uuid-in-package isn't supported, since it doesn't support CORS.
    if (!IsHttpsAndMatchesOrigin(prefix, out->seller))
      return false;
    base::flat_set<url::Origin> interest_group_buyers(
        out->non_shared_params.interest_group_buyers
            ? *out->non_shared_params.interest_group_buyers
            : std::vector<url::Origin>());
    for (const auto& [buyer_origin, bundle_url] :
         direct_from_seller_signals->per_buyer_signals) {
      // The renderer shouldn't provide bundles for origins that aren't buyers
      // in this auction -- there would be no worklet to receive them.
      if (interest_group_buyers.count(buyer_origin) < 1)
        return false;
      // All DirectFromSellerSignals must come from the seller.
      if (!IsHttpsAndMatchesOrigin(bundle_url.bundle_url, out->seller))
        return false;
    }
    if (direct_from_seller_signals->seller_signals &&
        !IsHttpsAndMatchesOrigin(
            direct_from_seller_signals->seller_signals->bundle_url,
            out->seller)) {
      // All DirectFromSellerSignals must come from the seller.
      return false;
    }
    if (direct_from_seller_signals->auction_signals &&
        !IsHttpsAndMatchesOrigin(
            direct_from_seller_signals->auction_signals->bundle_url,
            out->seller)) {
      // All DirectFromSellerSignals must come from the seller.
      return false;
    }
  }

  return true;
}

}  // namespace mojo

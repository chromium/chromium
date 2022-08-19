// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/auction_config_mojom_traits.h"

#include <string>

#include "base/containers/flat_map.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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

bool StructTraits<blink::mojom::AuctionAdConfigNonSharedParamsDataView,
                  blink::AuctionConfig::NonSharedParams>::
    Read(blink::mojom::AuctionAdConfigNonSharedParamsDataView data,
         blink::AuctionConfig::NonSharedParams* out) {
  if (!data.ReadInterestGroupBuyers(&out->interest_group_buyers) ||
      !data.ReadAuctionSignals(&out->auction_signals) ||
      !data.ReadSellerSignals(&out->seller_signals) ||
      !data.ReadSellerTimeout(&out->seller_timeout) ||
      !data.ReadPerBuyerSignals(&out->per_buyer_signals) ||
      !data.ReadPerBuyerTimeouts(&out->per_buyer_timeouts) ||
      !data.ReadAllBuyersTimeout(&out->all_buyers_timeout) ||
      !data.ReadPerBuyerGroupLimits(&out->per_buyer_group_limits) ||
      !data.ReadPerBuyerPrioritySignals(&out->per_buyer_priority_signals) ||
      !data.ReadAllBuyersPrioritySignals(&out->all_buyers_priority_signals) ||
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

  return true;
}

}  // namespace mojo

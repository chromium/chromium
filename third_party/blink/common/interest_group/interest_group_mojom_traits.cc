// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/interest_group_mojom_traits.h"

#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"

namespace mojo {

bool StructTraits<
    blink::mojom::InterestGroupAdDataView,
    blink::InterestGroup::Ad>::Read(blink::mojom::InterestGroupAdDataView data,
                                    blink::InterestGroup::Ad* out) {
  if (!data.ReadRenderUrl(&out->render_url_) ||
      !data.ReadSizeGroup(&out->size_group) ||
      !data.ReadBuyerReportingId(&out->buyer_reporting_id) ||
      !data.ReadBuyerAndSellerReportingId(
          &out->buyer_and_seller_reporting_id) ||
      !data.ReadSelectableBuyerAndSellerReportingIds(
          &out->selectable_buyer_and_seller_reporting_ids) ||
      !data.ReadMetadata(&out->metadata) ||
      !data.ReadAdRenderId(&out->ad_render_id) ||
      !data.ReadAllowedReportingOrigins(&out->allowed_reporting_origins)) {
    return false;
  }
  return true;
}

bool StructTraits<blink::mojom::SellerCapabilitiesDataView,
                  blink::SellerCapabilitiesType>::
    Read(blink::mojom::SellerCapabilitiesDataView data,
         blink::SellerCapabilitiesType* out) {
  if (data.allows_interest_group_counts()) {
    out->Put(blink::SellerCapabilities::kInterestGroupCounts);
  }
  if (data.allows_latency_stats()) {
    out->Put(blink::SellerCapabilities::kLatencyStats);
  }
  return true;
}

bool StructTraits<blink::mojom::AuctionServerRequestFlagsDataView,
                  blink::AuctionServerRequestFlags>::
    Read(blink::mojom::AuctionServerRequestFlagsDataView data,
         blink::AuctionServerRequestFlags* out) {
  if (data.omit_ads()) {
    out->Put(blink::AuctionServerRequestFlagsEnum::kOmitAds);
  }
  if (data.include_full_ads()) {
    out->Put(blink::AuctionServerRequestFlagsEnum::kIncludeFullAds);
  }
  if (data.omit_user_bidding_signals()) {
    out->Put(blink::AuctionServerRequestFlagsEnum::kOmitUserBiddingSignals);
  }
  return true;
}

bool StructTraits<blink::mojom::InterestGroupDataView, blink::InterestGroup>::
    Read(blink::mojom::InterestGroupDataView data, blink::InterestGroup* out) {
  out->priority = data.priority();
  out->enable_bidding_signals_prioritization =
      data.enable_bidding_signals_prioritization();
  out->execution_mode = data.execution_mode();
  out->trusted_bidding_signals_slot_size_mode =
      data.trusted_bidding_signals_slot_size_mode();
  out->max_trusted_bidding_signals_url_length =
      data.max_trusted_bidding_signals_url_length();
  if (!data.ReadExpiry(&out->expiry) || !data.ReadOwner(&out->owner) ||
      !data.ReadName(&out->name) ||
      !data.ReadPriorityVector(&out->priority_vector) ||
      !data.ReadPrioritySignalsOverrides(&out->priority_signals_overrides) ||
      !data.ReadSellerCapabilities(&out->seller_capabilities) ||
      !data.ReadAllSellersCapabilities(&out->all_sellers_capabilities) ||
      !data.ReadBiddingUrl(&out->bidding_url) ||
      !data.ReadBiddingWasmHelperUrl(&out->bidding_wasm_helper_url) ||
      !data.ReadUpdateUrl(&out->update_url) ||
      !data.ReadTrustedBiddingSignalsUrl(&out->trusted_bidding_signals_url) ||
      !data.ReadTrustedBiddingSignalsKeys(&out->trusted_bidding_signals_keys) ||
      !data.ReadTrustedBiddingSignalsCoordinator(
          &out->trusted_bidding_signals_coordinator) ||
      !data.ReadUserBiddingSignals(&out->user_bidding_signals) ||
      !data.ReadAds(&out->ads) || !data.ReadAdComponents(&out->ad_components) ||
      !data.ReadAdSizes(&out->ad_sizes) ||
      !data.ReadSizeGroups(&out->size_groups) ||
      !data.ReadAuctionServerRequestFlags(&out->auction_server_request_flags) ||
      !data.ReadAdditionalBidKey(&out->additional_bid_key) ||
      !data.ReadAggregationCoordinatorOrigin(
          &out->aggregation_coordinator_origin)) {
    return false;
  }
  return out->IsValid();
}

}  // namespace mojo

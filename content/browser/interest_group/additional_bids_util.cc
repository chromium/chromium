// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/additional_bids_util.h"

#include <memory>
#include <string>

#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "base/values.h"
#include "content/browser/interest_group/interest_group_auction.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "url/origin.h"

namespace content {

AdditionalBidDecodeResult::AdditionalBidDecodeResult() = default;
AdditionalBidDecodeResult::AdditionalBidDecodeResult(
    AdditionalBidDecodeResult&& other) = default;
AdditionalBidDecodeResult::~AdditionalBidDecodeResult() = default;

AdditionalBidDecodeResult& AdditionalBidDecodeResult::operator=(
    AdditionalBidDecodeResult&&) = default;

base::expected<AdditionalBidDecodeResult, std::string> DecodeAdditionalBid(
    InterestGroupAuction* auction,
    const base::Value& bid_in,
    const base::Uuid& auction_nonce,
    const url::Origin& seller,
    base::optional_ref<const url::Origin> top_level_seller) {
  const base::Value::Dict* result_dict = bid_in.GetIfDict();
  if (!result_dict) {
    return base::unexpected(
        base::StrCat({"Additional bid on auction with seller '",
                      seller.Serialize(), "' is not a dictionary."}));
  }

  const std::string* nonce = result_dict->FindString("auctionNonce");
  if (!nonce || *nonce != auction_nonce.AsLowercaseString()) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to missing or incorrect nonce."}));
  }

  const std::string* bid_seller = result_dict->FindString("seller");
  if (!bid_seller || url::Origin::Create(GURL(*bid_seller)) != seller) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to missing or incorrect seller."}));
  }

  const std::string* bid_top_level_seller =
      result_dict->FindString("topLevelSeller");
  if (top_level_seller.has_value()) {
    // Component auction.
    if (!bid_top_level_seller ||
        url::Origin::Create(GURL(*bid_top_level_seller)) != *top_level_seller) {
      return base::unexpected(base::StrCat(
          {"Additional bid on auction with seller '", seller.Serialize(),
           "' rejected due to missing or incorrect topLevelSeller."}));
    }
  } else {
    // Top-level or single-level auction.
    if (bid_top_level_seller) {
      return base::unexpected(base::StrCat(
          {"Additional bid on auction with seller '", seller.Serialize(),
           "' rejected due to specifying topLevelSeller in a non-component "
           "auction."}));
    }
  }

  const std::string* ig_name =
      result_dict->FindStringByDottedPath("interestGroup.name");
  const std::string* ig_bidding_url_str =
      result_dict->FindStringByDottedPath("interestGroup.biddingLogicURL");
  const std::string* ig_owner_string =
      result_dict->FindStringByDottedPath("interestGroup.owner");

  GURL ig_bidding_url;
  if (ig_bidding_url_str) {
    ig_bidding_url = GURL(*ig_bidding_url_str);
  }

  absl::optional<url::Origin> ig_owner;
  if (ig_owner_string) {
    GURL ig_owner_url(*ig_owner_string);
    if (ig_owner_url.is_valid() && ig_owner_url.SchemeIs("https")) {
      ig_owner = url::Origin::Create(ig_owner_url);
    }
  }

  if (!ig_name || !ig_bidding_url.is_valid() || !ig_owner.has_value()) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to missing or invalid interest group info."}));
  }

  if (!ig_owner->IsSameOriginWith(ig_bidding_url)) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to invalid origin of biddingLogicURL."}));
  }

  auto synth_interest_group = std::make_unique<StorageInterestGroup>();
  synth_interest_group->interest_group.owner =
      url::Origin::Create(ig_bidding_url);
  synth_interest_group->interest_group.name = *ig_name;
  synth_interest_group->interest_group.owner = std::move(ig_owner).value();
  synth_interest_group->interest_group.bidding_url = std::move(ig_bidding_url);

  // Add ads.
  const base::Value::Dict* bid_dict = result_dict->FindDict("bid");
  if (!bid_dict) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to missing bid info."}));
  }

  const std::string* render_url_str = bid_dict->FindString("render");
  GURL render_url;
  if (render_url_str) {
    render_url = GURL(*render_url_str);
  }
  if (!render_url.is_valid()) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to missing or invalid creative URL."}));
  }

  // Create ad vector and its first entry.
  synth_interest_group->interest_group.ads.emplace();
  synth_interest_group->interest_group.ads.value().emplace_back();
  synth_interest_group->interest_group.ads.value()[0].render_url = render_url;

  absl::optional<double> bid_val = bid_dict->FindDouble("bid");
  if (!bid_val || bid_val.value() <= 0) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to missing or invalid bid value."}));
  }

  std::string ad_metadata = "null";
  const base::Value* ad_metadata_val = bid_dict->Find("ad");
  if (ad_metadata_val) {
    absl::optional<std::string> serialized_metadata =
        base::WriteJson(*ad_metadata_val);
    if (serialized_metadata) {
      ad_metadata = std::move(serialized_metadata).value();
    }
  }

  absl::optional<blink::AdCurrency> bid_currency;
  const base::Value* bid_currency_val = bid_dict->Find("bidCurrency");
  if (bid_currency_val) {
    const std::string* bid_currency_str = bid_currency_val->GetIfString();
    if (!bid_currency_str || !blink::IsValidAdCurrencyCode(*bid_currency_str)) {
      return base::unexpected(base::StrCat(
          {"Additional bid on auction with seller '", seller.Serialize(),
           "' rejected due to invalid bidCurrency."}));
    } else {
      bid_currency = blink::AdCurrency::From(*bid_currency_str);
    }
  }

  absl::optional<double> ad_cost;
  const base::Value* ad_cost_val = bid_dict->Find("adCost");
  if (ad_cost_val) {
    ad_cost = ad_cost_val->GetIfDouble();
    if (!ad_cost.has_value()) {
      return base::unexpected(base::StrCat(
          {"Additional bid on auction with seller '", seller.Serialize(),
           "' rejected due to invalid adCost."}));
    }
  }

  // modelingSignals in generateBid() ignores out-of-range values, so this
  // matches the behavior.
  absl::optional<double> modeling_signals;
  const base::Value* modeling_signals_val = bid_dict->Find("modelingSignals");
  if (modeling_signals_val) {
    absl::optional<double> modeling_signals_in =
        modeling_signals_val->GetIfDouble();
    if (!modeling_signals_in.has_value()) {
      return base::unexpected(base::StrCat(
          {"Additional bid on auction with seller '", seller.Serialize(),
           "' rejected due to non-numeric modelingSignals."}));
    }
    if (*modeling_signals_in >= 0 && *modeling_signals_in < 4096) {
      modeling_signals = modeling_signals_in;
    }
  }

  std::vector<blink::AdDescriptor> ad_components;
  const base::Value* ad_components_val = bid_dict->Find("adComponents");
  if (ad_components_val) {
    const base::Value::List* ad_components_list =
        ad_components_val->GetIfList();
    if (!ad_components_list) {
      return base::unexpected(base::StrCat(
          {"Additional bid on auction with seller '", seller.Serialize(),
           "' rejected due to invalid adComponents."}));
    }
    synth_interest_group->interest_group.ad_components.emplace();
    for (const base::Value& ad_component : *ad_components_list) {
      const std::string* ad_component_str = ad_component.GetIfString();
      GURL ad_component_url;
      if (ad_component_str) {
        ad_component_url = GURL(*ad_component_str);
      }
      if (!ad_component_url.is_valid()) {
        return base::unexpected(base::StrCat(
            {"Additional bid on auction with seller '", seller.Serialize(),
             "' rejected due to invalid entry in adComponents."}));
      }
      ad_components.emplace_back(ad_component_url);
      // TODO(http://crbug.com/1464874): What's the story with dimensions?
      synth_interest_group->interest_group.ad_components->emplace_back(
          std::move(ad_component_url), /*metadata=*/absl::nullopt);
    }
  }

  AdditionalBidDecodeResult result;
  result.bid_state = std::make_unique<InterestGroupAuction::BidState>();
  result.bid_state->additional_bid_buyer =
      synth_interest_group->interest_group.owner;
  result.bid_state->bidder = std::move(synth_interest_group);
  result.bid_state->made_bid = true;
  result.bid_state->BeginTracing();

  const blink::InterestGroup::Ad* bid_ad =
      &result.bid_state->bidder->interest_group.ads.value()[0];
  result.bid = std::make_unique<InterestGroupAuction::Bid>(
      InterestGroupAuction::Bid::BidRole::kBothKAnonModes, ad_metadata,
      *bid_val,
      /*bid_currency=*/bid_currency,
      /*ad_cost=*/ad_cost,
      /*ad_descriptor=*/blink::AdDescriptor(bid_ad->render_url),
      /*ad_component_descriptors=*/std::move(ad_components),
      /*modeling_signals=*/
      static_cast<absl::optional<uint16_t>>(modeling_signals),
      /*bid_duration=*/base::TimeDelta(),
      /*bidding_signals_data_version=*/absl::nullopt, bid_ad,
      result.bid_state.get(), auction);

  // TODO(http://crbug.com/1464874): Do we need to fill in any k-anon info?
  // TODO(http://crbug.com/1464874): Parse the actual negative targeting info.

  return result;
}

}  // namespace content

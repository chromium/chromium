// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/additional_bids_util.h"

#include <stdint.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "base/values.h"
#include "content/browser/interest_group/auction_metrics_recorder.h"
#include "content/browser/interest_group/interest_group_auction.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "third_party/blink/public/common/interest_group/ad_auction_constants.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "url/origin.h"

namespace content {

namespace {

// Returns error string on failure.
template <size_t N>
std::optional<std::string> DecodeBase64Fixed(std::string_view field,
                                             const std::string& in,
                                             std::array<uint8_t, N>& out) {
  std::string decoded;
  if (!base::Base64Decode(in, &decoded, base::Base64DecodePolicy::kForgiving)) {
    return base::StrCat({"Field '", field, "' is not valid base64."});
  }
  if (decoded.size() != N) {
    return base::StrCat({"Field '", field, "' has unexpected length."});
  }
  std::copy(decoded.begin(), decoded.end(), out.data());

  return std::nullopt;
}

bool AdditionalBidKeyHasMatchingValidSignature(
    const std::vector<SignedAdditionalBidSignature>& signatures,
    const std::vector<size_t>& valid_signatures,
    const blink::InterestGroup::AdditionalBidKey& key) {
  for (size_t i : valid_signatures) {
    CHECK_LT(i, signatures.size());
    if (signatures[i].key == key) {
      return true;
    }
  }
  return false;
}

}  // namespace

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
    const base::flat_set<url::Origin>& interest_group_buyers,
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
  if (!ig_name) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to missing interest group name."}));
  }

  const std::string* ig_bidding_url_str =
      result_dict->FindStringByDottedPath("interestGroup.biddingLogicURL");
  if (!ig_bidding_url_str) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to missing interest group bidding URL."}));
  }

  GURL ig_bidding_url = GURL(*ig_bidding_url_str);
  if (!ig_bidding_url.is_valid()) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to invalid interest group bidding URL."}));
  }

  const std::string* ig_owner_string =
      result_dict->FindStringByDottedPath("interestGroup.owner");
  if (!ig_owner_string) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to missing interest group owner."}));
  }
  GURL ig_owner_url(*ig_owner_string);
  if (!ig_owner_url.is_valid()) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to invalid interest group owner URL."}));
  }
  if (!ig_owner_url.SchemeIs("https")) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to non-https interest group owner URL."}));
  }
  url::Origin ig_owner = url::Origin::Create(ig_owner_url);

  if (interest_group_buyers.find(ig_owner) == interest_group_buyers.end()) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected because the additional bid's owner, '",
         ig_owner.Serialize(), "', is not in interestGroupBuyers."}));
  }

  if (!ig_owner.IsSameOriginWith(ig_bidding_url)) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to invalid origin of biddingLogicURL."}));
  }

  auto synth_interest_group = StorageInterestGroup();
  synth_interest_group.interest_group.name = *ig_name;
  synth_interest_group.interest_group.owner = std::move(ig_owner);
  synth_interest_group.interest_group.bidding_url = std::move(ig_bidding_url);
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
  synth_interest_group.interest_group.ads.emplace();
  synth_interest_group.interest_group.ads->emplace_back(
      render_url, /*metadata=*/std::nullopt);

  std::optional<double> bid_val = bid_dict->FindDouble("bid");
  if (!bid_val || bid_val.value() <= 0) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to missing or invalid bid value."}));
  }

  std::string ad_metadata = "null";
  const base::Value* ad_metadata_val = bid_dict->Find("ad");
  if (ad_metadata_val) {
    std::optional<std::string> serialized_metadata =
        base::WriteJson(*ad_metadata_val);
    if (serialized_metadata) {
      ad_metadata = std::move(serialized_metadata).value();
    }
  }

  std::optional<blink::AdCurrency> bid_currency;
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

  std::optional<double> ad_cost;
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
  std::optional<double> modeling_signals;
  const base::Value* modeling_signals_val = bid_dict->Find("modelingSignals");
  if (modeling_signals_val) {
    std::optional<double> modeling_signals_in =
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
    if (ad_components_list->size() > blink::MaxAdAuctionAdComponents()) {
      return base::unexpected(base::StrCat(
          {"Additional bid on auction with seller '", seller.Serialize(),
           "' rejected due to too many ad component URLs."}));
    }

    synth_interest_group.interest_group.ad_components.emplace();
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
      synth_interest_group.interest_group.ad_components->emplace_back(
          std::move(ad_component_url), /*metadata=*/std::nullopt);
    }
  }

  AdditionalBidDecodeResult result;

  const base::Value* single_negative_ig =
      result_dict->Find("negativeInterestGroup");
  const base::Value* multiple_negative_ig =
      result_dict->Find("negativeInterestGroups");

  if (single_negative_ig && multiple_negative_ig) {
    return base::unexpected(base::StrCat(
        {"Additional bid on auction with seller '", seller.Serialize(),
         "' rejected due to specifying both 'negativeInterestGroup' and "
         "'negativeInterestGroups'."}));
  }

  if (single_negative_ig) {
    const std::string* negative_ig_name = single_negative_ig->GetIfString();
    if (!negative_ig_name) {
      return base::unexpected(base::StrCat(
          {"Additional bid on auction with seller '", seller.Serialize(),
           "' rejected due to non-string 'negativeInterestGroup'."}));
    }
    result.negative_target_interest_group_names.push_back(*negative_ig_name);
  }

  if (multiple_negative_ig) {
    const base::Value::Dict* multiple_negative_ig_dict =
        multiple_negative_ig->GetIfDict();
    if (!multiple_negative_ig_dict) {
      return base::unexpected(base::StrCat(
          {"Additional bid on auction with seller '", seller.Serialize(),
           "' rejected due to non-dictionary 'negativeInterestGroups'."}));
    }
    const std::string* joining_origin_str =
        multiple_negative_ig_dict->FindString("joiningOrigin");
    GURL joining_origin_url;
    if (joining_origin_str) {
      joining_origin_url = GURL(*joining_origin_str);
    }
    if (!joining_origin_url.is_valid() ||
        !joining_origin_url.SchemeIs("https")) {
      return base::unexpected(base::StrCat(
          {"Additional bid on auction with seller '", seller.Serialize(),
           "' rejected due to invalid or missing 'joiningOrigin'."}));
    }
    result.negative_target_joining_origin =
        url::Origin::Create(joining_origin_url);

    const base::Value::List* interest_group_names =
        multiple_negative_ig_dict->FindList("interestGroupNames");
    if (!interest_group_names) {
      return base::unexpected(base::StrCat(
          {"Additional bid on auction with seller '", seller.Serialize(),
           "' rejected due to missing or invalid 'interestGroupNames' within "
           "'negativeInterestGroups'."}));
    }
    for (const base::Value& negative_ig : *interest_group_names) {
      const std::string* negative_ig_str = negative_ig.GetIfString();
      if (!negative_ig_str) {
        return base::unexpected(base::StrCat(
            {"Additional bid on auction with seller '", seller.Serialize(),
             "' rejected due to non-string 'interestGroupNames' entry."}));
      }
      result.negative_target_interest_group_names.push_back(*negative_ig_str);
    }
  }
  SingleStorageInterestGroup storage_interest_group(
      std::move(synth_interest_group));
  result.bid_state = std::make_unique<InterestGroupAuction::BidState>(
      std::move(storage_interest_group));
  result.bid_state->additional_bid_buyer =
      result.bid_state->bidder->interest_group.owner;
  result.bid_state->made_bid = true;
  result.bid_state->BeginTracing();

  const blink::InterestGroup::Ad* bid_ad =
      &result.bid_state->bidder->interest_group.ads.value()[0];
  result.bid = std::make_unique<InterestGroupAuction::Bid>(
      auction_worklet::mojom::BidRole::kBothKAnonModes, ad_metadata, *bid_val,
      /*bid_currency=*/bid_currency,
      /*ad_cost=*/ad_cost,
      /*ad_descriptor=*/blink::AdDescriptor(GURL(bid_ad->render_url())),
      /*ad_component_descriptors=*/std::move(ad_components),
      /*modeling_signals=*/
      static_cast<std::optional<uint16_t>>(modeling_signals),
      /*bid_duration=*/base::TimeDelta(),
      /*bidding_signals_data_version=*/std::nullopt, bid_ad,
      /*selected_buyer_and_seller_reporting_id=*/std::nullopt,
      result.bid_state.get(), auction);

  // TODO(http://crbug.com/1464874): Do we need to fill in any k-anon info?

  return result;
}

SignedAdditionalBid::SignedAdditionalBid() = default;
SignedAdditionalBid::SignedAdditionalBid(SignedAdditionalBid&& other) = default;
SignedAdditionalBid::~SignedAdditionalBid() = default;

SignedAdditionalBid& SignedAdditionalBid::operator=(SignedAdditionalBid&&) =
    default;

std::vector<size_t> SignedAdditionalBid::VerifySignatures() {
  std::vector<size_t> verified;
  for (size_t i = 0; i < signatures.size(); ++i) {
    if (ED25519_verify(
            reinterpret_cast<const uint8_t*>(additional_bid_json.data()),
            additional_bid_json.size(), signatures[i].signature.data(),
            signatures[i].key.data())) {
      verified.push_back(i);
    }
  }
  return verified;
}

base::expected<SignedAdditionalBid, std::string> DecodeSignedAdditionalBid(
    base::Value signed_additional_bid_in) {
  base::Value::Dict* in_dict = signed_additional_bid_in.GetIfDict();
  if (!in_dict) {
    return base::unexpected("Signed additional bid not a dictionary.");
  }

  SignedAdditionalBid result;

  std::string* bid_json = in_dict->FindString("bid");
  if (!bid_json) {
    return base::unexpected(
        "Signed additional bid missing string 'bid' field.");
  }
  result.additional_bid_json = std::move(*bid_json);

  const base::Value::List* signature_list = in_dict->FindList("signatures");
  if (!signature_list) {
    return base::unexpected(
        "Signed additional bid missing list 'signatures' field.");
  }

  for (const base::Value& sig_entry : *signature_list) {
    SignedAdditionalBidSignature decoded_signature;

    const base::Value::Dict* sig_entry_dict = sig_entry.GetIfDict();
    if (!sig_entry_dict) {
      return base::unexpected(
          "Signed additional bid 'signatures' list entry not a dictionary.");
    }
    const std::string* key = sig_entry_dict->FindString("key");
    if (!key) {
      return base::unexpected(
          "Signed additional bid 'signatures' list entry missing 'key' "
          "string.");
    }

    std::optional<std::string> maybe_key_error =
        DecodeBase64Fixed("key", *key, decoded_signature.key);
    if (maybe_key_error.has_value()) {
      return base::unexpected(maybe_key_error.value());
    }

    const std::string* signature = sig_entry_dict->FindString("signature");
    if (!signature) {
      return base::unexpected(
          "Signed additional bid 'signatures' list entry missing 'signature' "
          "string.");
    }

    std::optional<std::string> maybe_signature_error =
        DecodeBase64Fixed("signature", *signature, decoded_signature.signature);
    if (maybe_signature_error.has_value()) {
      return base::unexpected(maybe_signature_error.value());
    }
    result.signatures.push_back(std::move(decoded_signature));
  }

  return result;
}

AdAuctionNegativeTargeter::AdAuctionNegativeTargeter() = default;
AdAuctionNegativeTargeter::~AdAuctionNegativeTargeter() = default;

void AdAuctionNegativeTargeter::AddInterestGroupInfo(
    const url::Origin& buyer,
    const std::string& name,
    const url::Origin& joining_origin,
    const blink::InterestGroup::AdditionalBidKey& key) {
  // Should not have any duplicates since (buyer, name) ought to be the DB
  // primary key.
  DCHECK(!negative_interest_groups_.contains(std::make_pair(buyer, name)));
  auto& spot = negative_interest_groups_[std::make_pair(buyer, name)];
  spot.joining_origin = joining_origin;
  spot.key = key;
}

size_t AdAuctionNegativeTargeter::GetNumNegativeInterestGroups() {
  return negative_interest_groups_.size();
}

bool AdAuctionNegativeTargeter::ShouldDropDueToNegativeTargeting(
    const url::Origin& buyer,
    const std::optional<url::Origin>& negative_target_joining_origin,
    const std::vector<std::string>& negative_target_interest_group_names,
    const std::vector<SignedAdditionalBidSignature>& signatures,
    const std::vector<size_t>& valid_signatures,
    const url::Origin& seller,
    AuctionMetricsRecorder& auction_metrics_recorder,
    std::vector<std::string>& errors_out) {
  if (valid_signatures.size() != signatures.size()) {
    errors_out.push_back(
        base::StrCat({"Warning: Some signatures on an additional bid from '",
                      buyer.Serialize(), "' on auction with seller '",
                      seller.Serialize(), "' failed to verify."}));
  }

  for (const std::string& ig_name : negative_target_interest_group_names) {
    auto negative_info_it =
        negative_interest_groups_.find(std::make_pair(buyer, ig_name));

    // Negative group not there, no reason to reject thus far.
    if (negative_info_it == negative_interest_groups_.end()) {
      continue;
    }

    const NegativeInfo& negative_info = negative_info_it->second;

    // Negative group there, but we may need to ignore it if it doesn't have
    // a matching signature.
    if (!AdditionalBidKeyHasMatchingValidSignature(signatures, valid_signatures,
                                                   negative_info.key)) {
      auction_metrics_recorder
          .RecordNegativeInterestGroupIgnoredDueToInvalidSignature();
      errors_out.push_back(base::StrCat(
          {"Warning: Ignoring negative targeting group '", ig_name,
           "' on an additional bid from '", buyer.Serialize(),
           "' on auction with seller '", seller.Serialize(),
           "' since its key does not correspond to a valid signature."}));
      continue;
    }

    // Must also have proper joining origin, if applicable.
    if (negative_target_joining_origin.has_value()) {
      if (*negative_target_joining_origin != negative_info.joining_origin) {
        auction_metrics_recorder
            .RecordNegativeInterestGroupIgnoredDueToJoiningOriginMismatch();
        errors_out.push_back(base::StrCat(
            {"Warning: Ignoring negative targeting group '", ig_name,
             "' on an additional bid from '", buyer.Serialize(),
             "' on auction with seller '", seller.Serialize(),
             "' since it does not have the expected joining origin."}));
        continue;
      }
    }

    // Found a negative group that meets all requirements.
    return true;
  }

  // No validated negative groups found.
  return false;
}

AdAuctionNegativeTargeter::NegativeInfo::NegativeInfo() = default;
AdAuctionNegativeTargeter::NegativeInfo::~NegativeInfo() = default;

}  // namespace content

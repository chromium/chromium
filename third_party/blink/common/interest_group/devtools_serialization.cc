// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/devtools_serialization.h"

#include <cmath>
#include <string_view>
#include <tuple>

#include "base/base64.h"
#include "base/strings/to_string.h"
#include "base/values.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"
#include "third_party/blink/public/common/interest_group/auction_config.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/ad_auction_service.mojom.h"

namespace blink {

namespace {

std::string SerializeIntoKey(const url::Origin& in) {
  return in.Serialize();
}

std::string SerializeIntoKey(const std::string& in) {
  return in;
}

using BuyerReportType = AuctionConfig::NonSharedParams::BuyerReportType;
std::string SerializeIntoKey(BuyerReportType report_type) {
  switch (report_type) {
    case BuyerReportType::kInterestGroupCount:
      return "interestGroupCount";
    case BuyerReportType::kBidCount:
      return "bidCount";
    case BuyerReportType::kTotalGenerateBidLatency:
      return "totalGenerateBidLatency";
    case BuyerReportType::kTotalSignalsFetchLatency:
      return "totalSignalsFetchLatency";
  };
  NOTREACHED();
}

using RealTimeReportingType =
    AuctionConfig::NonSharedParams::RealTimeReportingType;
std::string SerializeIntoValue(RealTimeReportingType report_type) {
  switch (report_type) {
    case RealTimeReportingType::kDefaultLocalReporting:
      return "default-local-reporting";
  };
  NOTREACHED();
}

template <typename T>
base::Value SerializeIntoValue(const T& in) {
  return base::Value(in);
}

// Forward declare for mutual recursion.
template <typename K, typename V>
base::Value SerializeIntoValue(const base::flat_map<K, V>&);

template <typename T>
void SerializeIntoDict(std::string_view field,
                       const T& value,
                       base::Value::Dict& out);

template <typename T>
void SerializeIntoDict(std::string_view field,
                       const std::optional<T>& value,
                       base::Value::Dict& out);

template <typename T>
base::Value SerializeIntoValue(const std::optional<T>& value) {
  if (value.has_value()) {
    return SerializeIntoValue(*value);
  } else {
    return base::Value();
  }
}

template <>
base::Value SerializeIntoValue(const url::Origin& value) {
  return base::Value(value.Serialize());
}

template <>
base::Value SerializeIntoValue(const GURL& value) {
  return base::Value(value.spec());
}

template <>
base::Value SerializeIntoValue(const base::TimeDelta& value) {
  return base::Value(value.InMillisecondsF());
}

template <>
base::Value SerializeIntoValue(const absl::uint128& value) {
  return base::Value(base::ToString(value));
}

template <>
base::Value SerializeIntoValue(
    const blink::AuctionConfig::AdKeywordReplacement& value) {
  base::Value::Dict result;
  result.Set("match", SerializeIntoValue(value.match));
  result.Set("replacement", SerializeIntoValue(value.replacement));
  return base::Value(std::move(result));
}

template <>
base::Value SerializeIntoValue(const double& value) {
  // base::Value, like JSON, can only store finite numbers, so we encode the
  // rest as strings.
  if (std::isfinite(value)) {
    return base::Value(value);
  }
  if (std::isnan(value)) {
    return base::Value("NaN");
  }
  if (std::signbit(value)) {
    return base::Value("-Infinity");
  } else {
    return base::Value("Infinity");
  }
}

template <>
base::Value SerializeIntoValue(
    const AuctionConfig::NonSharedParams::AuctionReportBuyersConfig& value) {
  base::Value::Dict result;
  result.Set("bucket", SerializeIntoValue(value.bucket));
  result.Set("scale", SerializeIntoValue(value.scale));
  return base::Value(std::move(result));
}

template <>
base::Value SerializeIntoValue(const SellerCapabilitiesType& value) {
  base::Value::List result;
  for (blink::SellerCapabilities cap : value) {
    switch (cap) {
      case blink::SellerCapabilities::kInterestGroupCounts:
        result.Append("interest-group-counts");
        break;
      case blink::SellerCapabilities::kLatencyStats:
        result.Append("latency-stats");
        break;
    }
  }
  return base::Value(std::move(result));
}

template <>
base::Value SerializeIntoValue(const base::Uuid& uuid) {
  return base::Value(uuid.AsLowercaseString());
}

template <>
base::Value SerializeIntoValue(
    const blink::AuctionConfig::ServerResponseConfig& server_config) {
  base::Value::Dict result;
  result.Set("requestId", SerializeIntoValue(server_config.request_id));
  return base::Value(std::move(result));
}

template <typename T>
base::Value SerializeIntoValue(const std::vector<T>& values) {
  base::Value::List out;
  for (const T& in_val : values) {
    out.Append(SerializeIntoValue(in_val));
  }
  return base::Value(std::move(out));
}

template <typename T>
base::Value SerializeIntoValue(const AuctionConfig::MaybePromise<T>& promise) {
  base::Value::Dict result;
  result.Set("pending", promise.is_promise());
  if (!promise.is_promise()) {
    result.Set("value", SerializeIntoValue(promise.value()));
  }
  return base::Value(std::move(result));
}

template <>
base::Value SerializeIntoValue(
    const AuctionConfig::NonSharedParams::AuctionReportBuyerDebugModeConfig&
        value) {
  base::Value::Dict result;
  result.Set("enabled", value.is_enabled);
  if (value.debug_key.has_value()) {
    // debug_key is uint64, so it doesn't fit into regular JS numeric types.
    result.Set("debugKey", base::ToString(value.debug_key.value()));
  }
  return base::Value(std::move(result));
}

template <typename K, typename V>
base::Value SerializeIntoValue(const base::flat_map<K, V>& value) {
  base::Value::Dict result;
  for (const auto& kv : value) {
    result.Set(SerializeIntoKey(kv.first), SerializeIntoValue(kv.second));
  }
  return base::Value(std::move(result));
}

// Helper to put the split out all_/'*' value back into a single map.
// Annoyingly we are quite inconsistent about how optional is used. This handles
// both cases for the map; for the value the caller can just use make_optional.
template <typename T>
base::Value::Dict SerializeSplitMapHelper(
    const std::optional<T>& all_value,
    const std::optional<base::flat_map<url::Origin, T>>& per_values) {
  base::Value::Dict result;
  if (all_value.has_value()) {
    result.Set("*", SerializeIntoValue(*all_value));
  }
  if (per_values.has_value()) {
    for (const auto& kv : *per_values) {
      result.Set(SerializeIntoKey(kv.first), SerializeIntoValue(kv.second));
    }
  }

  return result;
}

template <typename T>
base::Value::Dict SerializeSplitMapHelper(
    const std::optional<T>& all_value,
    const base::flat_map<url::Origin, T>& per_values) {
  base::Value::Dict result;
  if (all_value.has_value()) {
    result.Set("*", SerializeIntoValue(*all_value));
  }

  for (const auto& kv : per_values) {
    result.Set(SerializeIntoKey(kv.first), SerializeIntoValue(kv.second));
  }

  return result;
}

template <>
base::Value SerializeIntoValue(const AuctionConfig::BuyerTimeouts& in) {
  return base::Value(
      SerializeSplitMapHelper(in.all_buyers_timeout, in.per_buyer_timeouts));
}

template <>
base::Value SerializeIntoValue(const AuctionConfig::BuyerCurrencies& in) {
  return base::Value(
      SerializeSplitMapHelper(in.all_buyers_currency, in.per_buyer_currencies));
}

template <>
base::Value SerializeIntoValue(const AdCurrency& in) {
  return base::Value(in.currency_code());
}

template <>
base::Value SerializeIntoValue(const AdSize& ad_size) {
  base::Value::Dict result;
  result.Set("width",
             ConvertAdDimensionToString(ad_size.width, ad_size.width_units));
  result.Set("height",
             ConvertAdDimensionToString(ad_size.height, ad_size.height_units));
  return base::Value(std::move(result));
}

template <>
base::Value SerializeIntoValue(
    const blink::mojom::InterestGroup::ExecutionMode& in) {
  switch (in) {
    case blink::mojom::InterestGroup::ExecutionMode::kCompatibilityMode:
      return SerializeIntoValue("compatibility");

    case blink::mojom::InterestGroup::ExecutionMode::kGroupedByOriginMode:
      return SerializeIntoValue("group-by-origin");

    case blink::mojom::InterestGroup::ExecutionMode::kFrozenContext:
      return SerializeIntoValue("frozen-context");
  }
}

template <>
base::Value SerializeIntoValue(const InterestGroup::AdditionalBidKey& in) {
  return base::Value(base::Base64Encode(in));
}

template <>
base::Value SerializeIntoValue(const InterestGroup::Ad& ad) {
  base::Value::Dict result;
  SerializeIntoDict("renderURL", ad.render_url(), result);
  SerializeIntoDict("metadata", ad.metadata, result);
  SerializeIntoDict("buyerReportingId", ad.buyer_reporting_id, result);
  SerializeIntoDict("buyerAndSellerReportingId",
                    ad.buyer_and_seller_reporting_id, result);
  SerializeIntoDict("selectableBuyerAndSellerReportingIds",
                    ad.selectable_buyer_and_seller_reporting_ids, result);
  SerializeIntoDict("adRenderId", ad.ad_render_id, result);
  SerializeIntoDict("allowedReportingOrigins", ad.allowed_reporting_origins,
                    result);
  return base::Value(std::move(result));
}

template <>
base::Value SerializeIntoValue(const AuctionServerRequestFlags& flags) {
  base::Value::List result;
  for (auto flag : flags) {
    switch (flag) {
      case AuctionServerRequestFlagsEnum::kOmitAds:
        result.Append("omit-ads");
        break;

      case AuctionServerRequestFlagsEnum::kIncludeFullAds:
        result.Append("include-full-ads");
        break;
      case AuctionServerRequestFlagsEnum::kOmitUserBiddingSignals:
        result.Append("omit-user-bidding-signals");
        break;
    }
  }
  return base::Value(std::move(result));
}

template <typename T>
void SerializeIntoDict(std::string_view field,
                       const T& value,
                       base::Value::Dict& out) {
  out.Set(field, SerializeIntoValue(value));
}

// Unlike the value serializer this just makes the field not set, rather than
// explicitly null.
template <typename T>
void SerializeIntoDict(std::string_view field,
                       const std::optional<T>& value,
                       base::Value::Dict& out) {
  if (value.has_value()) {
    out.Set(field, SerializeIntoValue(*value));
  }
}

// For use with SerializeSplitMapHelper.
void SerializeIntoDict(std::string_view field,
                       base::Value::Dict value,
                       base::Value::Dict& out) {
  out.Set(field, std::move(value));
}

}  // namespace

base::Value::Dict SerializeAuctionConfigForDevtools(const AuctionConfig& conf) {
  base::Value::Dict result;
  SerializeIntoDict("seller", conf.seller, result);
  SerializeIntoDict("serverResponse", conf.server_response, result);
  SerializeIntoDict("decisionLogicURL", conf.decision_logic_url, result);
  SerializeIntoDict("trustedScoringSignalsURL",
                    conf.trusted_scoring_signals_url, result);
  SerializeIntoDict("deprecatedRenderURLReplacements",
                    conf.non_shared_params.deprecated_render_url_replacements,
                    result);
  SerializeIntoDict("interestGroupBuyers",
                    conf.non_shared_params.interest_group_buyers, result);
  SerializeIntoDict("auctionSignals", conf.non_shared_params.auction_signals,
                    result);
  SerializeIntoDict("sellerSignals", conf.non_shared_params.seller_signals,
                    result);
  SerializeIntoDict("sellerTimeout", conf.non_shared_params.seller_timeout,
                    result);
  SerializeIntoDict("perBuyerSignals", conf.non_shared_params.per_buyer_signals,
                    result);
  SerializeIntoDict("perBuyerTimeouts", conf.non_shared_params.buyer_timeouts,
                    result);
  SerializeIntoDict("perBuyerCumulativeTimeouts",
                    conf.non_shared_params.buyer_cumulative_timeouts, result);
  SerializeIntoDict("reportingTimeout",
                    conf.non_shared_params.reporting_timeout, result);
  SerializeIntoDict("sellerCurrency", conf.non_shared_params.seller_currency,
                    result);
  SerializeIntoDict("perBuyerCurrencies",
                    conf.non_shared_params.buyer_currencies, result);
  SerializeIntoDict(
      "perBuyerGroupLimits",
      SerializeSplitMapHelper(
          std::make_optional(conf.non_shared_params.all_buyers_group_limit),
          conf.non_shared_params.per_buyer_group_limits),
      result);
  SerializeIntoDict("perBuyerPrioritySignals",
                    SerializeSplitMapHelper(
                        conf.non_shared_params.all_buyers_priority_signals,
                        conf.non_shared_params.per_buyer_priority_signals),
                    result);
  SerializeIntoDict("auctionReportBuyerKeys",
                    conf.non_shared_params.auction_report_buyer_keys, result);
  SerializeIntoDict("auctionReportBuyers",
                    conf.non_shared_params.auction_report_buyers, result);
  SerializeIntoDict("requiredSellerCapabilities",
                    conf.non_shared_params.required_seller_capabilities,
                    result);
  SerializeIntoDict(
      "auctionReportBuyerDebugModeConfig",
      conf.non_shared_params.auction_report_buyer_debug_mode_config, result);
  SerializeIntoDict("requestedSize", conf.non_shared_params.requested_size,
                    result);
  SerializeIntoDict("allSlotsRequestedSizes",
                    conf.non_shared_params.all_slots_requested_sizes, result);
  SerializeIntoDict(
      "perBuyerMultiBidLimit",
      SerializeSplitMapHelper(
          std::make_optional(conf.non_shared_params.all_buyers_multi_bid_limit),
          conf.non_shared_params.per_buyer_multi_bid_limits),
      result);
  SerializeIntoDict("auctionNonce", conf.non_shared_params.auction_nonce,
                    result);
  SerializeIntoDict("sellerRealTimeReportingType",
                    conf.non_shared_params.seller_real_time_reporting_type,
                    result);
  SerializeIntoDict("perBuyerRealTimeReportingTypes",
                    conf.non_shared_params.per_buyer_real_time_reporting_types,
                    result);

  // For component auctions, we only serialize the seller names to give a
  // quick overview, since they'll get their own events.
  if (!conf.non_shared_params.component_auctions.empty()) {
    base::Value::List component_auctions;
    for (const auto& child_config : conf.non_shared_params.component_auctions) {
      component_auctions.Append(child_config.seller.Serialize());
    }
    result.Set("componentAuctions", std::move(component_auctions));
  }

  SerializeIntoDict(
      "maxTrustedScoringSignalsURLLength",
      conf.non_shared_params.max_trusted_scoring_signals_url_length, result);

  SerializeIntoDict("trustedScoringSignalsCoordinator",
                    conf.non_shared_params.trusted_scoring_signals_coordinator,
                    result);

  // direct_from_seller_signals --- skipped.
  SerializeIntoDict("expectsDirectFromSellerSignalsHeaderAdSlot",
                    conf.expects_direct_from_seller_signals_header_ad_slot,
                    result);
  SerializeIntoDict("sellerExperimentGroupId", conf.seller_experiment_group_id,
                    result);
  SerializeIntoDict(
      "perBuyerExperimentGroupIds",
      SerializeSplitMapHelper(conf.all_buyer_experiment_group_id,
                              conf.per_buyer_experiment_group_ids),
      result);
  SerializeIntoDict("expectsAdditionalBids", conf.expects_additional_bids,
                    result);
  SerializeIntoDict("aggregationCoordinatorOrigin",
                    conf.aggregation_coordinator_origin, result);

  return result;
}

base::Value::Dict SerializeInterestGroupForDevtools(const InterestGroup& ig) {
  base::Value::Dict result;
  // This used to have its own type in Devtools protocol
  // ("InterestGroupDetails"); the fields that existed there are named to match;
  // otherwise the WebIDL is generally followed.
  SerializeIntoDict("expirationTime", ig.expiry.InSecondsFSinceUnixEpoch(),
                    result);
  SerializeIntoDict("ownerOrigin", ig.owner, result);
  SerializeIntoDict("name", ig.name, result);

  SerializeIntoDict("priority", ig.priority, result);
  SerializeIntoDict("enableBiddingSignalsPrioritization",
                    ig.enable_bidding_signals_prioritization, result);
  SerializeIntoDict("priorityVector", ig.priority_vector, result);
  SerializeIntoDict("prioritySignalsOverrides", ig.priority_signals_overrides,
                    result);

  SerializeIntoDict(
      "sellerCapabilities",
      SerializeSplitMapHelper(std::make_optional(ig.all_sellers_capabilities),
                              ig.seller_capabilities),
      result);
  SerializeIntoDict("executionMode", ig.execution_mode, result);

  SerializeIntoDict("biddingLogicURL", ig.bidding_url, result);
  SerializeIntoDict("biddingWasmHelperURL", ig.bidding_wasm_helper_url, result);
  SerializeIntoDict("updateURL", ig.update_url, result);
  SerializeIntoDict("trustedBiddingSignalsURL", ig.trusted_bidding_signals_url,
                    result);
  SerializeIntoDict("trustedBiddingSignalsKeys",
                    ig.trusted_bidding_signals_keys, result);

  SerializeIntoDict("trustedBiddingSignalsSlotSizeMode",
                    InterestGroup::TrustedBiddingSignalsSlotSizeModeToString(
                        ig.trusted_bidding_signals_slot_size_mode),
                    result);
  SerializeIntoDict("maxTrustedBiddingSignalsURLLength",
                    ig.max_trusted_bidding_signals_url_length, result);
  SerializeIntoDict("trustedBiddingSignalsCoordinator",
                    ig.trusted_bidding_signals_coordinator, result);
  SerializeIntoDict("userBiddingSignals", ig.user_bidding_signals, result);
  SerializeIntoDict("ads", ig.ads, result);
  SerializeIntoDict("adComponents", ig.ad_components, result);
  SerializeIntoDict("adSizes", ig.ad_sizes, result);
  SerializeIntoDict("sizeGroups", ig.size_groups, result);
  SerializeIntoDict("auctionServerRequestFlags",
                    ig.auction_server_request_flags, result);
  SerializeIntoDict("additionalBidKey", ig.additional_bid_key, result);
  SerializeIntoDict("aggregationCoordinatorOrigin",
                    ig.aggregation_coordinator_origin, result);

  return result;
}

}  // namespace blink

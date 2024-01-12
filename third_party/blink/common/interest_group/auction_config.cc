// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/auction_config.h"

#include <cmath>
#include <string_view>
#include <tuple>

#include "base/strings/to_string.h"
#include "third_party/blink/public/common/interest_group/ad_display_size_utils.h"

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
  NOTREACHED_NORETURN();
}

template <typename T>
base::Value SerializeIntoValue(const T& in) {
  return base::Value(in);
}

// Forward declare for mutual recursion.
template <typename K, typename V>
base::Value SerializeIntoValue(const base::flat_map<K, V>&);

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

template <typename K, typename V>
base::Value SerializeIntoValue(const base::flat_map<K, V>& value) {
  base::Value::Dict result;
  for (const auto& kv : value) {
    result.Set(SerializeIntoKey(kv.first), SerializeIntoValue(kv.second));
  }
  return base::Value(std::move(result));
}

// Helpers to put the split out all_/'*' value back into a single map.
// Annoyingly we are quite inconsistent about how optional is used.
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

template <typename T>
base::Value::Dict SerializeSplitMapHelper(
    const T& all_value,
    const base::flat_map<url::Origin, T>& per_values) {
  base::Value::Dict result;
  result.Set("*", SerializeIntoValue(all_value));
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

template <typename T>
void SerializeIntoDict(std::string_view field,
                       const T& value,
                       base::Value::Dict& out) {
  out.Set(field, SerializeIntoValue(value));
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

DirectFromSellerSignalsSubresource::DirectFromSellerSignalsSubresource() =
    default;
DirectFromSellerSignalsSubresource::DirectFromSellerSignalsSubresource(
    const DirectFromSellerSignalsSubresource&) = default;
DirectFromSellerSignalsSubresource::DirectFromSellerSignalsSubresource(
    DirectFromSellerSignalsSubresource&&) = default;
DirectFromSellerSignalsSubresource::~DirectFromSellerSignalsSubresource() =
    default;

DirectFromSellerSignalsSubresource&
DirectFromSellerSignalsSubresource::operator=(
    const DirectFromSellerSignalsSubresource&) = default;
DirectFromSellerSignalsSubresource&
DirectFromSellerSignalsSubresource::operator=(
    DirectFromSellerSignalsSubresource&&) = default;

bool operator==(const DirectFromSellerSignalsSubresource& a,
                const DirectFromSellerSignalsSubresource& b) {
  return std::tie(a.bundle_url, a.token) == std::tie(b.bundle_url, b.token);
}

DirectFromSellerSignals::DirectFromSellerSignals() = default;
DirectFromSellerSignals::DirectFromSellerSignals(
    const DirectFromSellerSignals&) = default;
DirectFromSellerSignals::DirectFromSellerSignals(DirectFromSellerSignals&&) =
    default;
DirectFromSellerSignals::~DirectFromSellerSignals() = default;

DirectFromSellerSignals& DirectFromSellerSignals::operator=(
    const DirectFromSellerSignals&) = default;
DirectFromSellerSignals& DirectFromSellerSignals::operator=(
    DirectFromSellerSignals&&) = default;

AuctionConfig::NonSharedParams::NonSharedParams() = default;
AuctionConfig::NonSharedParams::NonSharedParams(const NonSharedParams&) =
    default;
AuctionConfig::NonSharedParams::NonSharedParams(NonSharedParams&&) = default;
AuctionConfig::NonSharedParams::~NonSharedParams() = default;

AuctionConfig::NonSharedParams& AuctionConfig::NonSharedParams::operator=(
    const NonSharedParams&) = default;
AuctionConfig::NonSharedParams& AuctionConfig::NonSharedParams::operator=(
    NonSharedParams&&) = default;

AuctionConfig::ServerResponseConfig::ServerResponseConfig() = default;
AuctionConfig::ServerResponseConfig::ServerResponseConfig(
    const ServerResponseConfig& other) = default;
AuctionConfig::ServerResponseConfig::ServerResponseConfig(
    ServerResponseConfig&&) = default;
AuctionConfig::ServerResponseConfig::~ServerResponseConfig() = default;

AuctionConfig::ServerResponseConfig&
AuctionConfig::ServerResponseConfig::operator=(
    const ServerResponseConfig& other) = default;

AuctionConfig::ServerResponseConfig&
AuctionConfig::ServerResponseConfig::operator=(ServerResponseConfig&&) =
    default;

AuctionConfig::AuctionConfig() = default;
AuctionConfig::AuctionConfig(const AuctionConfig&) = default;
AuctionConfig::AuctionConfig(AuctionConfig&&) = default;
AuctionConfig::~AuctionConfig() = default;

AuctionConfig& AuctionConfig::operator=(const AuctionConfig&) = default;
AuctionConfig& AuctionConfig::operator=(AuctionConfig&&) = default;

int AuctionConfig::NumPromises() const {
  int total = 0;
  if (non_shared_params.auction_signals.is_promise()) {
    ++total;
  }
  if (non_shared_params.seller_signals.is_promise()) {
    ++total;
  }
  if (non_shared_params.per_buyer_signals.is_promise()) {
    ++total;
  }
  if (non_shared_params.buyer_timeouts.is_promise()) {
    ++total;
  }
  if (non_shared_params.buyer_currencies.is_promise()) {
    ++total;
  }
  if (non_shared_params.buyer_cumulative_timeouts.is_promise()) {
    ++total;
  }
  if (direct_from_seller_signals.is_promise()) {
    ++total;
  }
  if (expects_direct_from_seller_signals_header_ad_slot) {
    ++total;
  }
  if (expects_additional_bids) {
    ++total;
  }
  for (const blink::AuctionConfig& sub_auction :
       non_shared_params.component_auctions) {
    total += sub_auction.NumPromises();
  }
  return total;
}

bool AuctionConfig::IsHttpsAndMatchesSellerOrigin(const GURL& url) const {
  return url.scheme() == url::kHttpsScheme &&
         url::Origin::Create(url) == seller;
}

bool AuctionConfig::IsDirectFromSellerSignalsValid(
    const std::optional<blink::DirectFromSellerSignals>&
        candidate_direct_from_seller_signals) const {
  if (!candidate_direct_from_seller_signals.has_value()) {
    return true;
  }

  const GURL& prefix = candidate_direct_from_seller_signals->prefix;
  // The prefix can't have a query because the browser process appends its own
  // query suffix.
  if (prefix.has_query()) {
    return false;
  }
  // NOTE: uuid-in-package isn't supported, since it doesn't support CORS.
  if (!IsHttpsAndMatchesSellerOrigin(prefix)) {
    return false;
  }

  base::flat_set<url::Origin> interest_group_buyers(
      non_shared_params.interest_group_buyers
          ? *non_shared_params.interest_group_buyers
          : std::vector<url::Origin>());
  for (const auto& [buyer_origin, bundle_url] :
       candidate_direct_from_seller_signals->per_buyer_signals) {
    // The renderer shouldn't provide bundles for origins that aren't buyers
    // in this auction -- there would be no worklet to receive them.
    if (interest_group_buyers.count(buyer_origin) < 1) {
      return false;
    }
    // All DirectFromSellerSignals must come from the seller.
    if (!IsHttpsAndMatchesSellerOrigin(bundle_url.bundle_url)) {
      return false;
    }
  }
  if (candidate_direct_from_seller_signals->seller_signals &&
      !IsHttpsAndMatchesSellerOrigin(
          candidate_direct_from_seller_signals->seller_signals->bundle_url)) {
    // All DirectFromSellerSignals must come from the seller.
    return false;
  }
  if (candidate_direct_from_seller_signals->auction_signals &&
      !IsHttpsAndMatchesSellerOrigin(
          candidate_direct_from_seller_signals->auction_signals->bundle_url)) {
    // All DirectFromSellerSignals must come from the seller.
    return false;
  }
  return true;
}

base::Value::Dict AuctionConfig::SerializeForDevtools() const {
  base::Value::Dict result;
  SerializeIntoDict("seller", seller, result);
  SerializeIntoDict("serverResponse", server_response, result);
  SerializeIntoDict("decisionLogicUrl", decision_logic_url, result);
  SerializeIntoDict("trustedScoringSignalsUrl", trusted_scoring_signals_url,
                    result);
  SerializeIntoDict("interestGroupBuyers",
                    non_shared_params.interest_group_buyers, result);
  SerializeIntoDict("auctionSignals", non_shared_params.auction_signals,
                    result);
  SerializeIntoDict("sellerSignals", non_shared_params.seller_signals, result);
  SerializeIntoDict("sellerTimeout", non_shared_params.seller_timeout, result);
  SerializeIntoDict("perBuyerSignals", non_shared_params.per_buyer_signals,
                    result);
  SerializeIntoDict("perBuyerTimeouts", non_shared_params.buyer_timeouts,
                    result);
  SerializeIntoDict("perBuyerCumulativeTimeouts",
                    non_shared_params.buyer_cumulative_timeouts, result);
  SerializeIntoDict("sellerCurrency", non_shared_params.seller_currency,
                    result);
  SerializeIntoDict("perBuyerCurrencies", non_shared_params.buyer_currencies,
                    result);
  SerializeIntoDict(
      "perBuyerGroupLimits",
      SerializeSplitMapHelper(non_shared_params.all_buyers_group_limit,
                              non_shared_params.per_buyer_group_limits),
      result);
  SerializeIntoDict(
      "perBuyerPrioritySignals",
      SerializeSplitMapHelper(non_shared_params.all_buyers_priority_signals,
                              non_shared_params.per_buyer_priority_signals),
      result);
  SerializeIntoDict("auctionReportBuyerKeys",
                    non_shared_params.auction_report_buyer_keys, result);
  SerializeIntoDict("auctionReportBuyers",
                    non_shared_params.auction_report_buyers, result);
  SerializeIntoDict("requiredSellerCapabilities",
                    non_shared_params.required_seller_capabilities, result);
  SerializeIntoDict("requestedSize", non_shared_params.requested_size, result);
  SerializeIntoDict("allSlotsRequestedSizes",
                    non_shared_params.all_slots_requested_sizes, result);
  SerializeIntoDict("auctionNonce", non_shared_params.auction_nonce, result);

  // For component auctions, we only serialize the seller names to give a
  // quick overview, since they'll get their own events.
  if (!non_shared_params.component_auctions.empty()) {
    base::Value::List component_auctions;
    for (const auto& child_config : non_shared_params.component_auctions) {
      component_auctions.Append(child_config.seller.Serialize());
    }
    result.Set("componentAuctions", std::move(component_auctions));
  }

  // direct_from_seller_signals --- skipped.
  SerializeIntoDict("expectsDirectFromSellerSignalsHeaderAdSlot",
                    expects_direct_from_seller_signals_header_ad_slot, result);
  SerializeIntoDict("sellerExperimentGroupId", seller_experiment_group_id,
                    result);
  SerializeIntoDict("perBuyerExperimentGroupIds",
                    SerializeSplitMapHelper(all_buyer_experiment_group_id,
                                            per_buyer_experiment_group_ids),
                    result);
  SerializeIntoDict("expectsAdditionalBids", expects_additional_bids, result);
  SerializeIntoDict("aggregationCoordinatorOrigin",
                    aggregation_coordinator_origin, result);

  return result;
}

}  // namespace blink

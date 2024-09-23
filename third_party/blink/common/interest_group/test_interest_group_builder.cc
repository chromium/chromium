// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/interest_group/test_interest_group_builder.h"

#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/interest_group/interest_group.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace blink {

TestInterestGroupBuilder::TestInterestGroupBuilder(url::Origin owner,
                                                   std::string name) {
  interest_group_.expiry = base::Time::Now() + base::Days(30);
  interest_group_.owner = std::move(owner);
  interest_group_.name = std::move(name);
}

TestInterestGroupBuilder::~TestInterestGroupBuilder() = default;

InterestGroup TestInterestGroupBuilder::Build() {
  return std::move(interest_group_);
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetExpiry(
    base::Time expiry) {
  interest_group_.expiry = expiry;
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetPriority(
    double priority) {
  interest_group_.priority = priority;
  return *this;
}

TestInterestGroupBuilder&
TestInterestGroupBuilder::SetEnableBiddingSignalsPrioritization(
    bool enable_bidding_signals_prioritization) {
  interest_group_.enable_bidding_signals_prioritization =
      enable_bidding_signals_prioritization;
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetPriorityVector(
    std::optional<base::flat_map<std::string, double>> priority_vector) {
  interest_group_.priority_vector = std::move(priority_vector);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetPrioritySignalsOverrides(
    std::optional<base::flat_map<std::string, double>>
        priority_signals_overrides) {
  interest_group_.priority_signals_overrides =
      std::move(priority_signals_overrides);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetSellerCapabilities(
    std::optional<base::flat_map<url::Origin, SellerCapabilitiesType>>
        seller_capabilities) {
  interest_group_.seller_capabilities = std::move(seller_capabilities);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetAllSellersCapabilities(
    SellerCapabilitiesType all_sellers_capabilities) {
  interest_group_.all_sellers_capabilities =
      std::move(all_sellers_capabilities);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetExecutionMode(
    InterestGroup::ExecutionMode execution_mode) {
  interest_group_.execution_mode = execution_mode;
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetBiddingUrl(
    std::optional<GURL> bidding_url) {
  interest_group_.bidding_url = std::move(bidding_url);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetBiddingWasmHelperUrl(
    std::optional<GURL> bidding_wasm_helper_url) {
  interest_group_.bidding_wasm_helper_url = std::move(bidding_wasm_helper_url);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetUpdateUrl(
    std::optional<GURL> update_url) {
  interest_group_.update_url = std::move(update_url);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetTrustedBiddingSignalsUrl(
    std::optional<GURL> trusted_bidding_signals_url) {
  interest_group_.trusted_bidding_signals_url =
      std::move(trusted_bidding_signals_url);
  return *this;
}

TestInterestGroupBuilder&
TestInterestGroupBuilder::SetTrustedBiddingSignalsSlotSizeMode(
    InterestGroup::TrustedBiddingSignalsSlotSizeMode
        trusted_bidding_signals_slot_size_mode) {
  interest_group_.trusted_bidding_signals_slot_size_mode =
      std::move(trusted_bidding_signals_slot_size_mode);
  return *this;
}

TestInterestGroupBuilder&
TestInterestGroupBuilder::SetTrustedBiddingSignalsKeys(
    std::optional<std::vector<std::string>> trusted_bidding_signals_keys) {
  interest_group_.trusted_bidding_signals_keys =
      std::move(trusted_bidding_signals_keys);
  return *this;
}

TestInterestGroupBuilder&
TestInterestGroupBuilder::SetMaxTrustedBiddingSignalsURLLength(
    int32_t max_trusted_bidding_signals_url_length) {
  interest_group_.max_trusted_bidding_signals_url_length =
      max_trusted_bidding_signals_url_length;
  return *this;
}

TestInterestGroupBuilder&
TestInterestGroupBuilder::SetTrustedBiddingSignalsCoordinator(
    std::optional<url::Origin> trusted_bidding_signals_coordinator) {
  interest_group_.trusted_bidding_signals_coordinator =
      trusted_bidding_signals_coordinator;
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetUserBiddingSignals(
    std::optional<std::string> user_bidding_signals) {
  interest_group_.user_bidding_signals = std::move(user_bidding_signals);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetAds(
    std::optional<std::vector<InterestGroup::Ad>> ads) {
  interest_group_.ads = std::move(ads);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetAdComponents(
    std::optional<std::vector<InterestGroup::Ad>> ad_components) {
  interest_group_.ad_components = std::move(ad_components);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetAdSizes(
    std::optional<base::flat_map<std::string, blink::AdSize>> ad_sizes) {
  interest_group_.ad_sizes = std::move(ad_sizes);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetSizeGroups(
    std::optional<base::flat_map<std::string, std::vector<std::string>>>
        size_groups) {
  interest_group_.size_groups = std::move(size_groups);
  return *this;
}

TestInterestGroupBuilder&
TestInterestGroupBuilder::SetAuctionServerRequestFlags(
    AuctionServerRequestFlags flags) {
  interest_group_.auction_server_request_flags = std::move(flags);
  return *this;
}

TestInterestGroupBuilder& TestInterestGroupBuilder::SetAdditionalBidKey(
    std::optional<blink::InterestGroup::AdditionalBidKey> key) {
  interest_group_.additional_bid_key = std::move(key);
  return *this;
}

TestInterestGroupBuilder&
TestInterestGroupBuilder::SetAggregationCoordinatorOrigin(
    std::optional<url::Origin> agg_coordinator_origin) {
  interest_group_.aggregation_coordinator_origin = agg_coordinator_origin;
  return *this;
}

}  // namespace blink

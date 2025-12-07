// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/services/auction_worklet/public/cpp/test_bid_builder.h"

#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"

namespace auction_worklet {
TestBidBuilder::TestBidBuilder() {
  bid_ = auction_worklet::mojom::BidderWorkletBid::New();
  bid_->bid_role = auction_worklet::mojom::BidRole::kUnenforcedKAnon;
  bid_->ad = "null";
  bid_->bid = 1;
  bid_->ad_descriptor = blink::AdDescriptor(GURL("https://response.test/"));
}

TestBidBuilder::~TestBidBuilder() = default;

auction_worklet::mojom::BidderWorkletBidPtr TestBidBuilder::Build() {
  return std::move(bid_);
}

TestBidBuilder& TestBidBuilder::SetBidRole(
    auction_worklet::mojom::BidRole bid_role) {
  bid_->bid_role = bid_role;
  return *this;
}

TestBidBuilder& TestBidBuilder::SetAd(const std::string& ad) {
  bid_->ad = ad;
  return *this;
}

TestBidBuilder& TestBidBuilder::SetBid(double bid) {
  bid_->bid = bid;
  return *this;
}

TestBidBuilder& TestBidBuilder::SetBidCurrency(
    const std::optional<blink::AdCurrency>& bid_currency) {
  bid_->bid_currency = bid_currency;
  return *this;
}

TestBidBuilder& TestBidBuilder::SetAdCost(std::optional<double> ad_cost) {
  bid_->ad_cost = ad_cost;
  return *this;
}

TestBidBuilder& TestBidBuilder::SetAdDescriptor(
    const blink::AdDescriptor& ad_descriptor) {
  bid_->ad_descriptor = ad_descriptor;
  return *this;
}

TestBidBuilder& TestBidBuilder::SetSelectedBuyerAndSellerReportingId(
    const std::optional<std::string>& selected_buyer_and_seller_reporting_id) {
  bid_->selected_buyer_and_seller_reporting_id =
      selected_buyer_and_seller_reporting_id;
  return *this;
}

TestBidBuilder& TestBidBuilder::SetAdComponentDescriptors(
    const std::optional<std::vector<blink::AdDescriptor>>&
        ad_component_descriptors) {
  bid_->ad_component_descriptors = ad_component_descriptors;
  return *this;
}

TestBidBuilder& TestBidBuilder::SetModelingSignals(
    const std::optional<uint16_t>& modeling_signals) {
  bid_->modeling_signals = modeling_signals;
  return *this;
}

TestBidBuilder& TestBidBuilder::SetAggregateWinSignals(
    const std::optional<std::string>& aggregate_win_signals) {
  bid_->aggregate_win_signals = aggregate_win_signals;
  return *this;
}

TestBidBuilder& TestBidBuilder::SetBidDuration(base::TimeDelta bid_duration) {
  bid_->bid_duration = bid_duration;
  return *this;
}

}  // namespace auction_worklet

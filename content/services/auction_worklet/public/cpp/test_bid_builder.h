// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_TEST_BID_BUILDER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_TEST_BID_BUILDER_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "third_party/blink/public/common/interest_group/ad_display_size.h"

namespace auction_worklet {

// Test-only builder for creating single-use bids.
// By default, creates a bid with the following values:
//   bid_role      -> kUnenforcedKAnon
//   ad            -> "null"
//   bid           -> 1
//   ad_descriptor -> "https://response.test/"
class TestBidBuilder {
 public:
  TestBidBuilder();
  ~TestBidBuilder();

  auction_worklet::mojom::BidderWorkletBidPtr Build();

  TestBidBuilder& SetBidRole(auction_worklet::mojom::BidRole bid_role);
  TestBidBuilder& SetAd(const std::string& ad);
  TestBidBuilder& SetBid(double bid);
  TestBidBuilder& SetBidCurrency(
      const std::optional<blink::AdCurrency>& bid_currency);
  TestBidBuilder& SetAdCost(std::optional<double> ad_cost);
  TestBidBuilder& SetAdDescriptor(const blink::AdDescriptor& ad_descriptor);
  TestBidBuilder& SetSelectedBuyerAndSellerReportingId(
      const std::optional<std::string>& selected_buyer_and_seller_reporting_id);
  TestBidBuilder& SetAdComponentDescriptors(
      const std::optional<std::vector<blink::AdDescriptor>>&
          ad_component_descriptors);
  TestBidBuilder& SetModelingSignals(
      const std::optional<uint16_t>& modeling_signals);
  TestBidBuilder& SetAggregateWinSignals(
      const std::optional<std::string>& aggregate_win_signals);
  TestBidBuilder& SetBidDuration(base::TimeDelta bid_duration);

 private:
  auction_worklet::mojom::BidderWorkletBidPtr bid_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_PUBLIC_CPP_TEST_BID_BUILDER_H_

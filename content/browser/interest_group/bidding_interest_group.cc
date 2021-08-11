// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/bidding_interest_group.h"

#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"

namespace content {

BiddingInterestGroup::BiddingInterestGroup() = default;
BiddingInterestGroup::BiddingInterestGroup(
    auction_worklet::mojom::BiddingInterestGroupPtr group) {
  this->group = std::move(group);
}
BiddingInterestGroup::BiddingInterestGroup(BiddingInterestGroup&&) = default;
BiddingInterestGroup::~BiddingInterestGroup() = default;

}  // namespace content

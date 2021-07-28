// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/debuggable_auction_worklet.h"

#include "content/browser/interest_group/debuggable_auction_worklet_tracker.h"

namespace content {

DebuggableAuctionWorklet::DebuggableAuctionWorklet(
    RenderFrameHostImpl* owning_frame,
    const GURL& url,
    auction_worklet::mojom::BidderWorklet* bidder_worklet,
    bool& should_pause_on_start)
    : owning_frame_(owning_frame), url_(url), bidder_worklet_(bidder_worklet) {
  DebuggableAuctionWorkletTracker::GetInstance()->NotifyCreated(
      this, should_pause_on_start);
}

DebuggableAuctionWorklet::DebuggableAuctionWorklet(
    RenderFrameHostImpl* owning_frame,
    const GURL& url,
    auction_worklet::mojom::SellerWorklet* seller_worklet,
    bool& should_pause_on_start)
    : owning_frame_(owning_frame), url_(url), seller_worklet_(seller_worklet) {
  DebuggableAuctionWorkletTracker::GetInstance()->NotifyCreated(
      this, should_pause_on_start);
}

DebuggableAuctionWorklet::~DebuggableAuctionWorklet() {
  DebuggableAuctionWorkletTracker::GetInstance()->NotifyDestroyed(this);
}

}  // namespace content

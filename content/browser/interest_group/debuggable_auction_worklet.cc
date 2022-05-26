// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/debuggable_auction_worklet.h"

#include "base/strings/strcat.h"
#include "content/browser/interest_group/debuggable_auction_worklet_tracker.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace content {

std::string DebuggableAuctionWorklet::Title() const {
  if (absl::holds_alternative<auction_worklet::mojom::BidderWorklet*>(
          worklet_)) {
    return base::StrCat({"FLEDGE bidder worklet for ", url_.spec()});
  } else {
    return base::StrCat({"FLEDGE seller worklet for ", url_.spec()});
  }
}

void DebuggableAuctionWorklet::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  if (auction_worklet::mojom::BidderWorklet** bidder_worklet =
          absl::get_if<auction_worklet::mojom::BidderWorklet*>(&worklet_)) {
    (*bidder_worklet)->ConnectDevToolsAgent(std::move(agent));
  } else {
    absl::get<auction_worklet::mojom::SellerWorklet*>(worklet_)
        ->ConnectDevToolsAgent(std::move(agent));
  }
}

absl::optional<base::ProcessId> DebuggableAuctionWorklet::GetPid(
    PidCallback callback) {
  return process_handle_->GetPid(std::move(callback));
}

DebuggableAuctionWorklet::DebuggableAuctionWorklet(
    RenderFrameHostImpl* owning_frame,
    AuctionProcessManager::ProcessHandle* process_handle,
    const GURL& url,
    auction_worklet::mojom::BidderWorklet* bidder_worklet)
    : owning_frame_(owning_frame),
      process_handle_(process_handle),
      url_(url),
      worklet_(bidder_worklet) {
  DebuggableAuctionWorkletTracker::GetInstance()->NotifyCreated(
      this, should_pause_on_start_);
}

DebuggableAuctionWorklet::DebuggableAuctionWorklet(
    RenderFrameHostImpl* owning_frame,
    AuctionProcessManager::ProcessHandle* process_handle,
    const GURL& url,
    auction_worklet::mojom::SellerWorklet* seller_worklet)
    : owning_frame_(owning_frame),
      process_handle_(process_handle),
      url_(url),
      worklet_(seller_worklet) {
  DebuggableAuctionWorkletTracker::GetInstance()->NotifyCreated(
      this, should_pause_on_start_);
}

DebuggableAuctionWorklet::~DebuggableAuctionWorklet() {
  DebuggableAuctionWorkletTracker::GetInstance()->NotifyDestroyed(this);
}

}  // namespace content

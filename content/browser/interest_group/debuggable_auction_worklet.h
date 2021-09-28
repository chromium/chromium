// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_DEBUGGABLE_AUCTION_WORKLET_H_
#define CONTENT_BROWSER_INTEREST_GROUP_DEBUGGABLE_AUCTION_WORKLET_H_

#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-forward.h"
#include "url/gurl.h"

namespace content {

class RenderFrameHostImpl;

// This is an opaque representation of a worklet (either buyer or seller) for
// help in interfacing with a debugger --- adding observers to
// DebuggableAuctionWorkletTracker will notify of creation/destruction of these.
class CONTENT_EXPORT DebuggableAuctionWorklet {
 public:
  explicit DebuggableAuctionWorklet(const DebuggableAuctionWorklet&) = delete;
  DebuggableAuctionWorklet& operator=(const DebuggableAuctionWorklet&) = delete;

  const GURL& url() const { return url_; }
  void ConnectDevToolsAgent(
      mojo::PendingReceiver<blink::mojom::DevToolsAgent> agent);

  RenderFrameHostImpl* owning_frame() const { return owning_frame_; }

 private:
  friend class AuctionRunner;
  friend class std::default_delete<DebuggableAuctionWorklet>;

  // Registers `this` with DebuggableAuctionWorkletTracker, and passes through
  // NotifyCreated() observers.
  //
  // The mojo pipe must outlive `this`, as must `owning_frame`.
  //
  // `should_pause_on_start` will output, at constructor completion, if any
  // observer has requested the worklet to pause before starting work until
  // resumed by debugger.
  DebuggableAuctionWorklet(
      RenderFrameHostImpl* owning_frame,
      const GURL& url,
      auction_worklet::mojom::BidderWorklet* bidder_worklet,
      bool& should_pause_on_start);
  DebuggableAuctionWorklet(
      RenderFrameHostImpl* owning_frame,
      const GURL& url,
      auction_worklet::mojom::SellerWorklet* seller_worklet,
      bool& should_pause_on_start);

  // Unregisters `this` from DebuggableAuctionWorkletTracker, and notifies
  // NotifyDestroyed() observers.
  ~DebuggableAuctionWorklet();

  RenderFrameHostImpl* const owning_frame_ = nullptr;
  const GURL url_;

  absl::variant<auction_worklet::mojom::BidderWorklet*,
                auction_worklet::mojom::SellerWorklet*>
      worklet_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_DEBUGGABLE_AUCTION_WORKLET_H_

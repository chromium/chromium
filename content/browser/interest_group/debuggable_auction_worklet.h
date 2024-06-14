// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_DEBUGGABLE_AUCTION_WORKLET_H_
#define CONTENT_BROWSER_INTEREST_GROUP_DEBUGGABLE_AUCTION_WORKLET_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/process/process_handle.h"
#include "content/browser/interest_group/auction_process_manager.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom-forward.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/mojom/devtools/devtools_agent.mojom-forward.h"
#include "url/gurl.h"

namespace perfetto {
class TracedValue;
}  // namespace perfetto

namespace content {

class RenderFrameHostImpl;

// This is an opaque representation of a worklet (either buyer or seller) for
// help in interfacing with a debugger --- adding observers to
// DebuggableAuctionWorkletTracker will notify of creation/destruction of these.
class CONTENT_EXPORT DebuggableAuctionWorklet {
 public:
  enum class WorkletType {
    kBidder,
    kSeller,
  };

  using PidCallback = base::OnceCallback<void(base::ProcessId)>;

  explicit DebuggableAuctionWorklet(const DebuggableAuctionWorklet&) = delete;
  DebuggableAuctionWorklet& operator=(const DebuggableAuctionWorklet&) = delete;

  const GURL& url() const { return url_; }
  RenderFrameHostImpl* owning_frame() const { return owning_frame_; }

  // Human-readable description of the worklet. (For English-speaking humans,
  // anyway).
  std::string Title() const;

  // Returns a random GUID associated with this worklet.
  const std::string& UniqueId() const { return unique_id_; }

  WorkletType Type() const;

  void ConnectDevToolsAgent(
      mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent);

  // Returns true if the worklet should start in the paused state.
  bool should_pause_on_start() const { return should_pause_on_start_; }

  std::optional<base::ProcessId> GetPid(PidCallback callback);

 private:
  friend class AuctionRunner;
  friend class AuctionWorkletManager;
  friend class std::default_delete<DebuggableAuctionWorklet>;

  // Registers `this` with DebuggableAuctionWorkletTracker, and passes through
  // NotifyCreated() observers.
  //
  // The mojo pipe must outlive `this`, as must `owning_frame` and
  // `process_handle`.
  DebuggableAuctionWorklet(
      RenderFrameHostImpl* owning_frame,
      AuctionProcessManager::ProcessHandle& process_handle,
      const GURL& url,
      auction_worklet::mojom::BidderWorklet* bidder_worklet,
      size_t thread_index);
  DebuggableAuctionWorklet(
      RenderFrameHostImpl* owning_frame,
      AuctionProcessManager::ProcessHandle& process_handle,
      const GURL& url,
      auction_worklet::mojom::SellerWorklet* seller_worklet,
      size_t thread_index);

  // Unregisters `this` from DebuggableAuctionWorkletTracker, and notifies
  // NotifyDestroyed() observers.
  ~DebuggableAuctionWorklet();

  void RequestPid();
  void OnHavePid(base::ProcessId process_id);

  // Records parameter data for auction process assignment events.
  void TraceProcessData(perfetto::TracedValue trace_context);

  const raw_ptr<RenderFrameHostImpl> owning_frame_ = nullptr;
  const raw_ref<AuctionProcessManager::ProcessHandle> process_handle_;
  const GURL url_;
  const std::string unique_id_;

  std::optional<base::ProcessId> pid_;
  bool should_pause_on_start_ = false;

  absl::variant<auction_worklet::mojom::BidderWorklet*,
                auction_worklet::mojom::SellerWorklet*>
      worklet_;

  size_t thread_index_ = 0;

  base::WeakPtrFactory<DebuggableAuctionWorklet> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_DEBUGGABLE_AUCTION_WORKLET_H_

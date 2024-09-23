// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/debuggable_auction_worklet.h"

#include "base/strings/strcat.h"
#include "base/trace_event/trace_event.h"
#include "base/uuid.h"
#include "content/browser/interest_group/debuggable_auction_worklet_tracker.h"
#include "content/services/auction_worklet/public/mojom/bidder_worklet.mojom.h"
#include "content/services/auction_worklet/public/mojom/seller_worklet.mojom.h"

namespace content {

std::string DebuggableAuctionWorklet::Title() const {
  if (absl::holds_alternative<auction_worklet::mojom::BidderWorklet*>(
          worklet_)) {
    return base::StrCat({"FLEDGE bidder worklet for ", url_.spec()});
  } else {
    return base::StrCat({"FLEDGE seller worklet for ", url_.spec()});
  }
}

DebuggableAuctionWorklet::WorkletType DebuggableAuctionWorklet::Type() const {
  return absl::holds_alternative<auction_worklet::mojom::BidderWorklet*>(
             worklet_)
             ? WorkletType::kBidder
             : WorkletType::kSeller;
}

void DebuggableAuctionWorklet::ConnectDevToolsAgent(
    mojo::PendingAssociatedReceiver<blink::mojom::DevToolsAgent> agent) {
  if (auction_worklet::mojom::BidderWorklet** bidder_worklet =
          absl::get_if<auction_worklet::mojom::BidderWorklet*>(&worklet_)) {
    (*bidder_worklet)->ConnectDevToolsAgent(std::move(agent), thread_index_);
  } else {
    absl::get<auction_worklet::mojom::SellerWorklet*>(worklet_)
        ->ConnectDevToolsAgent(std::move(agent), thread_index_);
  }
}

DebuggableAuctionWorklet::DebuggableAuctionWorklet(
    RenderFrameHostImpl* owning_frame,
    AuctionProcessManager::ProcessHandle& process_handle,
    const GURL& url,
    auction_worklet::mojom::BidderWorklet* bidder_worklet,
    size_t thread_index)
    : owning_frame_(owning_frame),
      process_handle_(process_handle),
      url_(url),
      unique_id_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      worklet_(bidder_worklet),
      thread_index_(thread_index) {
  DebuggableAuctionWorkletTracker::GetInstance()->NotifyCreated(
      this, should_pause_on_start_);
  RequestPid();
}

DebuggableAuctionWorklet::DebuggableAuctionWorklet(
    RenderFrameHostImpl* owning_frame,
    AuctionProcessManager::ProcessHandle& process_handle,
    const GURL& url,
    auction_worklet::mojom::SellerWorklet* seller_worklet,
    size_t thread_index)
    : owning_frame_(owning_frame),
      process_handle_(process_handle),
      url_(url),
      unique_id_(base::Uuid::GenerateRandomV4().AsLowercaseString()),
      worklet_(seller_worklet),
      thread_index_(thread_index) {
  DebuggableAuctionWorkletTracker::GetInstance()->NotifyCreated(
      this, should_pause_on_start_);
  RequestPid();
}

DebuggableAuctionWorklet::~DebuggableAuctionWorklet() {
  DebuggableAuctionWorkletTracker::GetInstance()->NotifyDestroyed(this);
  if (pid_.has_value()) {
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                         "AuctionWorkletDoneWithProcess",
                         TRACE_EVENT_SCOPE_THREAD, "data",
                         [&](perfetto::TracedValue trace_context) {
                           TraceProcessData(std::move(trace_context));
                         });
  }
}

std::optional<base::ProcessId> DebuggableAuctionWorklet::GetPid(
    PidCallback callback) {
  return process_handle_->GetPid(std::move(callback));
}

void DebuggableAuctionWorklet::RequestPid() {
  std::optional<base::ProcessId> maybe_pid = process_handle_->GetPid(
      base::BindOnce(&DebuggableAuctionWorklet::OnHavePid,
                     weak_ptr_factory_.GetWeakPtr()));
  if (maybe_pid.has_value())
    OnHavePid(maybe_pid.value());
}

void DebuggableAuctionWorklet::OnHavePid(base::ProcessId process_id) {
  pid_ = process_id;
  // TODO(caseq): move all timeline-specific tracing logic to
  // devtools side (i.e. AuctionWorkletDevToolsAgentHost).
  TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"),
                       "AuctionWorkletRunningInProcess",
                       TRACE_EVENT_SCOPE_THREAD, "data",
                       [&](perfetto::TracedValue trace_context) {
                         TraceProcessData(std::move(trace_context));
                       });
}

void DebuggableAuctionWorklet::TraceProcessData(
    perfetto::TracedValue trace_context) {
  DCHECK(pid_.has_value());
  auto dict = std::move(trace_context).WriteDictionary();
  dict.Add("target", unique_id_);
  dict.Add("pid", pid_.value());
  dict.Add("host", url_.host());
  switch (Type()) {
    case DebuggableAuctionWorklet::WorkletType::kBidder:
      dict.Add("type", "bidder");
      break;
    case DebuggableAuctionWorklet::WorkletType::kSeller:
      dict.Add("type", "seller");
      break;
  }
}

}  // namespace content

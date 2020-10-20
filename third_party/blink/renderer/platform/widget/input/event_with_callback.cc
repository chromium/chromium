// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/event_with_callback.h"

#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/metrics/event_metrics.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"

namespace blink {

EventWithCallback::EventWithCallback(
    std::unique_ptr<WebCoalescedInputEvent> event,
    base::TimeTicks timestamp_now,
    InputHandlerProxy::EventDispositionCallback callback,
    std::unique_ptr<cc::EventMetrics> metrics)
    : event_(std::make_unique<WebCoalescedInputEvent>(*event)),
      creation_timestamp_(timestamp_now),
      last_coalesced_timestamp_(timestamp_now) {
  original_events_.emplace_back(std::move(event), std::move(metrics),
                                std::move(callback));
}

EventWithCallback::EventWithCallback(
    std::unique_ptr<WebCoalescedInputEvent> event,
    base::TimeTicks creation_timestamp,
    base::TimeTicks last_coalesced_timestamp,
    OriginalEventList original_events)
    : event_(std::move(event)),
      original_events_(std::move(original_events)),
      creation_timestamp_(creation_timestamp),
      last_coalesced_timestamp_(last_coalesced_timestamp) {}

EventWithCallback::~EventWithCallback() {}

bool EventWithCallback::CanCoalesceWith(const EventWithCallback& other) const {
  return event().CanCoalesce(other.event());
}

void EventWithCallback::SetScrollbarManipulationHandledOnCompositorThread() {
  for (auto& original_event : original_events_) {
    original_event.event_->EventPointer()
        ->SetScrollbarManipulationHandledOnCompositorThread();
  }
}

void EventWithCallback::CoalesceWith(EventWithCallback* other,
                                     base::TimeTicks timestamp_now) {
  event_->CoalesceWith(*other->event_);

  // Move original events.
  original_events_.splice(original_events_.end(), other->original_events_);
  last_coalesced_timestamp_ = timestamp_now;
}

static bool HandledOnCompositorThread(
    InputHandlerProxy::EventDisposition disposition) {
  return (disposition != InputHandlerProxy::DID_NOT_HANDLE &&
          disposition !=
              InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING_DUE_TO_FLING &&
          disposition != InputHandlerProxy::DID_HANDLE_NON_BLOCKING);
}

void EventWithCallback::RunCallbacks(
    InputHandlerProxy::EventDisposition disposition,
    const ui::LatencyInfo& latency,
    std::unique_ptr<InputHandlerProxy::DidOverscrollParams>
        did_overscroll_params,
    const WebInputEventAttribution& attribution) {
  // |original_events_| could be empty if this is the scroll event extracted
  // from the matrix multiplication.
  if (original_events_.size() == 0)
    return;

  // Ack the oldest event with original latency.
  auto& oldest_event = original_events_.front();
  oldest_event.event_->latency_info() = latency;
  std::move(oldest_event.callback_)
      .Run(disposition, std::move(oldest_event.event_),
           did_overscroll_params
               ? std::make_unique<InputHandlerProxy::DidOverscrollParams>(
                     *did_overscroll_params)
               : nullptr,
           attribution, std::move(oldest_event.metrics_));
  original_events_.pop_front();

  // If the event was handled on compositor thread, ack other events with
  // coalesced latency to avoid redundant tracking. If not, the event should
  // be handle on main thread, use the original latency instead.
  //
  // We overwrite the trace_id to ensure proper flow events along the critical
  // path.
  bool handled = HandledOnCompositorThread(disposition);
  for (auto& coalesced_event : original_events_) {
    if (handled) {
      int64_t original_trace_id =
          coalesced_event.event_->latency_info().trace_id();
      coalesced_event.event_->latency_info() = latency;
      coalesced_event.event_->latency_info().set_trace_id(original_trace_id);
      coalesced_event.event_->latency_info().set_coalesced();
    }
    std::move(coalesced_event.callback_)
        .Run(disposition, std::move(coalesced_event.event_),
             did_overscroll_params
                 ? std::make_unique<InputHandlerProxy::DidOverscrollParams>(
                       *did_overscroll_params)
                 : nullptr,
             attribution, std::move(coalesced_event.metrics_));
  }
}

EventWithCallback::OriginalEventWithCallback::OriginalEventWithCallback(
    std::unique_ptr<WebCoalescedInputEvent> event,
    std::unique_ptr<cc::EventMetrics> metrics,
    InputHandlerProxy::EventDispositionCallback callback)
    : event_(std::move(event)),
      metrics_(std::move(metrics)),
      callback_(std::move(callback)) {}

EventWithCallback::OriginalEventWithCallback::~OriginalEventWithCallback() =
    default;

}  // namespace blink

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/event_with_callback.h"

#include "base/trace_event/trace_event.h"
#include "cc/metrics/event_metrics.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"

namespace blink {

EventWithCallback::EventWithCallback(
    std::unique_ptr<WebCoalescedInputEvent> event,
    InputHandlerProxy::EventDispositionCallback callback,
    std::unique_ptr<cc::EventMetrics> metrics)
    : event_(std::make_unique<WebCoalescedInputEvent>(*event)) {
  original_events_.emplace_back(std::move(event), std::move(metrics),
                                std::move(callback));
}

EventWithCallback::EventWithCallback(
    std::unique_ptr<WebCoalescedInputEvent> event,
    OriginalEventList original_events)
    : event_(std::move(event)), original_events_(std::move(original_events)) {}

EventWithCallback::~EventWithCallback() = default;

bool EventWithCallback::CanCoalesceWith(const EventWithCallback& other) const {
  return event().CanCoalesce(other.event());
}

void EventWithCallback::SetScrollbarManipulationHandledOnCompositorThread() {
  for (auto& original_event : original_events_) {
    original_event.event_->EventPointer()
        ->SetScrollbarManipulationHandledOnCompositorThread();
  }
}

void EventWithCallback::CoalesceWith(EventWithCallback* other) {
  event_->CoalesceWith(*other->event_);
  auto* metrics = original_events_.empty()
                      ? nullptr
                      : original_events_.front().metrics_.get();
  auto* scroll_update_metrics = metrics ? metrics->AsScrollUpdate() : nullptr;
  auto* other_metrics = other->original_events_.empty()
                            ? nullptr
                            : other->original_events_.front().metrics_.get();
  auto* other_scroll_update_metrics =
      other_metrics ? other_metrics->AsScrollUpdate() : nullptr;
  if (scroll_update_metrics && other_scroll_update_metrics)
    scroll_update_metrics->CoalesceWith(*other_scroll_update_metrics);

  // Move original events.
  original_events_.splice(original_events_.end(), other->original_events_);
}

static bool HandledOnCompositorThread(
    InputHandlerProxy::EventDisposition disposition) {
  return (disposition != InputHandlerProxy::DID_NOT_HANDLE &&
          disposition !=
              InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING_DUE_TO_FLING &&
          disposition != InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING);
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

  // If the event was handled on the compositor thread, ack other events with
  // coalesced latency to avoid redundant tracking. `cc::EventMetrics` objects
  // will also be nullptr in this case because `TakeMetrics()` function is
  // already called and deleted them. This is fine since no further processing
  // and metrics reporting will be done on the events.
  //
  // On the other hand, if the event was not handled, original events should be
  // handled on the main thread. So, original latencies and `cc::EventMetrics`
  // should be used.
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

std::unique_ptr<cc::EventMetrics> EventWithCallback::TakeMetrics() {
  auto it = original_events_.begin();

  // Scroll events extracted from the matrix multiplication have no original
  // events and we don't report metrics for them.
  if (it == original_events_.end())
    return nullptr;

  // Throw away all original metrics except for the first one as they are not
  // useful anymore.
  auto first = it++;
  for (; it != original_events_.end(); it++)
    it->metrics_ = nullptr;

  // Return metrics for the first original event for reporting purposes.
  return std::move(first->metrics_);
}

void EventWithCallback::WillStartProcessingForMetrics() {
  DCHECK(metrics());
  for (auto& original_event : original_events_) {
    if (original_event.metrics_) {
      original_event.metrics_->SetDispatchStageTimestamp(
          cc::EventMetrics::DispatchStage::kRendererCompositorStarted);
    }
  }
}

void EventWithCallback::DidCompleteProcessingForMetrics() {
  DCHECK(metrics());
  for (auto& original_event : original_events_) {
    if (original_event.metrics_) {
      original_event.metrics_->SetDispatchStageTimestamp(
          cc::EventMetrics::DispatchStage::kRendererCompositorFinished);
    }
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

// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/input/web_coalesced_input_event.h"

#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "third_party/blink/public/common/input/web_gesture_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"

namespace blink {

WebInputEvent* WebCoalescedInputEvent::EventPointer() {
  return event_.get();
}

void WebCoalescedInputEvent::AddCoalescedEvent(
    const blink::WebInputEvent& event) {
  coalesced_events_.push_back(event.Clone());
}

const WebInputEvent& WebCoalescedInputEvent::Event() const {
  return *event_.get();
}

size_t WebCoalescedInputEvent::CoalescedEventSize() const {
  return coalesced_events_.size();
}

const WebInputEvent& WebCoalescedInputEvent::CoalescedEvent(
    size_t index) const {
  return *coalesced_events_[index].get();
}

const std::vector<std::unique_ptr<WebInputEvent>>&
WebCoalescedInputEvent::GetCoalescedEventsPointers() const {
  return coalesced_events_;
}

void WebCoalescedInputEvent::AddPredictedEvent(
    const blink::WebInputEvent& event) {
  predicted_events_.push_back(event.Clone());
}

size_t WebCoalescedInputEvent::PredictedEventSize() const {
  return predicted_events_.size();
}

const WebInputEvent& WebCoalescedInputEvent::PredictedEvent(
    size_t index) const {
  return *predicted_events_[index].get();
}

const std::vector<std::unique_ptr<WebInputEvent>>&
WebCoalescedInputEvent::GetPredictedEventsPointers() const {
  return predicted_events_;
}

WebCoalescedInputEvent::WebCoalescedInputEvent(const WebInputEvent& event,
                                               const ui::LatencyInfo& latency)
    : WebCoalescedInputEvent(event.Clone(), latency) {}

WebCoalescedInputEvent::WebCoalescedInputEvent(
    std::unique_ptr<WebInputEvent> event,
    const ui::LatencyInfo& latency)
    : event_(std::move(event)), latency_(latency) {
  DCHECK(event_);
  coalesced_events_.push_back(event_->Clone());
}

WebCoalescedInputEvent::WebCoalescedInputEvent(
    std::unique_ptr<WebInputEvent> event,
    std::vector<std::unique_ptr<WebInputEvent>> coalesced_events,
    std::vector<std::unique_ptr<WebInputEvent>> predicted_events,
    const ui::LatencyInfo& latency)
    : event_(std::move(event)),
      coalesced_events_(std::move(coalesced_events)),
      predicted_events_(std::move(predicted_events)),
      latency_(latency) {}

WebCoalescedInputEvent::WebCoalescedInputEvent(
    const WebCoalescedInputEvent& event) {
  event_ = event.event_->Clone();
  latency_ = event.latency_;
  for (const auto& coalesced_event : event.coalesced_events_)
    coalesced_events_.push_back(coalesced_event->Clone());
  for (const auto& predicted_event : event.predicted_events_)
    predicted_events_.push_back(predicted_event->Clone());
}

WebCoalescedInputEvent::~WebCoalescedInputEvent() = default;

bool WebCoalescedInputEvent::CanCoalesceWith(
    const WebCoalescedInputEvent& other) const {
  return event_->CanCoalesce(*other.event_);
}

void WebCoalescedInputEvent::CoalesceWith(
    const WebCoalescedInputEvent& newer_event) {
  TRACE_EVENT2("input", "WebCoalescedInputEvent::CoalesceWith", "traceId",
               latency_.trace_id(), "coalescedTraceId",
               newer_event.latency_.trace_id());

  // New events get coalesced into older events, and the newer timestamp
  // should always be preserved.
  const base::TimeTicks time_stamp = newer_event.event_->TimeStamp();
  event_->Coalesce(*newer_event.event_);
  event_->SetTimeStamp(time_stamp);
  AddCoalescedEvent(*newer_event.event_);

  TRACE_EVENT("input", "WebCoalescedInputEvent::CoalesceWith",
              [trace_id = newer_event.latency_.trace_id(),
               coalesced_to_trace_id =
                   latency_.trace_id()](perfetto::EventContext& ctx) {
                auto* event =
                    ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
                auto* scroll_data = event->set_scroll_deltas();
                scroll_data->set_trace_id(trace_id);
                scroll_data->set_coalesced_to_trace_id(coalesced_to_trace_id);
              });
}

}  // namespace blink

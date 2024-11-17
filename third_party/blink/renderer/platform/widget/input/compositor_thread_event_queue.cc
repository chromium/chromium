// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/compositor_thread_event_queue.h"

#include "base/trace_event/trace_event.h"
#include "cc/metrics/event_metrics.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"

namespace blink {

namespace {
// Sets |oldest_scroll_trace_id| or |oldest_pinch_trace_id| depending on the
// type of |event|.
void SetScrollOrPinchTraceId(EventWithCallback* event,
                             int64_t* oldest_scroll_trace_id,
                             int64_t* oldest_pinch_trace_id) {
  if (event->event().GetType() == WebInputEvent::Type::kGestureScrollUpdate) {
    DCHECK_EQ(-1, *oldest_scroll_trace_id);
    *oldest_scroll_trace_id = event->latency_info().trace_id();
    return;
  }
  DCHECK_EQ(WebInputEvent::Type::kGesturePinchUpdate, event->event().GetType());
  DCHECK_EQ(-1, *oldest_pinch_trace_id);
  *oldest_pinch_trace_id = event->latency_info().trace_id();
}

inline const WebGestureEvent& ToWebGestureEvent(const WebInputEvent& event) {
  DCHECK(WebInputEvent::IsGestureEventType(event.GetType()));
  return static_cast<const WebGestureEvent&>(event);
}

bool IsContinuousGestureEvent(WebInputEvent::Type type) {
  switch (type) {
    case WebGestureEvent::Type::kGestureScrollUpdate:
    case WebGestureEvent::Type::kGesturePinchUpdate:
      return true;
    default:
      return false;
  }
}

}  // namespace

CompositorThreadEventQueue::CompositorThreadEventQueue() {}

CompositorThreadEventQueue::~CompositorThreadEventQueue() {
  while (!queue_.empty()) {
    auto event_with_callback = Pop();
    event_with_callback->RunCallbacks(
        InputHandlerProxy::DROP_EVENT, event_with_callback->latency_info(),
        /*did_overscroll_params=*/nullptr,
        /*attribution=*/WebInputEventAttribution());
  }
}

void CompositorThreadEventQueue::Queue(
    std::unique_ptr<EventWithCallback> new_event) {
  if (queue_.empty() ||
      !IsContinuousGestureEvent(new_event->event().GetType()) ||
      !(queue_.back()->CanCoalesceWith(*new_event) ||
        WebGestureEvent::IsCompatibleScrollorPinch(
            ToWebGestureEvent(new_event->event()),
            ToWebGestureEvent(queue_.back()->event())))) {
    if (new_event->first_original_event()) {
      // Trace could be nested as there might be multiple events in queue.
      // e.g. |ScrollUpdate|, |ScrollEnd|, and another scroll sequence.
      TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("input",
                                        "CompositorThreadEventQueue::Queue",
                                        new_event->first_original_event());
    }
    queue_.push_back(std::move(new_event));
    return;
  }

  if (queue_.back()->CanCoalesceWith(*new_event)) {
    queue_.back()->CoalesceWith(new_event.get());
    return;
  }

  // We have only scrolls or pinches at this point (all other events are
  // filtered out by the if statements above). We want to coalesce this event
  // into the previous event(s) and represent it as a scroll and then a pinch.
  DCHECK(IsContinuousGestureEvent(new_event->event().GetType()));

  // If there is only one event in the queue we will still emit two events
  // (scroll and pinch) but the |new_event| will still be coalesced into the
  // |last_event|, but there will be only one LatencyInfo that should be traced
  // for two events. In this case we will output an empty LatencyInfo.
  //
  // However with two events one will be a GesturePinchUpdate and one will be a
  // GestureScrollUpdate and we will use the two non-coalesced event's trace_ids
  // to instrument the flow through the system.
  int64_t oldest_scroll_trace_id = -1;
  int64_t oldest_pinch_trace_id = -1;
  ui::LatencyInfo oldest_latency;

  // Extract the last event in queue (again either a scroll or a pinch).
  std::unique_ptr<EventWithCallback> last_event = std::move(queue_.back());
  queue_.pop_back();

  DCHECK(IsContinuousGestureEvent(last_event->event().GetType()));

  SetScrollOrPinchTraceId(last_event.get(), &oldest_scroll_trace_id,
                          &oldest_pinch_trace_id);
  oldest_latency = last_event->latency_info();
  EventWithCallback::OriginalEventList combined_original_events;
  combined_original_events.splice(combined_original_events.end(),
                                  last_event->original_events());
  combined_original_events.splice(combined_original_events.end(),
                                  new_event->original_events());

  // Extract the second last event in queue IF it's a scroll or a pinch for the
  // same target.
  std::unique_ptr<EventWithCallback> second_last_event;
  if (!queue_.empty() && WebGestureEvent::IsCompatibleScrollorPinch(
                             ToWebGestureEvent(new_event->event()),
                             ToWebGestureEvent(queue_.back()->event()))) {
    second_last_event = std::move(queue_.back());
    queue_.pop_back();
    SetScrollOrPinchTraceId(second_last_event.get(), &oldest_scroll_trace_id,
                            &oldest_pinch_trace_id);
    oldest_latency = second_last_event->latency_info();
    combined_original_events.splice(combined_original_events.begin(),
                                    second_last_event->original_events());
  }

  // To ensure proper trace tracking we have to determine which event was the
  // original non-coalesced event. If the event was artificially created (I.E it
  // sprung into existence in CoalesceScrollAndPinch and isn't associated with a
  // WebInputEvent that was in the queue) we will give it an empty LatencyInfo
  // (so it won't have anything reported for it). This can be seen when a
  // trace_id is equal to -1. We also move the original events into whichever
  // one is the original non-coalesced event, defaulting to the pinch event if
  // both are non-coalesced versions so it runs last.
  ui::LatencyInfo scroll_latency;
  EventWithCallback::OriginalEventList scroll_original_events;
  ui::LatencyInfo pinch_latency;
  EventWithCallback::OriginalEventList pinch_original_events;
  DCHECK(oldest_pinch_trace_id == -1 || oldest_scroll_trace_id == -1);
  if (oldest_scroll_trace_id != -1) {
    scroll_latency = oldest_latency;
    scroll_latency.set_trace_id(oldest_scroll_trace_id);
    scroll_original_events = std::move(combined_original_events);
  } else {
    // In both the valid pinch event trace id case and scroll and pinch both
    // have invalid trace_ids case, we will assign original_events to the
    // pinch_event.
    pinch_latency = oldest_latency;
    pinch_latency.set_trace_id(oldest_pinch_trace_id);
    pinch_original_events = std::move(combined_original_events);
  }

  TRACE_EVENT2("input", "CoalesceScrollAndPinch", "coalescedTraceId",
               new_event->latency_info().trace_id(), "traceId",
               scroll_latency.trace_id() != -1 ? scroll_latency.trace_id()
                                               : pinch_latency.trace_id());
  std::pair<std::unique_ptr<WebGestureEvent>, std::unique_ptr<WebGestureEvent>>
      coalesced_events = WebGestureEvent::CoalesceScrollAndPinch(
          second_last_event ? &ToWebGestureEvent(second_last_event->event())
                            : nullptr,
          ToWebGestureEvent(last_event->event()),
          ToWebGestureEvent(new_event->event()));
  DCHECK(coalesced_events.first);
  DCHECK(coalesced_events.second);

  auto scroll_event = std::make_unique<EventWithCallback>(
      std::make_unique<WebCoalescedInputEvent>(
          std::move(coalesced_events.first), scroll_latency),
      std::move(scroll_original_events));
  scroll_event->set_coalesced_scroll_and_pinch();

  auto pinch_event = std::make_unique<EventWithCallback>(
      std::make_unique<WebCoalescedInputEvent>(
          std::move(coalesced_events.second), pinch_latency),
      std::move(pinch_original_events));
  pinch_event->set_coalesced_scroll_and_pinch();

  queue_.push_back(std::move(scroll_event));
  queue_.push_back(std::move(pinch_event));
}

std::unique_ptr<EventWithCallback> CompositorThreadEventQueue::Pop() {
  DCHECK(!queue_.empty());
  std::unique_ptr<EventWithCallback> result = std::move(queue_.front());
  queue_.pop_front();

  if (result->first_original_event()) {
    TRACE_EVENT_NESTABLE_ASYNC_END2(
        "input", "CompositorThreadEventQueue::Queue",
        result->first_original_event(), "type", result->event().GetType(),
        "coalesced_count", result->coalesced_count());
  }
  return result;
}

WebInputEvent::Type CompositorThreadEventQueue::PeekType() const {
  return empty() ? WebInputEvent::Type::kUndefined
                 : queue_.front()->event().GetType();
}

}  // namespace blink

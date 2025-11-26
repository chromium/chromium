// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/widget/input/compositor_thread_event_queue.h"

#include "base/trace_event/trace_event.h"
#include "cc/metrics/event_metrics.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/perfetto/include/perfetto/tracing/track.h"

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

// Coalesces a series of GestureScrollUpdate and GesturePinchUpdate events
// into a single scroll and a single pinch.
std::pair<std::unique_ptr<EventWithCallback>,
          std::unique_ptr<EventWithCallback>>
CoalesceScrollAndPinchEvents(
    std::unique_ptr<EventWithCallback> second_last_event,
    std::unique_ptr<EventWithCallback> last_event,
    std::unique_ptr<EventWithCallback> new_event) {
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

  SetScrollOrPinchTraceId(last_event.get(), &oldest_scroll_trace_id,
                          &oldest_pinch_trace_id);
  oldest_latency = last_event->latency_info();
  EventWithCallback::OriginalEventList combined_original_events;
  combined_original_events.splice(combined_original_events.end(),
                                  last_event->original_events());
  combined_original_events.splice(combined_original_events.end(),
                                  new_event->original_events());

  if (second_last_event) {
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

  return {std::move(scroll_event), std::move(pinch_event)};
}

}  // namespace

CompositorThreadEventQueue::CompositorThreadEventQueue() = default;

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
  if (base::FeatureList::IsEnabled(
          features::kRefactorCompositorThreadEventQueue)) {
    if (new_event->first_original_event()) {
      // Trace could be nested as there might be multiple events in queue.
      // e.g. |ScrollUpdate|, |ScrollEnd|, and another scroll sequence.
      TRACE_EVENT_BEGIN(
          "input", "CompositorThreadEventQueue::Queue",
          perfetto::Track::FromPointer(new_event->first_original_event()));
    }
    queue_.push_back(std::move(new_event));
  } else {
    if (queue_.empty() ||
        !IsContinuousGestureEvent(new_event->event().GetType()) ||
        !(queue_.back()->CanCoalesceWith(*new_event) ||
          WebGestureEvent::IsCompatibleScrollorPinch(
              ToWebGestureEvent(new_event->event()),
              ToWebGestureEvent(queue_.back()->event())))) {
      if (new_event->first_original_event()) {
        // Trace could be nested as there might be multiple events in queue.
        // e.g. |ScrollUpdate|, |ScrollEnd|, and another scroll sequence.
        TRACE_EVENT_BEGIN(
            "input", "CompositorThreadEventQueue::Queue",
            perfetto::Track::FromPointer(new_event->first_original_event()));
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
    // |last_event|, but there will be only one LatencyInfo that should be
    // traced for two events. In this case we will output an empty LatencyInfo.
    //
    // However with two events one will be a GesturePinchUpdate and one will be
    // a GestureScrollUpdate and we will use the two non-coalesced event's
    // trace_ids to instrument the flow through the system.
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

    // Extract the second last event in queue IF it's a scroll or a pinch for
    // the same target.
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
    // original non-coalesced event. If the event was artificially created (I.E
    // it sprung into existence in CoalesceScrollAndPinch and isn't associated
    // with a WebInputEvent that was in the queue) we will give it an empty
    // LatencyInfo (so it won't have anything reported for it). This can be seen
    // when a trace_id is equal to -1. We also move the original events into
    // whichever one is the original non-coalesced event, defaulting to the
    // pinch event if both are non-coalesced versions so it runs last.
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
    std::pair<std::unique_ptr<WebGestureEvent>,
              std::unique_ptr<WebGestureEvent>>
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
}

std::unique_ptr<EventWithCallback> CompositorThreadEventQueue::Pop() {
  DCHECK(!queue_.empty());
  // Pop should only be called during the event dispatch phase,
  // not when a backlog is waiting to be coalesced.
  DCHECK_EQ(backlog_count_, 0ul);
  std::unique_ptr<EventWithCallback> result = std::move(queue_.front());
  queue_.pop_front();

  if (events_to_always_dispatch_ > 0) {
    events_to_always_dispatch_--;
  }

  if (result->first_original_event()) {
    TRACE_EVENT_END(
        "input", perfetto::Track::FromPointer(result->first_original_event()),
        "result", "dispatched", "type", result->event().GetType(),
        "coalesced_count", result->coalesced_count());
  }
  return result;
}

void CompositorThreadEventQueue::DidFinishDispatch() {
  backlog_count_ = queue_.size();
  if (backlog_count_) {
    TRACE_EVENT_INSTANT1(
        "input", "CompositorThreadEventQueue::DidFinishDispatch",
        TRACE_EVENT_SCOPE_THREAD, "backlog_count", backlog_count_);
  }
}

void CompositorThreadEventQueue::CoalesceEvents(base::TimeTicks sample_time) {
  events_to_always_dispatch_ = 0;
  if (queue_.empty()) {
    return;
  }

  auto backlog_end_it = queue_.begin();
  if (backlog_count_ < queue_.size()) {
    std::advance(backlog_end_it, backlog_count_);
  } else {
    backlog_end_it = queue_.end();
  }
  backlog_count_ = 0;  // Reset backlog for the next DidFinishDispatch call

  // Find the time boundary only within the new events (after the backlog).
  // Assumes new events are roughly time ordered.
  auto time_boundary_it =
      std::upper_bound(backlog_end_it, queue_.end(), sample_time,
                       [](base::TimeTicks time,
                          const std::unique_ptr<EventWithCallback>& event) {
                         return time < event->event().TimeStamp();
                       });

  // |batch_end_it| is |time_boundary_it| because this iterator marks the
  // end of the combined range of backlogged events and the new events
  // up to sample_time.
  auto batch_end_it = time_boundary_it;

  if (batch_end_it == queue_.begin()) {
    return;
  }

  // Move the events to be processed into a temporary queue.
  EventQueue processing_queue;
  processing_queue.insert(processing_queue.end(),
                          std::make_move_iterator(queue_.begin()),
                          std::make_move_iterator(batch_end_it));
  queue_.erase(queue_.begin(), batch_end_it);

  // Coalesce the events from the processing queue into a new queue.
  EventQueue coalesced_queue;
  while (!processing_queue.empty()) {
    std::unique_ptr<EventWithCallback> new_event =
        std::move(processing_queue.front());
    processing_queue.pop_front();

    if (coalesced_queue.empty()) {
      coalesced_queue.push_back(std::move(new_event));
      continue;
    }

    auto& last_event_ref = coalesced_queue.back();
    bool can_coalesce_with_last =
        IsContinuousGestureEvent(new_event->event().GetType()) &&
        (last_event_ref->CanCoalesceWith(*new_event) ||
         WebGestureEvent::IsCompatibleScrollorPinch(
             ToWebGestureEvent(new_event->event()),
             ToWebGestureEvent(last_event_ref->event())));

    if (!can_coalesce_with_last) {
      coalesced_queue.push_back(std::move(new_event));
      continue;
    }

    // End the trace for an event that is being consumed by coalescing. This
    // closes the slice that was started in `Queue()` and prevents dangling
    // traces. The "result":"coalesced" argument explicitly marks that this
    // event was consumed by the coalescing logic.
    if (new_event->first_original_event()) {
      TRACE_EVENT_END(
          "input",
          perfetto::Track::FromPointer(new_event->first_original_event()),
          "result", "coalesced", "type", new_event->event().GetType());
    }

    if (last_event_ref->CanCoalesceWith(*new_event)) {
      last_event_ref->CoalesceWith(new_event.get());
      continue;
    }

    // Scroll-and-pinch coalescing.
    std::unique_ptr<EventWithCallback> last_event =
        std::move(coalesced_queue.back());
    coalesced_queue.pop_back();

    std::unique_ptr<EventWithCallback> second_last_event;
    if (!coalesced_queue.empty()) {
      auto& second_last_ref = coalesced_queue.back();
      if (WebGestureEvent::IsCompatibleScrollorPinch(
              ToWebGestureEvent(new_event->event()),
              ToWebGestureEvent(second_last_ref->event()))) {
        second_last_event = std::move(coalesced_queue.back());
        coalesced_queue.pop_back();
      }
    }

    auto coalesced_pair = CoalesceScrollAndPinchEvents(
        std::move(second_last_event), std::move(last_event),
        std::move(new_event));

    coalesced_queue.push_back(std::move(coalesced_pair.first));
    coalesced_queue.push_back(std::move(coalesced_pair.second));
  }

  // Update State
  events_to_always_dispatch_ = coalesced_queue.size();

  // Insert the coalesced events back at the front of the main queue.
  queue_.insert(queue_.begin(),
                std::make_move_iterator(coalesced_queue.begin()),
                std::make_move_iterator(coalesced_queue.end()));
}

bool CompositorThreadEventQueue::IsNextEventReady(
    base::TimeTicks sample_time) const {
  if (queue_.empty()) {
    return false;
  }
  if (events_to_always_dispatch_ > 0) {
    return true;
  }
  return queue_.front()->event().TimeStamp() <= sample_time;
}

WebInputEvent::Type CompositorThreadEventQueue::PeekType() const {
  return empty() ? WebInputEvent::Type::kUndefined
                 : queue_.front()->event().GetType();
}

base::TimeTicks CompositorThreadEventQueue::PeekTimestamp() const {
  return empty() ? base::TimeTicks() : queue_.front()->event().TimeStamp();
}

const WebInputEvent* CompositorThreadEventQueue::FirstOriginalEvent() const {
  return empty() ? nullptr : queue_.front()->first_original_event();
}

}  // namespace blink

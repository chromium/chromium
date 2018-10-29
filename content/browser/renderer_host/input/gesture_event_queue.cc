// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/gesture_event_queue.h"

#include "base/auto_reset.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/renderer_host/input/touchpad_tap_suppression_controller.h"
#include "content/browser/renderer_host/input/touchscreen_tap_suppression_controller.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/web_input_event_traits.h"

using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace content {

GestureEventQueue::GestureEventWithLatencyInfoAndAckState::
    GestureEventWithLatencyInfoAndAckState(
        const GestureEventWithLatencyInfo& event)
    : GestureEventWithLatencyInfo(event) {}

GestureEventQueue::Config::Config() {
}

GestureEventQueue::GestureEventQueue(
    GestureEventQueueClient* client,
    FlingControllerEventSenderClient* fling_event_sender_client,
    FlingControllerSchedulerClient* fling_scheduler_client,
    const Config& config)
    : client_(client),
      scrolling_in_progress_(false),
      ignore_next_ack_(false),
      allow_multiple_inflight_events_(
          base::FeatureList::IsEnabled(features::kVsyncAlignedInputEvents)),
      debounce_interval_(config.debounce_interval),
      fling_controller_(fling_event_sender_client,
                        fling_scheduler_client,
                        config.fling_config) {
  DCHECK(client);
  DCHECK(fling_event_sender_client);
  DCHECK(fling_scheduler_client);
}

GestureEventQueue::~GestureEventQueue() { }

bool GestureEventQueue::DebounceOrQueueEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  // GFS and GFC should have been filtered in FlingControllerFilterEvent.
  DCHECK_NE(gesture_event.event.GetType(), WebInputEvent::kGestureFlingStart);
  DCHECK_NE(gesture_event.event.GetType(), WebInputEvent::kGestureFlingCancel);
  if (!ShouldForwardForBounceReduction(gesture_event))
    return false;

  QueueAndForwardIfNecessary(gesture_event);
  return true;
}

bool GestureEventQueue::FlingControllerFilterEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  TRACE_EVENT0("input", "GestureEventQueue::QueueEvent");
  if (fling_controller_.FilterGestureEvent(gesture_event))
    return true;

  // fling_controller_ is in charge of handling GFS events and the events are
  // not sent to the renderer, the controller processes the fling and generates
  // fling progress events (wheel events for touchpad and GSU events for
  // touchscreen and autoscroll) which are handled normally.
  if (gesture_event.event.GetType() == WebInputEvent::kGestureFlingStart) {
    fling_controller_.ProcessGestureFlingStart(gesture_event);
    return true;
  }

  // If the GestureFlingStart event is processed by the fling_controller_, the
  // GestureFlingCancel event should be the same.
  if (gesture_event.event.GetType() == WebInputEvent::kGestureFlingCancel) {
    fling_controller_.ProcessGestureFlingCancel(gesture_event);
    return true;
  }

  return false;
}

void GestureEventQueue::StopFling() {
  fling_controller_.StopFling();
}

bool GestureEventQueue::FlingCancellationIsDeferred() const {
  return fling_controller_.FlingCancellationIsDeferred();
}

gfx::Vector2dF GestureEventQueue::CurrentFlingVelocity() const {
  return fling_controller_.CurrentFlingVelocity();
}

bool GestureEventQueue::FlingInProgressForTest() const {
  return fling_controller_.fling_in_progress();
}

bool GestureEventQueue::ShouldForwardForBounceReduction(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (debounce_interval_ <= base::TimeDelta())
    return true;

  // Don't debounce any gesture events while a fling is in progress on the
  // browser side. A GSE event in this case ends fling progress and it shouldn't
  // get cancelled by its next GSB event.
  if (fling_controller_.fling_in_progress())
    return true;

  switch (gesture_event.event.GetType()) {
    case WebInputEvent::kGestureScrollUpdate:
      if (!scrolling_in_progress_) {
        debounce_deferring_timer_.Start(
            FROM_HERE,
            debounce_interval_,
            this,
            &GestureEventQueue::SendScrollEndingEventsNow);
      } else {
        // Extend the bounce interval.
        debounce_deferring_timer_.Reset();
      }
      scrolling_in_progress_ = true;
      debouncing_deferral_queue_.clear();
      return true;

    case WebInputEvent::kGesturePinchBegin:
    case WebInputEvent::kGesturePinchEnd:
    case WebInputEvent::kGesturePinchUpdate:
      // TODO(rjkroege): Debounce pinch (http://crbug.com/147647)
      return true;
    default:
      if (scrolling_in_progress_) {
        debouncing_deferral_queue_.push_back(gesture_event);
        return false;
      }
      return true;
  }
}

void GestureEventQueue::QueueAndForwardIfNecessary(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (allow_multiple_inflight_events_) {
    // GFS and GFC should have been filtered in FlingControllerFilterEvent to
    // get handled by fling controller.
    DCHECK_NE(gesture_event.event.GetType(), WebInputEvent::kGestureFlingStart);
    DCHECK_NE(gesture_event.event.GetType(),
              WebInputEvent::kGestureFlingCancel);
    coalesced_gesture_events_.push_back(gesture_event);
    client_->SendGestureEventImmediately(gesture_event);
    return;
  }

  switch (gesture_event.event.GetType()) {
    case WebInputEvent::kGesturePinchUpdate:
    case WebInputEvent::kGestureScrollUpdate:
      QueueScrollOrPinchAndForwardIfNecessary(gesture_event);
      return;
    case WebInputEvent::kGestureScrollBegin:
      if (OnScrollBegin(gesture_event))
        return;
      break;
    default:
      break;
  }

  coalesced_gesture_events_.push_back(gesture_event);
  if (coalesced_gesture_events_.size() == 1)
    client_->SendGestureEventImmediately(gesture_event);
}

bool GestureEventQueue::OnScrollBegin(
    const GestureEventWithLatencyInfo& gesture_event) {
  // If a synthetic scroll begin is encountered, it can cancel out a previous
  // synthetic scroll end. This allows a later gesture scroll update to coalesce
  // with the previous one. crbug.com/607340.
  bool synthetic = gesture_event.event.data.scroll_begin.synthetic;
  bool have_unsent_events =
      EventsInFlightCount() < coalesced_gesture_events_.size();
  if (synthetic && have_unsent_events) {
    GestureEventWithLatencyInfo* last_event = &coalesced_gesture_events_.back();
    if (last_event->event.GetType() == WebInputEvent::kGestureScrollEnd &&
        last_event->event.data.scroll_end.synthetic) {
      coalesced_gesture_events_.pop_back();
      return true;
    }
  }
  return false;
}

void GestureEventQueue::ProcessGestureAck(InputEventAckSource ack_source,
                                          InputEventAckState ack_result,
                                          WebInputEvent::Type type,
                                          const ui::LatencyInfo& latency) {
  TRACE_EVENT0("input", "GestureEventQueue::ProcessGestureAck");

  if (coalesced_gesture_events_.empty()) {
    DLOG(ERROR) << "Received unexpected ACK for event type " << type;
    return;
  }

  if (!allow_multiple_inflight_events_) {
    LegacyProcessGestureAck(ack_source, ack_result, type, latency);
    return;
  }

  // ACKs could come back out of order. We want to cache them to restore the
  // original order.
  for (auto& outstanding_event : coalesced_gesture_events_) {
    if (outstanding_event.ack_state() != INPUT_EVENT_ACK_STATE_UNKNOWN)
      continue;
    if (outstanding_event.event.GetType() == type) {
      outstanding_event.latency.AddNewLatencyFrom(latency);
      outstanding_event.set_ack_info(ack_source, ack_result);
      break;
    }
  }

  AckCompletedEvents();
}

void GestureEventQueue::AckCompletedEvents() {
  // Don't allow re-entrancy into this method otherwise
  // the ordering of acks won't be preserved.
  if (processing_acks_)
    return;
  base::AutoReset<bool> process_acks(&processing_acks_, true);
  while (!coalesced_gesture_events_.empty()) {
    auto iter = coalesced_gesture_events_.begin();
    if (iter->ack_state() == INPUT_EVENT_ACK_STATE_UNKNOWN)
      break;
    GestureEventWithLatencyInfoAndAckState event = *iter;
    coalesced_gesture_events_.erase(iter);
    AckGestureEventToClient(event, event.ack_source(), event.ack_state());
  }
}

void GestureEventQueue::AckGestureEventToClient(
    const GestureEventWithLatencyInfo& event_with_latency,
    InputEventAckSource ack_source,
    InputEventAckState ack_result) {
  DCHECK(allow_multiple_inflight_events_);

  // Ack'ing an event may enqueue additional gesture events.  By ack'ing the
  // event before the forwarding of queued events below, such additional events
  // can be coalesced with existing queued events prior to dispatch.
  client_->OnGestureEventAck(event_with_latency, ack_source, ack_result);

  fling_controller_.OnGestureEventAck(event_with_latency, ack_result);
}

void GestureEventQueue::LegacyProcessGestureAck(
    InputEventAckSource ack_source,
    InputEventAckState ack_result,
    WebInputEvent::Type type,
    const ui::LatencyInfo& latency) {
  DCHECK(!allow_multiple_inflight_events_);

  // Events are forwarded one-by-one.
  // It's possible that the ack for the second event in an in-flight,
  // coalesced Gesture{Scroll,Pinch}Update pair is received prior to the first
  // event ack.
  size_t event_index = 0;
  if (ignore_next_ack_ && coalesced_gesture_events_.size() > 1 &&
      coalesced_gesture_events_[0].event.GetType() != type &&
      coalesced_gesture_events_[1].event.GetType() == type) {
    event_index = 1;
  }

  GestureEventWithLatencyInfo event_with_latency =
      coalesced_gesture_events_[event_index];
  DCHECK_EQ(event_with_latency.event.GetType(), type);
  event_with_latency.latency.AddNewLatencyFrom(latency);

  // Ack'ing an event may enqueue additional gesture events.  By ack'ing the
  // event before the forwarding of queued events below, such additional events
  // can be coalesced with existing queued events prior to dispatch.
  client_->OnGestureEventAck(event_with_latency, ack_source, ack_result);

  fling_controller_.OnGestureEventAck(event_with_latency, ack_result);

  DCHECK_LT(event_index, coalesced_gesture_events_.size());
  coalesced_gesture_events_.erase(coalesced_gesture_events_.begin() +
                                  event_index);

  if (ignore_next_ack_) {
    ignore_next_ack_ = false;
    return;
  }

  if (coalesced_gesture_events_.empty())
    return;

  const GestureEventWithLatencyInfo& first_gesture_event =
      coalesced_gesture_events_.front();

  // Check for the coupled GesturePinchUpdate before sending either event,
  // handling the case where the first GestureScrollUpdate ack is synchronous.
  GestureEventWithLatencyInfo second_gesture_event;
  if (first_gesture_event.event.GetType() ==
          WebInputEvent::kGestureScrollUpdate &&
      coalesced_gesture_events_.size() > 1 &&
      coalesced_gesture_events_[1].event.GetType() ==
          WebInputEvent::kGesturePinchUpdate) {
    second_gesture_event = coalesced_gesture_events_[1];
    ignore_next_ack_ = true;
  }

  client_->SendGestureEventImmediately(first_gesture_event);
  if (second_gesture_event.event.GetType() != WebInputEvent::kUndefined)
    client_->SendGestureEventImmediately(second_gesture_event);
}

TouchpadTapSuppressionController*
    GestureEventQueue::GetTouchpadTapSuppressionController() {
  return fling_controller_.GetTouchpadTapSuppressionController();
}

void GestureEventQueue::ForwardGestureEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  QueueAndForwardIfNecessary(gesture_event);
}

void GestureEventQueue::SendScrollEndingEventsNow() {
  scrolling_in_progress_ = false;
  if (debouncing_deferral_queue_.empty())
    return;
  GestureQueue debouncing_deferral_queue;
  debouncing_deferral_queue.swap(debouncing_deferral_queue_);
  for (GestureQueue::const_iterator it = debouncing_deferral_queue.begin();
       it != debouncing_deferral_queue.end(); it++) {
    if (!fling_controller_.FilterGestureEvent(*it)) {
      QueueAndForwardIfNecessary(*it);
    }
  }
}

void GestureEventQueue::QueueScrollOrPinchAndForwardIfNecessary(
    const GestureEventWithLatencyInfo& gesture_event) {
  DCHECK_GE(coalesced_gesture_events_.size(), EventsInFlightCount());
  const size_t unsent_events_count =
      coalesced_gesture_events_.size() - EventsInFlightCount();
  if (!unsent_events_count) {
    coalesced_gesture_events_.push_back(gesture_event);
    if (coalesced_gesture_events_.size() == 1) {
      client_->SendGestureEventImmediately(gesture_event);
    } else if (coalesced_gesture_events_.size() == 2) {
      DCHECK(!ignore_next_ack_);
      // If there is an in-flight scroll, the new pinch can be forwarded
      // immediately, avoiding a potential frame delay between the two
      // (similarly for an in-flight pinch with a new scroll).
      const GestureEventWithLatencyInfo& first_event =
          coalesced_gesture_events_.front();
      if (gesture_event.event.GetType() != first_event.event.GetType() &&
          ui::IsCompatibleScrollorPinch(gesture_event.event,
                                        first_event.event)) {
        ignore_next_ack_ = true;
        client_->SendGestureEventImmediately(gesture_event);
      }
    }
    return;
  }

  GestureEventWithLatencyInfo* last_event = &coalesced_gesture_events_.back();
  if (last_event->CanCoalesceWith(gesture_event)) {
    last_event->CoalesceWith(gesture_event);
    return;
  }

  if (!ui::IsCompatibleScrollorPinch(gesture_event.event, last_event->event)) {
    coalesced_gesture_events_.push_back(gesture_event);
    return;
  }

  // Extract the last event in queue.
  blink::WebGestureEvent last_gesture_event =
      coalesced_gesture_events_.back().event;
  DCHECK_LE(coalesced_gesture_events_.back().latency.trace_id(),
            gesture_event.latency.trace_id());
  ui::LatencyInfo oldest_latency = coalesced_gesture_events_.back().latency;
  oldest_latency.set_coalesced();
  coalesced_gesture_events_.pop_back();

  // Extract the second last event in queue.
  ui::WebScopedInputEvent second_last_gesture_event = nullptr;
  if (unsent_events_count > 1 &&
      ui::IsCompatibleScrollorPinch(gesture_event.event,
                                    coalesced_gesture_events_.back().event)) {
    second_last_gesture_event =
        ui::WebInputEventTraits::Clone(coalesced_gesture_events_.back().event);
    DCHECK_LE(coalesced_gesture_events_.back().latency.trace_id(),
              oldest_latency.trace_id());
    oldest_latency = coalesced_gesture_events_.back().latency;
    oldest_latency.set_coalesced();
    coalesced_gesture_events_.pop_back();
  }

  std::pair<blink::WebGestureEvent, blink::WebGestureEvent> coalesced_events =
      ui::CoalesceScrollAndPinch(
          second_last_gesture_event
              ? &ui::ToWebGestureEvent(*second_last_gesture_event)
              : nullptr,
          last_gesture_event, gesture_event.event);

  GestureEventWithLatencyInfo scroll_event;
  scroll_event.event = coalesced_events.first;
  scroll_event.latency = oldest_latency;

  GestureEventWithLatencyInfo pinch_event;
  pinch_event.event = coalesced_events.second;
  pinch_event.latency = oldest_latency;

  coalesced_gesture_events_.push_back(scroll_event);
  coalesced_gesture_events_.push_back(pinch_event);
}

size_t GestureEventQueue::EventsInFlightCount() const {
  if (allow_multiple_inflight_events_) {
    // Currently unused, can be removed if compositor event queue was enabled by
    // default.
    NOTREACHED();
    return coalesced_gesture_events_.size();
  }

  if (coalesced_gesture_events_.empty())
    return 0;

  if (!ignore_next_ack_)
    return 1;

  DCHECK_GT(coalesced_gesture_events_.size(), 1U);
  return 2;
}

}  // namespace content

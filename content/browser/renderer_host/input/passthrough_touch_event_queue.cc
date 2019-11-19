// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/passthrough_touch_event_queue.h"

#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/renderer_host/input/touch_timeout_handler.h"
#include "content/common/input/web_touch_event_traits.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/point_f.h"

using blink::WebInputEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using ui::LatencyInfo;

namespace content {
namespace {

// Compare all properties of touch points to determine the state.
bool HasPointChanged(const WebTouchPoint& point_1,
                     const WebTouchPoint& point_2) {
  DCHECK_EQ(point_1.id, point_2.id);
  if (point_1.PositionInScreen() != point_2.PositionInScreen() ||
      point_1.PositionInWidget() != point_2.PositionInWidget() ||
      point_1.radius_x != point_2.radius_x ||
      point_1.radius_y != point_2.radius_y ||
      point_1.rotation_angle != point_2.rotation_angle ||
      point_1.force != point_2.force || point_1.tilt_x != point_2.tilt_x ||
      point_1.tilt_y != point_2.tilt_y) {
    return true;
  }
  return false;
}

}  // namespace

// static
const base::FeatureParam<std::string>
    PassthroughTouchEventQueue::kSkipTouchEventFilterType{
        &features::kSkipTouchEventFilter,
        features::kSkipTouchEventFilterTypeParamName,
        features::kSkipTouchEventFilterTypeParamValueDiscrete};

PassthroughTouchEventQueue::TouchEventWithLatencyInfoAndAckState::
    TouchEventWithLatencyInfoAndAckState(const TouchEventWithLatencyInfo& event)
    : TouchEventWithLatencyInfo(event),
      ack_state_(INPUT_EVENT_ACK_STATE_UNKNOWN) {}

bool PassthroughTouchEventQueue::TouchEventWithLatencyInfoAndAckState::
operator<(const TouchEventWithLatencyInfoAndAckState& other) const {
  return event.unique_touch_event_id < other.event.unique_touch_event_id;
}

PassthroughTouchEventQueue::PassthroughTouchEventQueue(
    PassthroughTouchEventQueueClient* client,
    const Config& config)
    : client_(client),
      has_handlers_(true),
      maybe_has_handler_for_current_sequence_(false),
      drop_remaining_touches_in_sequence_(false),
      send_touch_events_async_(false),
      processing_acks_(false),
      skip_touch_filter_(config.skip_touch_filter),
      events_to_always_forward_(config.events_to_always_forward) {
  if (config.touch_ack_timeout_supported) {
    timeout_handler_.reset(
        new TouchTimeoutHandler(this, config.desktop_touch_ack_timeout_delay,
                                config.mobile_touch_ack_timeout_delay));
  }
}

PassthroughTouchEventQueue::~PassthroughTouchEventQueue() {}

void PassthroughTouchEventQueue::SendTouchCancelEventForTouchEvent(
    const TouchEventWithLatencyInfo& event_to_cancel) {
  TouchEventWithLatencyInfo event = event_to_cancel;
  WebTouchEventTraits::ResetTypeAndTouchStates(
      WebInputEvent::kTouchCancel,
      // TODO(rbyers): Shouldn't we use a fresh timestamp?
      event.event.TimeStamp(), &event.event);
  SendTouchEventImmediately(&event, false);
}

void PassthroughTouchEventQueue::QueueEvent(
    const TouchEventWithLatencyInfo& event) {
  TRACE_EVENT0("input", "PassthroughTouchEventQueue::QueueEvent");
  PreFilterResult filter_result = FilterBeforeForwarding(event.event);
  bool should_forward_touch_event =
      filter_result == PreFilterResult::kUnfiltered;
  UMA_HISTOGRAM_ENUMERATION("Event.Touch.FilteredAtPassthroughQueue",
                            filter_result);

  if (!should_forward_touch_event) {
    client_->OnFilteringTouchEvent(event.event);

    TouchEventWithLatencyInfoAndAckState event_with_ack_state = event;
    event_with_ack_state.set_ack_info(InputEventAckSource::BROWSER,
                                      INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
    outstanding_touches_.insert(event_with_ack_state);
    AckCompletedEvents();
    return;
  }
  TouchEventWithLatencyInfo cloned_event(event);
  SendTouchEventImmediately(&cloned_event, true);
}

void PassthroughTouchEventQueue::PrependTouchScrollNotification() {
  TRACE_EVENT0("input",
               "PassthroughTouchEventQueue::PrependTouchScrollNotification");

  TouchEventWithLatencyInfo touch(WebInputEvent::kTouchScrollStarted,
                                  WebInputEvent::kNoModifiers,
                                  ui::EventTimeForNow(), LatencyInfo());
  touch.event.dispatch_type = WebInputEvent::kEventNonBlocking;
  SendTouchEventImmediately(&touch, true);
}

void PassthroughTouchEventQueue::ProcessTouchAck(
    InputEventAckSource ack_source,
    InputEventAckState ack_result,
    const LatencyInfo& latency_info,
    const uint32_t unique_touch_event_id,
    bool should_stop_timeout_monitor) {
  TRACE_EVENT0("input", "PassthroughTouchEventQueue::ProcessTouchAck");
  if (timeout_handler_ &&
      timeout_handler_->ConfirmTouchEvent(unique_touch_event_id, ack_result,
                                          should_stop_timeout_monitor))
    return;

  auto touch_event_iter = outstanding_touches_.begin();
  while (touch_event_iter != outstanding_touches_.end()) {
    if (unique_touch_event_id == touch_event_iter->event.unique_touch_event_id)
      break;
    ++touch_event_iter;
  }

  if (touch_event_iter == outstanding_touches_.end())
    return;

  TouchEventWithLatencyInfoAndAckState& event =
      const_cast<TouchEventWithLatencyInfoAndAckState&>(*touch_event_iter);
  event.latency.AddNewLatencyFrom(latency_info);
  event.set_ack_info(ack_source, ack_result);

  AckCompletedEvents();
}

void PassthroughTouchEventQueue::OnGestureScrollEvent(
    const GestureEventWithLatencyInfo& gesture_event) {
  if (gesture_event.event.GetType() ==
          blink::WebInputEvent::kGestureScrollUpdate &&
      gesture_event.event.resending_plugin_id == -1) {
    send_touch_events_async_ = true;
  }
}

void PassthroughTouchEventQueue::OnGestureEventAck(
    const GestureEventWithLatencyInfo& event,
    InputEventAckState ack_result) {
  // Turn events sent during gesture scrolls to be async.
  if (event.event.GetType() == blink::WebInputEvent::kGestureScrollUpdate &&
      event.event.resending_plugin_id == -1) {
    send_touch_events_async_ = (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED);
  }
}

void PassthroughTouchEventQueue::OnHasTouchEventHandlers(bool has_handlers) {
  has_handlers_ = has_handlers;
}

bool PassthroughTouchEventQueue::IsPendingAckTouchStart() const {
  if (outstanding_touches_.empty())
    return false;

  for (auto& iter : outstanding_touches_) {
    if (iter.event.GetType() == WebInputEvent::kTouchStart)
      return true;
  }
  return false;
}

void PassthroughTouchEventQueue::SetAckTimeoutEnabled(bool enabled) {
  if (timeout_handler_)
    timeout_handler_->SetEnabled(enabled);
}

void PassthroughTouchEventQueue::SetIsMobileOptimizedSite(
    bool mobile_optimized_site) {
  if (timeout_handler_)
    timeout_handler_->SetUseMobileTimeout(mobile_optimized_site);
}

bool PassthroughTouchEventQueue::IsAckTimeoutEnabled() const {
  return timeout_handler_ && timeout_handler_->IsEnabled();
}

bool PassthroughTouchEventQueue::Empty() const {
  return outstanding_touches_.empty();
}

void PassthroughTouchEventQueue::FlushQueue() {
  // Don't allow acks to be processed in AckCompletedEvents as that can
  // interfere with gesture event dispatch ordering.
  base::AutoReset<bool> process_acks(&processing_acks_, true);
  drop_remaining_touches_in_sequence_ = true;
  client_->FlushDeferredGestureQueue();
  while (!outstanding_touches_.empty()) {
    auto iter = outstanding_touches_.begin();
    TouchEventWithLatencyInfoAndAckState event = *iter;
    outstanding_touches_.erase(iter);
    if (event.ack_state() == INPUT_EVENT_ACK_STATE_UNKNOWN)
      event.set_ack_info(InputEventAckSource::BROWSER,
                         INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS);
    AckTouchEventToClient(event, event.ack_source(), event.ack_state());
  }
}

void PassthroughTouchEventQueue::StopTimeoutMonitor() {
  if (timeout_handler_)
    timeout_handler_->StopTimeoutMonitor();
}

void PassthroughTouchEventQueue::AckCompletedEvents() {
  // Don't allow re-entrancy into this method otherwise
  // the ordering of acks won't be preserved.
  if (processing_acks_)
    return;
  base::AutoReset<bool> process_acks(&processing_acks_, true);
  while (!outstanding_touches_.empty()) {
    auto iter = outstanding_touches_.begin();
    if (iter->ack_state() == INPUT_EVENT_ACK_STATE_UNKNOWN)
      break;
    TouchEventWithLatencyInfoAndAckState event = *iter;
    outstanding_touches_.erase(iter);
    AckTouchEventToClient(event, event.ack_source(), event.ack_state());
  }
}

void PassthroughTouchEventQueue::AckTouchEventToClient(
    const TouchEventWithLatencyInfo& acked_event,
    InputEventAckSource ack_source,
    InputEventAckState ack_result) {
  UpdateTouchConsumerStates(acked_event.event, ack_result);

  // Skip ack for TouchScrollStarted since it was synthesized within the queue.
  if (acked_event.event.GetType() != WebInputEvent::kTouchScrollStarted) {
    client_->OnTouchEventAck(acked_event, ack_source, ack_result);
  }
}

void PassthroughTouchEventQueue::SendTouchEventImmediately(
    TouchEventWithLatencyInfo* touch,
    bool wait_for_ack) {
  // Note: Touchstart events are marked cancelable to allow transitions between
  // platform scrolling and JS pinching. Touchend events, however, remain
  // uncancelable, mitigating the risk of jank when transitioning to a fling.
  if (send_touch_events_async_ &&
      touch->event.GetType() != WebInputEvent::kTouchStart)
    touch->event.dispatch_type = WebInputEvent::kEventNonBlocking;

  if (touch->event.GetType() == WebInputEvent::kTouchStart)
    touch->event.touch_start_or_first_touch_move = true;

  // For touchmove events, compare touch points position from current event
  // to last sent event and update touch points state.
  if (touch->event.GetType() == WebInputEvent::kTouchMove) {
    CHECK(last_sent_touchevent_);
    if (last_sent_touchevent_->GetType() == WebInputEvent::kTouchStart)
      touch->event.touch_start_or_first_touch_move = true;
    for (unsigned int i = 0; i < last_sent_touchevent_->touches_length; ++i) {
      const WebTouchPoint& last_touch_point = last_sent_touchevent_->touches[i];
      // Touches with same id may not have same index in Touches array.
      for (unsigned int j = 0; j < touch->event.touches_length; ++j) {
        const WebTouchPoint& current_touchmove_point = touch->event.touches[j];
        if (current_touchmove_point.id != last_touch_point.id)
          continue;

        if (!HasPointChanged(last_touch_point, current_touchmove_point))
          touch->event.touches[j].state = WebTouchPoint::kStateStationary;

        break;
      }
    }
  }

  if (touch->event.GetType() != WebInputEvent::kTouchScrollStarted) {
    if (last_sent_touchevent_)
      *last_sent_touchevent_ = touch->event;
    else
      last_sent_touchevent_.reset(new WebTouchEvent(touch->event));
  }

  if (timeout_handler_)
    timeout_handler_->StartIfNecessary(*touch);
  if (wait_for_ack)
    outstanding_touches_.insert(*touch);
  client_->SendTouchEventImmediately(*touch);
}

PassthroughTouchEventQueue::PreFilterResult
PassthroughTouchEventQueue::FilterBeforeForwarding(const WebTouchEvent& event) {
  PreFilterResult result = FilterBeforeForwardingImpl(event);
  if (result == PreFilterResult::kFilteredTimeout ||
      result == PreFilterResult::kFilteredNoNonstationaryPointers)
    return result;

  // Override non-timeout filter results based on the Finch trial that bypasses
  // the filter. We do this here so that the event still has the opportunity to
  // update any internal state that's necessary to handle future events
  // (i.e. future touch moves might be dropped, even if this touch start isn't
  // due to a filter override).
  if (!ShouldFilterForEvent(event))
    return PreFilterResult::kUnfiltered;

  return result;
}

bool PassthroughTouchEventQueue::ShouldFilterForEvent(
    const blink::WebTouchEvent& event) {
  // Always run all filtering if the SkipTouchEventFilter is disabled.
  if (!skip_touch_filter_)
    return true;
  // If the experiment is enabled and all events are forwarded, always skip
  // filtering.
  if (events_to_always_forward_ ==
      features::kSkipTouchEventFilterTypeParamValueAll)
    return false;
  // If the experiment is enabled and only discrete events are forwarded,
  // always run filtering for touchmove events only.
  return event.GetType() == WebInputEvent::kTouchMove;
}

PassthroughTouchEventQueue::PreFilterResult
PassthroughTouchEventQueue::FilterBeforeForwardingImpl(
    const WebTouchEvent& event) {
  // Unconditionally apply the timeout filter to avoid exacerbating
  // any responsiveness problems on the page.
  if (timeout_handler_ && timeout_handler_->FilterEvent(event))
    return PreFilterResult::kFilteredTimeout;

  if (event.GetType() == WebInputEvent::kTouchScrollStarted)
    return PreFilterResult::kUnfiltered;

  if (WebTouchEventTraits::IsTouchSequenceStart(event)) {
    // We don't know if we have a handler until we get the ACK back so
    // assume it is true.
    maybe_has_handler_for_current_sequence_ = true;
    send_touch_events_async_ = false;
    last_sent_touchevent_.reset();

    drop_remaining_touches_in_sequence_ = false;
    if (!has_handlers_) {
      drop_remaining_touches_in_sequence_ = true;
      // If the SkipTouchEventFilter experiment is running, drop through to
      // the loop that filters events with no nonstationary pointers below.
      if (ShouldFilterForEvent(event))
        return PreFilterResult::kFilteredNoPageHandlers;
    }
  }

  if (drop_remaining_touches_in_sequence_ &&
      event.GetType() != WebInputEvent::kTouchCancel &&
      // If the SkipTouchEventFilter experiment is running, drop through to
      // the loop that filters events with no nonstationary pointers below.
      ShouldFilterForEvent(event))
    return PreFilterResult::kFilteredNoPageHandlers;

  if (event.GetType() == WebInputEvent::kTouchStart) {
    if (has_handlers_ || maybe_has_handler_for_current_sequence_)
      return PreFilterResult::kUnfiltered;
    // If the SkipTouchEventFilter experiment is running, drop through to
    // the loop that filters events with no nonstationary pointers below.
    else if (ShouldFilterForEvent(event))
      return PreFilterResult::kFilteredNoPageHandlers;
  }

  // If none of the touch points active in the current sequence have handlers,
  // don't forward the touch event.
  if (!maybe_has_handler_for_current_sequence_ && ShouldFilterForEvent(event))
    return PreFilterResult::kFilteredNoHandlerForSequence;

  // Only forward a touch if it has a non-stationary pointer that is active
  // in the current touch sequence.
  for (size_t i = 0; i < event.touches_length; ++i) {
    const WebTouchPoint& point = event.touches[i];
    if (point.state == WebTouchPoint::kStateStationary)
      continue;

    // |last_sent_touchevent_| will be non-null as long as there is an
    // active touch sequence being forwarded to the renderer.
    if (!last_sent_touchevent_)
      continue;

    for (size_t j = 0; j < last_sent_touchevent_->touches_length; ++j) {
      if (point.id != last_sent_touchevent_->touches[j].id)
        continue;

      if (event.GetType() != WebInputEvent::kTouchMove)
        return PreFilterResult::kUnfiltered;

      // All pointers in TouchMove events may have state as StateMoved,
      // even though none of the pointers have not changed in real.
      // Forward these events when at least one pointer has changed.
      if (HasPointChanged(last_sent_touchevent_->touches[j], point))
        return PreFilterResult::kUnfiltered;

      // This is a TouchMove event for which we have yet to find a
      // non-stationary pointer. Continue checking the next pointers
      // in the |event|.
      break;
    }
  }

  return PreFilterResult::kFilteredNoNonstationaryPointers;
}

void PassthroughTouchEventQueue::UpdateTouchConsumerStates(
    const WebTouchEvent& event,
    InputEventAckState ack_result) {
  if (event.GetType() == WebInputEvent::kTouchStart) {
    if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
      send_touch_events_async_ = false;

    // Once we have the ack back for the sequence we know if there
    // is a handler or not. Other touch-starts sent can upgrade
    // whether we have a handler or not as well.
    if (WebTouchEventTraits::IsTouchSequenceStart(event)) {
      maybe_has_handler_for_current_sequence_ =
          ack_result != INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
    } else {
      maybe_has_handler_for_current_sequence_ |=
          ack_result != INPUT_EVENT_ACK_STATE_NO_CONSUMER_EXISTS;
    }
  } else if (WebTouchEventTraits::IsTouchSequenceEnd(event)) {
    maybe_has_handler_for_current_sequence_ = false;
  }
}

size_t PassthroughTouchEventQueue::SizeForTesting() const {
  return outstanding_touches_.size();
}

bool PassthroughTouchEventQueue::IsTimeoutRunningForTesting() const {
  return timeout_handler_ && timeout_handler_->IsTimeoutTimerRunning();
}

}  // namespace content

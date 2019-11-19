// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/mouse_wheel_phase_handler.h"

#include "base/bind.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_input_event_router.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "ui/events/base_event_utils.h"

namespace content {
MouseWheelPhaseHandler::MouseWheelPhaseHandler(
    RenderWidgetHostViewBase* const host_view)
    : host_view_(host_view),
      mouse_wheel_end_dispatch_timeout_(kDefaultMouseWheelLatchingTransaction),
      touchpad_scroll_phase_state_(TOUCHPAD_SCROLL_STATE_UNKNOWN) {}

void MouseWheelPhaseHandler::AddPhaseIfNeededAndScheduleEndEvent(
    blink::WebMouseWheelEvent& mouse_wheel_event,
    bool should_route_event) {

  bool has_phase =
      mouse_wheel_event.phase != blink::WebMouseWheelEvent::kPhaseNone ||
      mouse_wheel_event.momentum_phase != blink::WebMouseWheelEvent::kPhaseNone;
  TRACE_EVENT2("input",
               "MouseWheelPhaseHandler::AddPhaseIfNeededAndScheduleEndEvent",
               "has_phase", has_phase, "touchpad_scroll_phase_state",
               touchpad_scroll_phase_state_);

  if (has_phase) {
    if (mouse_wheel_event.phase == blink::WebMouseWheelEvent::kPhaseEnded) {
      // If the momentum_phase is anything other than blocked, don't send the
      // wheel end event immediately, start a timer instead to see whether
      // momentum phase of the scrolling starts or not. If momentum_phase is
      // blocked, that means that momentum scrolling events are not coming
      // next (i.e. preparing for a pinch, maybe), so end immediately.
      if (mouse_wheel_event.momentum_phase ==
          blink::WebMouseWheelEvent::kPhaseBlocked) {
        SendSyntheticWheelEventWithPhaseEnded(should_route_event);
      } else {
        ScheduleMouseWheelEndDispatching(
            should_route_event,
            max_time_between_phase_ended_and_momentum_phase_began());
      }
    } else if (mouse_wheel_event.phase ==
               blink::WebMouseWheelEvent::kPhaseBegan) {
      // A new scrolling sequence has started, send the pending wheel end
      // event to end the previous scrolling sequence.
      DispatchPendingWheelEndEvent();
    } else if (mouse_wheel_event.momentum_phase ==
               blink::WebMouseWheelEvent::kPhaseBegan) {
      // Momentum phase has started, drop the pending wheel end event to make
      // sure that no wheel end event will be sent during the momentum phase
      // of scrolling.
      IgnorePendingWheelEndEvent();
    }
  } else {  // !has_phase
    switch (touchpad_scroll_phase_state_) {
      case TOUCHPAD_SCROLL_STATE_UNKNOWN: {
        mouse_wheel_event.has_synthetic_phase = true;
        // Break the latching when the location difference between the current
        // and the initial wheel event positions exceeds the maximum allowed
        // threshold or when the event modifiers have changed.
        if (!IsWithinSlopRegion(mouse_wheel_event) ||
            HasDifferentModifiers(mouse_wheel_event) ||
            ShouldBreakLatchingDueToDirectionChange(mouse_wheel_event)) {
          DispatchPendingWheelEndEvent();
        }

        if (!mouse_wheel_end_dispatch_timer_.IsRunning()) {
          mouse_wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
          first_wheel_location_ =
              gfx::Vector2dF(mouse_wheel_event.PositionInWidget().x,
                             mouse_wheel_event.PositionInWidget().y);
          initial_wheel_event_ = mouse_wheel_event;
          first_scroll_update_ack_state_ =
              FirstScrollUpdateAckState::kNotArrived;
          ScheduleMouseWheelEndDispatching(should_route_event,
                                           mouse_wheel_end_dispatch_timeout_);
        } else {  // mouse_wheel_end_dispatch_timer_.IsRunning()
          bool non_zero_delta =
              mouse_wheel_event.delta_x || mouse_wheel_event.delta_y;
          mouse_wheel_event.phase =
              non_zero_delta ? blink::WebMouseWheelEvent::kPhaseChanged
                             : blink::WebMouseWheelEvent::kPhaseStationary;
          mouse_wheel_end_dispatch_timer_.Reset();
        }
        break;
      }
      case TOUCHPAD_SCROLL_MAY_BEGIN:
        mouse_wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
        touchpad_scroll_phase_state_ = TOUCHPAD_SCROLL_IN_PROGRESS;
        break;
      case TOUCHPAD_SCROLL_IN_PROGRESS:
        mouse_wheel_event.phase = blink::WebMouseWheelEvent::kPhaseChanged;
        break;
      default:
        NOTREACHED();
    }
  }

  last_mouse_wheel_event_ = mouse_wheel_event;
}

void MouseWheelPhaseHandler::DispatchPendingWheelEndEvent() {
  if (!mouse_wheel_end_dispatch_timer_.IsRunning())
    return;

  TRACE_EVENT_INSTANT0("input", "MouseWheelPhaseHandler Dispatched",
                       TRACE_EVENT_SCOPE_THREAD);
  mouse_wheel_end_dispatch_timer_.FireNow();
}

void MouseWheelPhaseHandler::IgnorePendingWheelEndEvent() {
  if (!mouse_wheel_end_dispatch_timer_.IsRunning())
    return;

  TRACE_EVENT_INSTANT0("input", "MouseWheelPhaseHandler Ignored",
                       TRACE_EVENT_SCOPE_THREAD);
  mouse_wheel_end_dispatch_timer_.Stop();
}

void MouseWheelPhaseHandler::ResetTouchpadScrollSequence() {
  TRACE_EVENT_INSTANT0("input", "MouseWheelPhaseHandler Reset",
                       TRACE_EVENT_SCOPE_THREAD);
  touchpad_scroll_phase_state_ = TOUCHPAD_SCROLL_STATE_UNKNOWN;
}

void MouseWheelPhaseHandler::SendWheelEndForTouchpadScrollingIfNeeded(
    bool should_route_event) {
  if (touchpad_scroll_phase_state_ == TOUCHPAD_SCROLL_IN_PROGRESS) {
    RenderWidgetHostImpl* widget_host = host_view_->host();
    if (!widget_host) {
      ResetTouchpadScrollSequence();
      return;
    }

    TRACE_EVENT_INSTANT0("input", "MouseWheelPhaseHandler Sent touchpad end",
                         TRACE_EVENT_SCOPE_THREAD);
    SendSyntheticWheelEventWithPhaseEnded(should_route_event);
  }

  ResetTouchpadScrollSequence();
}

void MouseWheelPhaseHandler::TouchpadScrollingMayBegin() {
  // End the timer-based wheel scroll sequence before starting a touchpad scroll
  // sequence.
  if (mouse_wheel_end_dispatch_timer_.IsRunning()) {
    DCHECK_EQ(TOUCHPAD_SCROLL_STATE_UNKNOWN, touchpad_scroll_phase_state_);
    DispatchPendingWheelEndEvent();
  }

  TRACE_EVENT_INSTANT0("input", "MouseWheelPhaseHandler May Begin",
                       TRACE_EVENT_SCOPE_THREAD);
  touchpad_scroll_phase_state_ = TOUCHPAD_SCROLL_MAY_BEGIN;
}

void MouseWheelPhaseHandler::SendSyntheticWheelEventWithPhaseEnded(
    bool should_route_event) {
  TRACE_EVENT0("input",
               "MouseWheelPhaseHandler::SendSyntheticWheelEventWithPhaseEnded");
  last_mouse_wheel_event_.SetTimeStamp(ui::EventTimeForNow());
  last_mouse_wheel_event_.delta_x = 0;
  last_mouse_wheel_event_.delta_y = 0;
  last_mouse_wheel_event_.wheel_ticks_x = 0;
  last_mouse_wheel_event_.wheel_ticks_y = 0;
  last_mouse_wheel_event_.phase = blink::WebMouseWheelEvent::kPhaseEnded;
  last_mouse_wheel_event_.dispatch_type =
      blink::WebInputEvent::DispatchType::kEventNonBlocking;

  if (should_route_event) {
    RenderWidgetHostImpl* widget_host = host_view_->host();
    if (!widget_host || !widget_host->delegate() ||
        !widget_host->delegate()->GetInputEventRouter())
      return;

    widget_host->delegate()->GetInputEventRouter()->RouteMouseWheelEvent(
        host_view_, &last_mouse_wheel_event_,
        ui::LatencyInfo(ui::SourceEventType::WHEEL));
  } else {
    host_view_->ProcessMouseWheelEvent(
        last_mouse_wheel_event_, ui::LatencyInfo(ui::SourceEventType::WHEEL));
  }
}

void MouseWheelPhaseHandler::ScheduleMouseWheelEndDispatching(
    bool should_route_event,
    const base::TimeDelta timeout) {
  TRACE_EVENT_INSTANT0("input", "MouseWheelPhaseHandler timer started",
                       TRACE_EVENT_SCOPE_THREAD);
  mouse_wheel_end_dispatch_timer_.Start(
      FROM_HERE, timeout,
      base::BindOnce(
          &MouseWheelPhaseHandler::SendSyntheticWheelEventWithPhaseEnded,
          base::Unretained(this), should_route_event));
}

bool MouseWheelPhaseHandler::IsWithinSlopRegion(
    const blink::WebMouseWheelEvent& wheel_event) const {
  // This function is called to check if breaking timer-based wheel scroll
  // latching sequence is needed or not, and timer-based wheel scroll latching
  // happens only when scroll state is unknown.
  DCHECK(touchpad_scroll_phase_state_ == TOUCHPAD_SCROLL_STATE_UNKNOWN);
  gfx::Vector2dF current_wheel_location(wheel_event.PositionInWidget().x,
                                        wheel_event.PositionInWidget().y);
  return (current_wheel_location - first_wheel_location_).LengthSquared() <
         kWheelLatchingSlopRegion * kWheelLatchingSlopRegion;
}

bool MouseWheelPhaseHandler::HasDifferentModifiers(
    const blink::WebMouseWheelEvent& wheel_event) const {
  // This function is called to check if breaking timer-based wheel scroll
  // latching sequence is needed or not, and timer-based wheel scroll latching
  // happens only when scroll state is unknown.
  DCHECK(touchpad_scroll_phase_state_ == TOUCHPAD_SCROLL_STATE_UNKNOWN);
  return wheel_event.GetModifiers() != initial_wheel_event_.GetModifiers();
}

bool MouseWheelPhaseHandler::ShouldBreakLatchingDueToDirectionChange(
    const blink::WebMouseWheelEvent& wheel_event) const {
  // This function is called to check if breaking timer-based wheel scroll
  // latching sequence is needed or not, and timer-based wheel scroll latching
  // happens only when scroll state is unknown.
  DCHECK(touchpad_scroll_phase_state_ == TOUCHPAD_SCROLL_STATE_UNKNOWN);
  if (first_scroll_update_ack_state_ != FirstScrollUpdateAckState::kNotConsumed)
    return false;

  bool consistent_x_direction =
      (wheel_event.delta_x == 0 && initial_wheel_event_.delta_x == 0) ||
      wheel_event.delta_x * initial_wheel_event_.delta_x > 0;
  bool consistent_y_direction =
      (wheel_event.delta_y == 0 && initial_wheel_event_.delta_y == 0) ||
      wheel_event.delta_y * initial_wheel_event_.delta_y > 0;
  return !consistent_x_direction || !consistent_y_direction;
}

void MouseWheelPhaseHandler::GestureEventAck(
    const blink::WebGestureEvent& event,
    InputEventAckState ack_result) {
  if (event.GetType() != blink::WebInputEvent::kGestureScrollUpdate ||
      first_scroll_update_ack_state_ !=
          FirstScrollUpdateAckState::kNotArrived) {
    return;
  }
  if (ack_result == INPUT_EVENT_ACK_STATE_CONSUMED)
    first_scroll_update_ack_state_ = FirstScrollUpdateAckState::kConsumed;
  else
    first_scroll_update_ack_state_ = FirstScrollUpdateAckState::kNotConsumed;
}

}  // namespace content

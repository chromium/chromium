// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOUSE_WHEEL_PHASE_HANDLER_H_
#define MOUSE_WHEEL_PHASE_HANDLER_H_

#include "base/timer/timer.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/common/content_export.h"
#include "content/public/common/input_event_ack_state.h"
#include "third_party/blink/public/platform/web_mouse_wheel_event.h"

namespace content {
class RenderWidgetHostViewBase;

// The duration after which a synthetic wheel with zero deltas and
// phase = |kPhaseEnded| will be sent after the last wheel event.
constexpr base::TimeDelta kDefaultMouseWheelLatchingTransaction =
    base::TimeDelta::FromMilliseconds(500);

// Maximum allowed difference between coordinates of two mouse wheel events in
// the same scroll sequence.
const double kWheelLatchingSlopRegion = 10.0;

// On ChromeOS wheel events don't have phase information; However, whenever the
// user puts down their fingers on touchpad a GFC is received and at the end of
// touchpad scrolling when the user lifts their fingers a GFS is received. This
// enum tracks the current state of the touchpad scrolling by listening to GFC
// and GFS events.
enum TouchpadScrollPhaseState {
  // Scrolling with normal mouse wheels doesn't give any information about the
  // state of scrolling.
  TOUCHPAD_SCROLL_STATE_UNKNOWN = 0,
  // Shows that the user has put their fingers down and a scroll may start.
  TOUCHPAD_SCROLL_MAY_BEGIN,
  // Scrolling has started and the user hasn't lift their fingers, yet.
  TOUCHPAD_SCROLL_IN_PROGRESS,
};

enum class FirstScrollUpdateAckState {
  // Shows that the ACK for the first GSU event is not arrived yet.
  kNotArrived = 0,
  // Shows that the first GSU event is consumed.
  kConsumed,
  // Shows that the first GSU event is not consumed.
  kNotConsumed,
};

// The MouseWheelPhaseHandler is responsible for adding the proper phase to
// wheel events. Phase information is necessary for wheel scrolling since it
// shows the start and end of a scrolling sequence.
class CONTENT_EXPORT MouseWheelPhaseHandler {
 public:
  MouseWheelPhaseHandler(RenderWidgetHostViewBase* const host_view);
  ~MouseWheelPhaseHandler() {}

  void AddPhaseIfNeededAndScheduleEndEvent(
      blink::WebMouseWheelEvent& mouse_wheel_event,
      bool should_route_event);
  void DispatchPendingWheelEndEvent();
  void IgnorePendingWheelEndEvent();
  void ResetTouchpadScrollSequence();
  void SendWheelEndForTouchpadScrollingIfNeeded(bool should_route_event);
  void TouchpadScrollingMayBegin();

  // Used to set the timer timeout for testing.
  void set_mouse_wheel_end_dispatch_timeout(base::TimeDelta timeout) {
    mouse_wheel_end_dispatch_timeout_ = timeout;
  }

  bool HasPendingWheelEndEvent() const {
    return mouse_wheel_end_dispatch_timer_.IsRunning();
  }
  void GestureEventAck(const blink::WebGestureEvent& event,
                       InputEventAckState ack_result);

  // Used to verify the correctness of touchpad_scroll_phase_state_'s value in
  // testing.
  TouchpadScrollPhaseState touchpad_scroll_phase_state_for_test() const {
    return touchpad_scroll_phase_state_;
  }

  // Used in testing for setting the max time to wait for momentum phase began
  // after a scroll phase end.
  void set_max_time_between_phase_ended_and_momentum_phase_began(
      base::TimeDelta timeout) {
    max_time_between_phase_ended_and_momentum_phase_began_ = timeout;
  }

  // Used get the max time to wait for a momentum scroll to begin.
  const base::TimeDelta
  max_time_between_phase_ended_and_momentum_phase_began() {
    return max_time_between_phase_ended_and_momentum_phase_began_;
  }

 private:
  void SendSyntheticWheelEventWithPhaseEnded(bool should_route_event);
  void ScheduleMouseWheelEndDispatching(bool should_route_event,
                                        const base::TimeDelta timeout);
  bool IsWithinSlopRegion(const blink::WebMouseWheelEvent& wheel_event) const;
  bool HasDifferentModifiers(
      const blink::WebMouseWheelEvent& wheel_event) const;
  bool ShouldBreakLatchingDueToDirectionChange(
      const blink::WebMouseWheelEvent& wheel_event) const;

  RenderWidgetHostViewBase* const host_view_;
  base::OneShotTimer mouse_wheel_end_dispatch_timer_;
  base::TimeDelta mouse_wheel_end_dispatch_timeout_;
  blink::WebMouseWheelEvent last_mouse_wheel_event_;
  TouchpadScrollPhaseState touchpad_scroll_phase_state_;
  // This is used to break the timer based latching when the difference between
  // the locations of the first wheel event and the current wheel event is
  // larger than some threshold. The variable value is only valid while the
  // dispatch timer is running.
  gfx::Vector2dF first_wheel_location_;

  // This is used to break the timer based latching when the new wheel event has
  // different modifiers or when it is in a different direction from the
  // previous wheel events and the scrolling has been ignored.
  blink::WebMouseWheelEvent initial_wheel_event_;

  FirstScrollUpdateAckState first_scroll_update_ack_state_ =
      FirstScrollUpdateAckState::kNotArrived;

  // Maximum time that the phase handler waits for arrival of a wheel event with
  // momentum_phase = kPhaseBegan before sending its previous wheel event with
  // phase = kPhaseEnded.
  base::TimeDelta max_time_between_phase_ended_and_momentum_phase_began_ =
      base::TimeDelta::FromMilliseconds(100);

  DISALLOW_COPY_AND_ASSIGN(MouseWheelPhaseHandler);
};

}  // namespace content

#endif  // MOUSE_WHEEL_PHASE_HANDLER_H_

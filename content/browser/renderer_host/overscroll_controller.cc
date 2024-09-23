// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/overscroll_controller.h"

#include <algorithm>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/notreached.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

namespace content {

namespace {

// Minimum amount of time after an actual scroll after which a pull-to-refresh
// can start.
constexpr base::TimeDelta kPullToRefreshCoolOffDelay = base::Milliseconds(600);

bool IsGestureEventFromTouchpad(const blink::WebInputEvent& event) {
  DCHECK(blink::WebInputEvent::IsGestureEventType(event.GetType()));
  const blink::WebGestureEvent& gesture =
      static_cast<const blink::WebGestureEvent&>(event);
  return gesture.SourceDevice() == blink::WebGestureDevice::kTouchpad;
}

bool IsGestureEventFromAutoscroll(const blink::WebGestureEvent event) {
  return event.SourceDevice() == blink::WebGestureDevice::kSyntheticAutoscroll;
}

bool IsGestureScrollUpdateInertialEvent(const blink::WebInputEvent& event) {
  if (event.GetType() != blink::WebInputEvent::Type::kGestureScrollUpdate)
    return false;

  const blink::WebGestureEvent& gesture =
      static_cast<const blink::WebGestureEvent&>(event);
  return gesture.data.scroll_update.inertial_phase ==
         blink::WebGestureEvent::InertialPhaseState::kMomentum;
}

float ClampAbsoluteValue(float value, float max_abs) {
  DCHECK_LT(0.f, max_abs);
  return std::clamp(value, -max_abs, max_abs);
}

}  // namespace

OverscrollController::OverscrollController() {}

OverscrollController::~OverscrollController() {}

bool OverscrollController::ShouldProcessEvent(
    const blink::WebInputEvent& event) {
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kGestureScrollBegin:
    case blink::WebInputEvent::Type::kGestureScrollUpdate:
    case blink::WebInputEvent::Type::kGestureScrollEnd: {
      const blink::WebGestureEvent& gesture =
          static_cast<const blink::WebGestureEvent&>(event);

      // Gesture events with Autoscroll source don't cause overscrolling.
      if (IsGestureEventFromAutoscroll(gesture))
        return false;

      ui::ScrollGranularity granularity;
      switch (event.GetType()) {
        case blink::WebInputEvent::Type::kGestureScrollBegin:
          granularity = gesture.data.scroll_begin.delta_hint_units;
          break;
        case blink::WebInputEvent::Type::kGestureScrollUpdate:
          granularity = gesture.data.scroll_update.delta_units;
          break;
        case blink::WebInputEvent::Type::kGestureScrollEnd:
          granularity = gesture.data.scroll_end.delta_units;
          break;
        default:
          granularity = ui::ScrollGranularity::kScrollByPixel;
          break;
      }

      return granularity == ui::ScrollGranularity::kScrollByPrecisePixel;
    }
    default:
      break;
  }
  return true;
}

bool OverscrollController::ShouldIgnoreInertialEvent(
    const blink::WebInputEvent& event) const {
  return ignore_following_inertial_events_ &&
         IsGestureScrollUpdateInertialEvent(event);
  }

bool OverscrollController::WillHandleEvent(const blink::WebInputEvent& event) {
  if (!ShouldProcessEvent(event))
    return false;

  // TODO(mohsen): Consider filtering mouse-wheel events during overscroll. See
  // https://crbug.com/772106.
  if (event.GetType() == blink::WebInputEvent::Type::kMouseWheel)
    return false;

  if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin) {
    ignore_following_inertial_events_ = false;
    first_inertial_event_time_.reset();
    time_since_last_ignored_scroll_ =
        event.TimeStamp() - last_ignored_scroll_time_;
    // Will handle events when processing ACKs to ensure the correct order.
    return false;
  }

  if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollEnd) {
    if (scroll_state_ == ScrollState::CONTENT_CONSUMING ||
        overscroll_ignored_) {
      last_ignored_scroll_time_ = event.TimeStamp();
    }
    // Will handle events when processing ACKs to ensure the correct order.
    return false;
  }

  // Consume the scroll-update events if they are from a inertial scroll (fling)
  // event that completed an overscroll gesture.
  if (ShouldIgnoreInertialEvent(event))
    return true;

  bool reset_scroll_state = false;
  if (scroll_state_ != ScrollState::NONE || overscroll_delta_x_ ||
      overscroll_delta_y_) {
    switch (event.GetType()) {
      case blink::WebInputEvent::Type::kGestureFlingStart:
        reset_scroll_state = true;
        break;

      default:
        if (blink::WebInputEvent::IsMouseEventType(event.GetType()) ||
            blink::WebInputEvent::IsKeyboardEventType(event.GetType())) {
          reset_scroll_state = true;
        }
        break;
    }
  }

  if (reset_scroll_state)
    ResetScrollState();

  if (DispatchEventCompletesAction(event)) {
    CompleteAction();

    // Let the event be dispatched to the renderer.
    return false;
  }

  if (overscroll_mode_ != OVERSCROLL_NONE && DispatchEventResetsState(event)) {
    SetOverscrollMode(OVERSCROLL_NONE, OverscrollSource::NONE);

    // Let the event be dispatched to the renderer.
    return false;
  }

  if (overscroll_mode_ != OVERSCROLL_NONE) {
    // Consume the event only if it updates the overscroll state.
    if (ProcessEventForOverscroll(event))
      return true;
  } else if (reset_scroll_state) {
    overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
  }

  // In overscrolling state, consume scroll-update and fling-start events when
  // they do not contribute to overscroll in order to prevent content scroll.
  // TODO(bokan): This needs to account for behavior_ somehow since if the page
  // declares that it doesn't want an overscroll effect, we should allow
  // sending the scroll update events to generate DOM overscroll events.
  // https://crbug.com/1112183.
  return scroll_state_ == ScrollState::OVERSCROLLING &&
         (event.GetType() == blink::WebInputEvent::Type::kGestureScrollUpdate ||
          event.GetType() == blink::WebInputEvent::Type::kGestureFlingStart);
}

void OverscrollController::OnDidOverscroll(
    const ui::DidOverscrollParams& params) {
  // TODO(sunyunjia): We should also decide whether to trigger overscroll,
  // update scroll_state_ here. See https://crbug.com/799467.
  behavior_ = params.overscroll_behavior;
}

void OverscrollController::ReceivedEventACK(const blink::WebInputEvent& event,
                                            bool processed) {
  if (!ShouldProcessEvent(event))
    return;

  // An inertial scroll (fling) event from a completed overscroll gesture
  // should not modify states below.
  if (ShouldIgnoreInertialEvent(event))
    return;

  if (processed) {
    // If a scroll event is consumed by the page, i.e. some content on the page
    // has been scrolled, then there is not going to be an overscroll gesture,
    // until the current scroll ends, and a new scroll gesture starts.
    // Similarly, if a mouse-wheel event is consumed, probably the page has
    // implemented its own scroll-like behavior and no overscroll should happen.
    if (scroll_state_ == ScrollState::NONE &&
        (event.GetType() == blink::WebInputEvent::Type::kGestureScrollUpdate ||
         event.GetType() == blink::WebInputEvent::Type::kMouseWheel)) {
      scroll_state_ = ScrollState::CONTENT_CONSUMING;
    }
    // In overscrolling state, only return if we are in an overscroll mode;
    // otherwise, we would want to ProcessEventForOverscroll to let it start a
    // new overscroll mode.
    if (scroll_state_ != ScrollState::OVERSCROLLING ||
        overscroll_mode_ != OVERSCROLL_NONE) {
      return;
    }
  }

  if (event.GetType() == blink::WebInputEvent::Type::kMouseWheel)
    return;

  ProcessEventForOverscroll(event);
}

void OverscrollController::Reset() {
  overscroll_mode_ = OVERSCROLL_NONE;
  overscroll_source_ = OverscrollSource::NONE;
  overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
  ResetScrollState();
}

void OverscrollController::Cancel() {
  SetOverscrollMode(OVERSCROLL_NONE, OverscrollSource::NONE);
  overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
  ResetScrollState();
}

bool OverscrollController::DispatchEventCompletesAction(
    const blink::WebInputEvent& event) const {
  if (overscroll_mode_ == OVERSCROLL_NONE)
    return false;
  DCHECK_NE(OverscrollSource::NONE, overscroll_source_);

  // Complete the overscroll gesture if there was a mouse move or a scroll-end
  // after the threshold.
  if (event.GetType() != blink::WebInputEvent::Type::kMouseMove &&
      event.GetType() != blink::WebInputEvent::Type::kGestureScrollEnd &&
      event.GetType() != blink::WebInputEvent::Type::kGestureFlingStart &&
      event.GetType() != blink::WebInputEvent::Type::kGestureScrollUpdate)
    return false;

  // Complete the overscroll gesture for inertial scroll (fling) event from
  // touchpad.
  if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollUpdate) {
    if (overscroll_source_ != OverscrollSource::TOUCHPAD)
      return false;
    DCHECK(IsGestureEventFromTouchpad(event));
    const blink::WebGestureEvent gesture_event =
        static_cast<const blink::WebGestureEvent&>(event);
    if (gesture_event.data.scroll_update.inertial_phase !=
        blink::WebGestureEvent::InertialPhaseState::kMomentum)
      return false;
  }

  if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollEnd &&
      overscroll_source_ == OverscrollSource::TOUCHPAD) {
    DCHECK(IsGestureEventFromTouchpad(event));
    // Complete the action for a GSE with touchpad source only when it is in
    // momentumPhase.
    const blink::WebGestureEvent gesture_event =
        static_cast<const blink::WebGestureEvent&>(event);
    if (gesture_event.data.scroll_end.inertial_phase !=
        blink::WebGestureEvent::InertialPhaseState::kMomentum)
      return false;
  }

  if (!delegate_)
    return false;

  if (event.GetType() == blink::WebInputEvent::Type::kGestureFlingStart) {
    // Check to see if the fling is in the same direction of the overscroll.
    const blink::WebGestureEvent gesture =
        static_cast<const blink::WebGestureEvent&>(event);
    switch (overscroll_mode_) {
      case OVERSCROLL_EAST:
        if (gesture.data.fling_start.velocity_x < 0)
          return false;
        break;
      case OVERSCROLL_WEST:
        if (gesture.data.fling_start.velocity_x > 0)
          return false;
        break;
      case OVERSCROLL_NORTH:
        if (gesture.data.fling_start.velocity_y > 0)
          return false;
        break;
      case OVERSCROLL_SOUTH:
        if (gesture.data.fling_start.velocity_y < 0)
          return false;
        break;
      case OVERSCROLL_NONE:
        NOTREACHED_IN_MIGRATION();
    }
  }

  const gfx::Size size = delegate_->GetDisplaySize();
  if (size.IsEmpty())
    return false;

  const float delta =
      overscroll_mode_ == OVERSCROLL_WEST || overscroll_mode_ == OVERSCROLL_EAST
          ? overscroll_delta_x_
          : overscroll_delta_y_;
  const float ratio = fabs(delta) / std::max(size.width(), size.height());
  const float threshold =
      overscroll_source_ == OverscrollSource::TOUCHPAD
          ? OverscrollConfig::kCompleteTouchpadThresholdPercent
          : OverscrollConfig::kCompleteTouchscreenThresholdPercent;
  return ratio >= threshold;
}

bool OverscrollController::DispatchEventResetsState(
    const blink::WebInputEvent& event) const {
  switch (event.GetType()) {
    // GestureScrollBegin/End ACK will reset overscroll state when necessary.
    case blink::WebInputEvent::Type::kGestureScrollBegin:
    case blink::WebInputEvent::Type::kGestureScrollEnd:
    case blink::WebInputEvent::Type::kGestureScrollUpdate:
    case blink::WebInputEvent::Type::kGestureFlingCancel:
      return false;

    default:
      // Touch events can arrive during an overscroll gesture initiated by
      // touch-scrolling. These events should not reset the overscroll state.
      return !blink::WebInputEvent::IsTouchEventType(event.GetType());
  }
}

bool OverscrollController::ProcessEventForOverscroll(
    const blink::WebInputEvent& event) {
  bool event_processed = false;
  switch (event.GetType()) {
    case blink::WebInputEvent::Type::kGestureScrollBegin: {
      if (overscroll_mode_ != OVERSCROLL_NONE)
        SetOverscrollMode(OVERSCROLL_NONE, OverscrollSource::NONE);
      break;
    }
    case blink::WebInputEvent::Type::kGestureScrollEnd: {
      // Only reset the state on  GestureScrollEnd generated from the touchpad
      // when the scrolling is in inertial state.
      const blink::WebGestureEvent gesture_event =
          static_cast<const blink::WebGestureEvent&>(event);
      bool reset_scroll_state =
          !IsGestureEventFromTouchpad(event) ||
          (gesture_event.data.scroll_end.inertial_phase ==
           blink::WebGestureEvent::InertialPhaseState::kMomentum);

      if (reset_scroll_state)
        ResetScrollState();

      if (DispatchEventCompletesAction(event)) {
        CompleteAction();
        break;
      }

      if (!reset_scroll_state)
        break;

      if (overscroll_mode_ != OVERSCROLL_NONE) {
        SetOverscrollMode(OVERSCROLL_NONE, OverscrollSource::NONE);
      } else {
        overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
      }
      break;
    }
    case blink::WebInputEvent::Type::kGestureScrollUpdate: {
      const blink::WebGestureEvent& gesture =
          static_cast<const blink::WebGestureEvent&>(event);
      bool is_gesture_scroll_update_inertial_event =
          IsGestureScrollUpdateInertialEvent(event);
      event_processed = ProcessOverscroll(
          gesture.data.scroll_update.delta_x,
          gesture.data.scroll_update.delta_y,
          gesture.SourceDevice() == blink::WebGestureDevice::kTouchpad,
          is_gesture_scroll_update_inertial_event);
      if (is_gesture_scroll_update_inertial_event) {
        // Record the timestamp of first inertial event.
        if (!first_inertial_event_time_) {
          first_inertial_event_time_ = event.TimeStamp();
          break;
        }
        base::TimeDelta inertial_event_interval =
            event.TimeStamp() - first_inertial_event_time_.value();
        if (inertial_event_interval >=
            OverscrollConfig::MaxInertialEventsBeforeOverscrollCancellation()) {
          ignore_following_inertial_events_ = true;
          // Reset overscroll state if fling didn't complete the overscroll
          // gesture within the first 20 inertial events.
          Cancel();
        }
      }
      break;
    }
    case blink::WebInputEvent::Type::kGestureFlingStart: {
      const float kFlingVelocityThreshold = 1100.f;
      const blink::WebGestureEvent& gesture =
          static_cast<const blink::WebGestureEvent&>(event);
      float velocity_x = gesture.data.fling_start.velocity_x;
      float velocity_y = gesture.data.fling_start.velocity_y;
      if (fabs(velocity_x) > kFlingVelocityThreshold) {
        if ((overscroll_mode_ == OVERSCROLL_WEST && velocity_x < 0) ||
            (overscroll_mode_ == OVERSCROLL_EAST && velocity_x > 0)) {
          CompleteAction();
          event_processed = true;
          break;
        }
      } else if (fabs(velocity_y) > kFlingVelocityThreshold) {
        if ((overscroll_mode_ == OVERSCROLL_NORTH && velocity_y < 0) ||
            (overscroll_mode_ == OVERSCROLL_SOUTH && velocity_y > 0)) {
          CompleteAction();
          event_processed = true;
          break;
        }
      }

      // Reset overscroll state if fling didn't complete the overscroll gesture.
      SetOverscrollMode(OVERSCROLL_NONE, OverscrollSource::NONE);
      break;
    }

    default:
      DCHECK(blink::WebInputEvent::IsGestureEventType(event.GetType()) ||
             blink::WebInputEvent::IsTouchEventType(event.GetType()))
          << "Received unexpected event: " << event.GetType();
  }
  return event_processed;
}

bool OverscrollController::ProcessOverscroll(float delta_x,
                                             float delta_y,
                                             bool is_touchpad,
                                             bool is_inertial) {
  if (scroll_state_ == ScrollState::CONTENT_CONSUMING)
    return false;

  // Do not start overscroll for inertial events.
  if (overscroll_mode_ == OVERSCROLL_NONE && is_inertial)
    return false;

  overscroll_delta_x_ += delta_x;
  overscroll_delta_y_ += delta_y;

  const float start_threshold =
      is_touchpad ? OverscrollConfig::kStartTouchpadThresholdDips
                  : OverscrollConfig::kStartTouchscreenThresholdDips;
  if (fabs(overscroll_delta_x_) <= start_threshold &&
      fabs(overscroll_delta_y_) <= start_threshold) {
    SetOverscrollMode(OVERSCROLL_NONE, OverscrollSource::NONE);
    return true;
  }

  if (delegate_) {
    std::optional<float> cap = delegate_->GetMaxOverscrollDelta();
    if (cap) {
      DCHECK_LE(0.f, cap.value());
      switch (overscroll_mode_) {
        case OVERSCROLL_WEST:
        case OVERSCROLL_EAST:
          overscroll_delta_x_ = ClampAbsoluteValue(
              overscroll_delta_x_, cap.value() + start_threshold);
          break;
        case OVERSCROLL_NORTH:
        case OVERSCROLL_SOUTH:
          overscroll_delta_y_ = ClampAbsoluteValue(
              overscroll_delta_y_, cap.value() + start_threshold);
          break;
        case OVERSCROLL_NONE:
          break;
      }
    }
  }

  // Compute the current overscroll direction. If the direction is different
  // from the current direction, then always switch to no-overscroll mode first
  // to make sure that subsequent scroll events go through to the page first.
  OverscrollMode new_mode = OVERSCROLL_NONE;
  const float kMinRatio = 2.5;
  if (fabs(overscroll_delta_x_) > start_threshold &&
      fabs(overscroll_delta_x_) > fabs(overscroll_delta_y_) * kMinRatio)
    new_mode = overscroll_delta_x_ > 0.f ? OVERSCROLL_EAST : OVERSCROLL_WEST;
  else if (fabs(overscroll_delta_y_) > start_threshold &&
           fabs(overscroll_delta_y_) > fabs(overscroll_delta_x_) * kMinRatio)
    new_mode = overscroll_delta_y_ > 0.f ? OVERSCROLL_SOUTH : OVERSCROLL_NORTH;

  // The horizontal overscroll is used for history navigation. Enable it for
  // touchpad only if TouchpadOverscrollHistoryNavigation is enabled.
  if ((new_mode == OVERSCROLL_EAST || new_mode == OVERSCROLL_WEST) &&
      is_touchpad &&
      !OverscrollConfig::TouchpadOverscrollHistoryNavigationEnabled()) {
    new_mode = OVERSCROLL_NONE;
  }

  // The vertical overscroll is used for pull-to-refresh. Enable it only if
  // pull-to-refresh is enabled.
  if (new_mode == OVERSCROLL_SOUTH || new_mode == OVERSCROLL_NORTH) {
    auto ptr_mode = OverscrollConfig::GetPullToRefreshMode();
    if (ptr_mode == OverscrollConfig::PullToRefreshMode::kDisabled ||
        (ptr_mode ==
             OverscrollConfig::PullToRefreshMode::kEnabledTouchschreen &&
         is_touchpad) ||
        time_since_last_ignored_scroll_ < kPullToRefreshCoolOffDelay) {
      overscroll_ignored_ = true;
      new_mode = OVERSCROLL_NONE;
    }
  }

  if (overscroll_mode_ == OVERSCROLL_NONE) {
    SetOverscrollMode(new_mode, is_touchpad ? OverscrollSource::TOUCHPAD
                                            : OverscrollSource::TOUCHSCREEN);
  } else if (new_mode != overscroll_mode_) {
    SetOverscrollMode(OVERSCROLL_NONE, OverscrollSource::NONE);
  }

  if (overscroll_mode_ == OVERSCROLL_NONE)
    return false;

  overscroll_ignored_ = false;

  // Tell the delegate about the overscroll update so that it can update
  // the display accordingly (e.g. show history preview etc.).
  if (delegate_) {
    // Do not include the threshold amount when sending the deltas to the
    // delegate.
    float delegate_delta_x = overscroll_delta_x_;
    if (fabs(delegate_delta_x) > start_threshold) {
      if (delegate_delta_x < 0)
        delegate_delta_x += start_threshold;
      else
        delegate_delta_x -= start_threshold;
    } else {
      delegate_delta_x = 0.f;
    }

    float delegate_delta_y = overscroll_delta_y_;
    if (fabs(delegate_delta_y) > start_threshold) {
      if (delegate_delta_y < 0)
        delegate_delta_y += start_threshold;
      else
        delegate_delta_y -= start_threshold;
    } else {
      delegate_delta_y = 0.f;
    }
    return delegate_->OnOverscrollUpdate(delegate_delta_x, delegate_delta_y);
  }
  return false;
}

void OverscrollController::CompleteAction() {
  ignore_following_inertial_events_ = true;
  if (delegate_)
    delegate_->OnOverscrollComplete(overscroll_mode_);
  Reset();
}

void OverscrollController::SetOverscrollMode(OverscrollMode mode,
                                             OverscrollSource source) {
  if (overscroll_mode_ == mode)
    return;

  // If the mode changes to NONE, source is also NONE.
  DCHECK(mode != OVERSCROLL_NONE || source == OverscrollSource::NONE);

  // When setting to a non-NONE mode and there is a locked mode, don't set the
  // mode if the new mode is not the same as the locked mode.
  if (mode != OVERSCROLL_NONE && locked_mode_ != OVERSCROLL_NONE &&
      mode != locked_mode_) {
    return;
  }

  OverscrollMode old_mode = overscroll_mode_;
  overscroll_mode_ = mode;
  overscroll_source_ = source;
  if (overscroll_mode_ == OVERSCROLL_NONE) {
    overscroll_delta_x_ = overscroll_delta_y_ = 0.f;
  } else {
    scroll_state_ = ScrollState::OVERSCROLLING;
    locked_mode_ = overscroll_mode_;
  }
  if (delegate_) {
    delegate_->OnOverscrollModeChange(old_mode, overscroll_mode_, source,
                                      behavior_);
  }
}

void OverscrollController::ResetScrollState() {
  scroll_state_ = ScrollState::NONE;
  locked_mode_ = OVERSCROLL_NONE;
}

}  // namespace content

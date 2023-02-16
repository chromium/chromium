// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_smooth_move_gesture.h"

#include <stdint.h>

#include "base/check_op.h"
#include "base/notreached.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"
#include "ui/gfx/geometry/point_f.h"

namespace content {
namespace {

gfx::Vector2dF ProjectScalarOntoVector(float scalar,
                                       const gfx::Vector2dF& vector) {
  return gfx::ScaleVector2d(vector, scalar / vector.Length());
}

// returns the animation progress along an arctan curve to provide simple
// ease-in ease-out behavior.
float GetCurvedRatio(const base::TimeTicks& current,
                     const base::TimeTicks& start,
                     const base::TimeTicks& end,
                     int speed_in_pixels_s) {
  // Increasing this would make the start and the end of the curv smoother.
  // Hence the higher value for the higher speed.
  const float kArctanRange = sqrt(static_cast<double>(speed_in_pixels_s)) / 100;

  const float kMaxArctan = std::atan(kArctanRange / 2);
  const float kMinArctan = std::atan(-kArctanRange / 2);

  float linear_ratio = (current - start) / (end - start);
  return (std::atan(kArctanRange * linear_ratio - kArctanRange / 2) -
          kMinArctan) /
         (kMaxArctan - kMinArctan);
}

}  // namespace

SyntheticSmoothMoveGestureParams::SyntheticSmoothMoveGestureParams() = default;

SyntheticSmoothMoveGestureParams::SyntheticSmoothMoveGestureParams(
    const SyntheticSmoothMoveGestureParams& other) = default;

SyntheticSmoothMoveGestureParams::~SyntheticSmoothMoveGestureParams() = default;

SyntheticSmoothMoveGesture::SyntheticSmoothMoveGesture(
    SyntheticSmoothMoveGestureParams params)
    : params_(params),
      current_move_segment_start_position_(params.start_point) {}

SyntheticSmoothMoveGesture::~SyntheticSmoothMoveGesture() {}

SyntheticGesture::Result SyntheticSmoothMoveGesture::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  DCHECK(dispatching_controller_);
  if (state_ == SETUP) {
    state_ = STARTED;
    current_move_segment_ = -1;
    current_move_segment_stop_time_ = timestamp;
  }

  switch (params_.input_type) {
    case SyntheticSmoothMoveGestureParams::TOUCH_INPUT:
      if (!synthetic_pointer_driver_)
        synthetic_pointer_driver_ = SyntheticPointerDriver::Create(
            content::mojom::GestureSourceType::kTouchInput,
            params_.from_devtools_debugger);
      ForwardTouchInputEvents(timestamp, target);
      break;
    case SyntheticSmoothMoveGestureParams::MOUSE_DRAG_INPUT:
      if (!synthetic_pointer_driver_)
        synthetic_pointer_driver_ = SyntheticPointerDriver::Create(
            content::mojom::GestureSourceType::kMouseInput,
            params_.from_devtools_debugger);
      ForwardMouseClickInputEvents(timestamp, target);
      break;
    case SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT:
      ForwardMouseWheelInputEvents(timestamp, target);
      // A mousewheel should not be able to close the WebContents.
      DCHECK(dispatching_controller_);
      break;
    default:
      return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
  }
  if (!dispatching_controller_) {
    // A pointer gesture can cause the controller (and therefore `this`) to be
    // synchronously deleted (e.g. clicking tab-close). Return immediately in
    // this case.
    return SyntheticGesture::GESTURE_ABORT;
  }

  return (state_ == DONE) ? SyntheticGesture::GESTURE_FINISHED
                          : SyntheticGesture::GESTURE_RUNNING;
}

// TODO(ssid): Clean up the switch statements by adding functions instead of
// large code, in the Forward*Events functions. Move the actions for all input
// types to different class (SyntheticInputDevice) which generates input events
// for all input types. The gesture class can use instance of device actions.
// Refer: crbug.com/461825

// CAUTION: forwarding a pointer press/release can cause `this` to be deleted.
void SyntheticSmoothMoveGesture::ForwardTouchInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  switch (state_) {
    case STARTED:
      if (MoveIsNoOp()) {
        state_ = DONE;
        break;
      }
      if (params_.add_slop)
        AddTouchSlopToFirstDistance(target);
      ComputeNextMoveSegment();
      PressPoint(target, timestamp);
      if (!dispatching_controller_) {
        return;
      }
      state_ = MOVING;
      break;
    case MOVING: {
      base::TimeTicks event_timestamp = ClampTimestamp(timestamp);
      gfx::Vector2dF delta = GetPositionDeltaAtTime(event_timestamp);
      MovePoint(target, delta, event_timestamp);

      if (FinishedCurrentMoveSegment(event_timestamp)) {
        if (!IsLastMoveSegment()) {
          current_move_segment_start_position_ +=
              params_.distances[current_move_segment_];
          ComputeNextMoveSegment();
        } else if (params_.prevent_fling) {
          state_ = STOPPING;
        } else {
          ReleasePoint(target, event_timestamp);
          if (!dispatching_controller_) {
            return;
          }
          state_ = DONE;
        }
      }
    } break;
    case STOPPING:
      if (timestamp - current_move_segment_stop_time_ >=
          target->PointerAssumedStoppedTime()) {
        base::TimeTicks event_timestamp = current_move_segment_stop_time_ +
                                          target->PointerAssumedStoppedTime();
        ReleasePoint(target, event_timestamp);
        if (!dispatching_controller_) {
          return;
        }
        state_ = DONE;
      }
      break;
    case SETUP:
      NOTREACHED()
          << "State SETUP invalid for synthetic scroll using touch input.";
      break;
    case DONE:
      NOTREACHED()
          << "State DONE invalid for synthetic scroll using touch input.";
      break;
  }
}

void SyntheticSmoothMoveGesture::ForwardMouseWheelInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  switch (state_) {
    case STARTED:
      if (MoveIsNoOp()) {
        state_ = DONE;
        break;
      }
      ComputeNextMoveSegment();
      state_ = MOVING;
      break;
    case MOVING: {
      base::TimeTicks event_timestamp = ClampTimestamp(timestamp);
      gfx::Vector2dF delta = GetPositionDeltaAtTime(event_timestamp) -
                             current_move_segment_total_delta_;
      if (delta.x() || delta.y()) {
        blink::WebMouseWheelEvent::Phase phase =
            needs_scroll_begin_ ? blink::WebMouseWheelEvent::kPhaseBegan
                                : blink::WebMouseWheelEvent::kPhaseChanged;
        ForwardMouseWheelEvent(target, delta, phase, event_timestamp,
                               params_.modifiers);
        current_move_segment_total_delta_ += delta;
        needs_scroll_begin_ = false;
      }

      if (FinishedCurrentMoveSegment(event_timestamp)) {
        if (!IsLastMoveSegment()) {
          current_move_segment_total_delta_ = gfx::Vector2dF();
          ComputeNextMoveSegment();
        } else {
          state_ = DONE;

          // Start flinging on the swipe action.
          if (!params_.prevent_fling && (params_.fling_velocity_x != 0 ||
                                         params_.fling_velocity_y != 0)) {
            ForwardFlingGestureEvent(
                target, blink::WebGestureEvent::Type::kGestureFlingStart);
          } else {
            // Forward a wheel event with phase ended and zero deltas.
            ForwardMouseWheelEvent(target, gfx::Vector2d(),
                                   blink::WebMouseWheelEvent::kPhaseEnded,
                                   event_timestamp, params_.modifiers);
          }
          needs_scroll_begin_ = true;
        }
      }
    } break;
    case SETUP:
      NOTREACHED() << "State SETUP invalid for synthetic scroll using mouse "
                      "wheel input.";
      break;
    case STOPPING:
      NOTREACHED() << "State STOPPING invalid for synthetic scroll using mouse "
                      "wheel input.";
      break;
    case DONE:
      NOTREACHED()
          << "State DONE invalid for synthetic scroll using mouse wheel input.";
      break;
  }
}

// CAUTION: forwarding a pointer press/release can cause `this` to be deleted.
void SyntheticSmoothMoveGesture::ForwardMouseClickInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  switch (state_) {
    case STARTED:
      if (MoveIsNoOp()) {
        state_ = DONE;
        break;
      }
      ComputeNextMoveSegment();
      PressPoint(target, timestamp);
      if (!dispatching_controller_) {
        return;
      }
      state_ = MOVING;
      break;
    case MOVING: {
      base::TimeTicks event_timestamp = ClampTimestamp(timestamp);
      gfx::Vector2dF delta = GetPositionDeltaAtTime(event_timestamp);
      MovePoint(target, delta, event_timestamp);

      if (FinishedCurrentMoveSegment(event_timestamp)) {
        if (!IsLastMoveSegment()) {
          current_move_segment_start_position_ +=
              params_.distances[current_move_segment_];
          ComputeNextMoveSegment();
        } else {
          ReleasePoint(target, event_timestamp);
          if (!dispatching_controller_) {
            return;
          }
          state_ = DONE;
        }
      }
    } break;
    case STOPPING:
      NOTREACHED()
          << "State STOPPING invalid for synthetic drag using mouse input.";
      break;
    case SETUP:
      NOTREACHED()
          << "State SETUP invalid for synthetic drag using mouse input.";
      break;
    case DONE:
      NOTREACHED()
          << "State DONE invalid for synthetic drag using mouse input.";
      break;
  }
}

void SyntheticSmoothMoveGesture::ForwardMouseWheelEvent(
    SyntheticGestureTarget* target,
    const gfx::Vector2dF& delta,
    const blink::WebMouseWheelEvent::Phase phase,
    const base::TimeTicks& timestamp,
    int modifiers) const {
  if (params_.from_devtools_debugger) {
    modifiers |= blink::WebInputEvent::kFromDebugger;
  }
  blink::WebMouseWheelEvent mouse_wheel_event =
      blink::SyntheticWebMouseWheelEventBuilder::Build(
          0, 0, delta.x(), delta.y(), modifiers, params_.granularity);

  mouse_wheel_event.SetPositionInWidget(
      current_move_segment_start_position_.x(),
      current_move_segment_start_position_.y());
  mouse_wheel_event.phase = phase;

  mouse_wheel_event.SetTimeStamp(timestamp);

  target->DispatchInputEventToPlatform(mouse_wheel_event);
}

void SyntheticSmoothMoveGesture::ForwardFlingGestureEvent(
    SyntheticGestureTarget* target,
    const blink::WebInputEvent::Type type) const {
  blink::WebGestureEvent fling_gesture_event =
      blink::SyntheticWebGestureEventBuilder::Build(
          type, blink::WebGestureDevice::kTouchpad);
  fling_gesture_event.data.fling_start.velocity_x = params_.fling_velocity_x;
  fling_gesture_event.data.fling_start.velocity_y = params_.fling_velocity_y;
  fling_gesture_event.SetPositionInWidget(current_move_segment_start_position_);
  target->DispatchInputEventToPlatform(fling_gesture_event);
}

void SyntheticSmoothMoveGesture::PressPoint(SyntheticGestureTarget* target,
                                            const base::TimeTicks& timestamp) {
  DCHECK_EQ(current_move_segment_, 0);
  synthetic_pointer_driver_->Press(current_move_segment_start_position_.x(),
                                   current_move_segment_start_position_.y());
  synthetic_pointer_driver_->DispatchEvent(target, timestamp);
}

void SyntheticSmoothMoveGesture::MovePoint(SyntheticGestureTarget* target,
                                           const gfx::Vector2dF& delta,
                                           const base::TimeTicks& timestamp) {
  DCHECK_GE(current_move_segment_, 0);
  DCHECK_LT(current_move_segment_, static_cast<int>(params_.distances.size()));
  gfx::PointF new_position = current_move_segment_start_position_ + delta;
  synthetic_pointer_driver_->Move(new_position.x(), new_position.y());
  synthetic_pointer_driver_->DispatchEvent(target, timestamp);
}

void SyntheticSmoothMoveGesture::ReleasePoint(
    SyntheticGestureTarget* target,
    const base::TimeTicks& timestamp) {
  DCHECK_EQ(current_move_segment_,
            static_cast<int>(params_.distances.size()) - 1);
  gfx::PointF position;
  if (params_.input_type ==
      SyntheticSmoothMoveGestureParams::MOUSE_DRAG_INPUT) {
    position = current_move_segment_start_position_ +
               GetPositionDeltaAtTime(timestamp);
  }
  synthetic_pointer_driver_->Release();
  synthetic_pointer_driver_->DispatchEvent(target, timestamp);
}

void SyntheticSmoothMoveGesture::AddTouchSlopToFirstDistance(
    SyntheticGestureTarget* target) {
  DCHECK_GE(params_.distances.size(), 1ul);
  gfx::Vector2dF& first_move_distance = params_.distances[0];
  DCHECK_GT(first_move_distance.Length(), 0);
  first_move_distance += ProjectScalarOntoVector(target->GetTouchSlopInDips(),
                                                 first_move_distance);
}

gfx::Vector2dF SyntheticSmoothMoveGesture::GetPositionDeltaAtTime(
    const base::TimeTicks& timestamp) const {
  // Make sure the final delta is correct. Using the computation below can lead
  // to issues with floating point precision.
  // TODO(bokan): This comment makes it sound like we have pixel perfect
  // precision. In fact, gestures can accumulate a significant amount of
  // error (e.g. due to snapping to physical pixels on each event).
  if (FinishedCurrentMoveSegment(timestamp))
    return params_.distances[current_move_segment_];

  return gfx::ScaleVector2d(
      params_.distances[current_move_segment_],
      GetCurvedRatio(timestamp, current_move_segment_start_time_,
                     current_move_segment_stop_time_,
                     params_.speed_in_pixels_s));
}

void SyntheticSmoothMoveGesture::ComputeNextMoveSegment() {
  current_move_segment_++;
  DCHECK_LT(current_move_segment_, static_cast<int>(params_.distances.size()));
  // Percentage based scrolls do not require velocity and are delivered in a
  // single segment. No need to compute another segment
  if (params_.granularity == ui::ScrollGranularity::kScrollByPercentage) {
    current_move_segment_start_time_ = current_move_segment_stop_time_;
  } else {
    int64_t total_duration_in_us = static_cast<int64_t>(
        1e6 * (params_.distances[current_move_segment_].Length() /
               params_.speed_in_pixels_s));
    DCHECK_GT(total_duration_in_us, 0);
    current_move_segment_start_time_ = current_move_segment_stop_time_;
    current_move_segment_stop_time_ = current_move_segment_start_time_ +
                                      base::Microseconds(total_duration_in_us);
  }
}

base::TimeTicks SyntheticSmoothMoveGesture::ClampTimestamp(
    const base::TimeTicks& timestamp) const {
  return std::min(timestamp, current_move_segment_stop_time_);
}

bool SyntheticSmoothMoveGesture::FinishedCurrentMoveSegment(
    const base::TimeTicks& timestamp) const {
  return timestamp >= current_move_segment_stop_time_;
}

bool SyntheticSmoothMoveGesture::IsLastMoveSegment() const {
  DCHECK_LT(current_move_segment_, static_cast<int>(params_.distances.size()));
  return current_move_segment_ ==
         static_cast<int>(params_.distances.size()) - 1;
}

bool SyntheticSmoothMoveGesture::MoveIsNoOp() const {
  return params_.distances.size() == 0 || params_.distances[0].IsZero();
}

}  // namespace content

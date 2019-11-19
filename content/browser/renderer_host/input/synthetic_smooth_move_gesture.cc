// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_smooth_move_gesture.h"

#include <stdint.h>

#include "base/logging.h"
#include "ui/gfx/geometry/point_f.h"

namespace content {
namespace {

gfx::Vector2dF ProjectScalarOntoVector(float scalar,
                                       const gfx::Vector2dF& vector) {
  return gfx::ScaleVector2d(vector, scalar / vector.Length());
}

const int kDefaultSpeedInPixelsPerSec = 800;

}  // namespace

SyntheticSmoothMoveGestureParams::SyntheticSmoothMoveGestureParams()
    : speed_in_pixels_s(kDefaultSpeedInPixelsPerSec),
      fling_velocity_x(0),
      fling_velocity_y(0),
      prevent_fling(true),
      add_slop(true),
      granularity(ui::input_types::ScrollGranularity::kScrollByPixel) {}

SyntheticSmoothMoveGestureParams::SyntheticSmoothMoveGestureParams(
    const SyntheticSmoothMoveGestureParams& other) = default;

SyntheticSmoothMoveGestureParams::~SyntheticSmoothMoveGestureParams() {}

SyntheticSmoothMoveGesture::SyntheticSmoothMoveGesture(
    SyntheticSmoothMoveGestureParams params)
    : params_(params),
      current_move_segment_start_position_(params.start_point),
      state_(SETUP),
      needs_scroll_begin_(true) {}

SyntheticSmoothMoveGesture::~SyntheticSmoothMoveGesture() {}

SyntheticGesture::Result SyntheticSmoothMoveGesture::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  if (state_ == SETUP) {
    state_ = STARTED;
    current_move_segment_ = -1;
    current_move_segment_stop_time_ = timestamp;
  }

  switch (params_.input_type) {
    case SyntheticSmoothMoveGestureParams::TOUCH_INPUT:
      if (!synthetic_pointer_driver_)
        synthetic_pointer_driver_ =
            SyntheticPointerDriver::Create(SyntheticGestureParams::TOUCH_INPUT);
      ForwardTouchInputEvents(timestamp, target);
      break;
    case SyntheticSmoothMoveGestureParams::MOUSE_DRAG_INPUT:
      if (!synthetic_pointer_driver_)
        synthetic_pointer_driver_ =
            SyntheticPointerDriver::Create(SyntheticGestureParams::MOUSE_INPUT);
      ForwardMouseClickInputEvents(timestamp, target);
      break;
    case SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT:
      ForwardMouseWheelInputEvents(timestamp, target);
      break;
    default:
      return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
  }
  return (state_ == DONE) ? SyntheticGesture::GESTURE_FINISHED
                          : SyntheticGesture::GESTURE_RUNNING;
}

// TODO(ssid): Clean up the switch statements by adding functions instead of
// large code, in the Forward*Events functions. Move the actions for all input
// types to different class (SyntheticInputDevice) which generates input events
// for all input types. The gesture class can use instance of device actions.
// Refer: crbug.com/461825

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

      // Android MotionEvents that carry mouse wheel ticks and the tick
      // granularity. Since it's not easy to change this granularity, it means
      // we can only scroll in terms of number of these ticks. Note also: if
      // the delta is smaller than one tick size we wont send an event or
      // accumulate it in current_move_segment_total_delta_ so that we don't
      // consider that delta applied. If we did, slow scrolls would be entirely
      // lost since we'd send 0 ticks in each event but assume delta was
      // applied.
      int pixels_per_wheel_tick = target->GetMouseWheelMinimumGranularity();
      if (pixels_per_wheel_tick) {
        int wheel_ticks_x = static_cast<int>(delta.x() / pixels_per_wheel_tick);
        int wheel_ticks_y = static_cast<int>(delta.y() / pixels_per_wheel_tick);
        delta = gfx::Vector2dF(wheel_ticks_x * pixels_per_wheel_tick,
                               wheel_ticks_y * pixels_per_wheel_tick);
      }

      if (delta.x() || delta.y()) {
        blink::WebMouseWheelEvent::Phase phase =
            needs_scroll_begin_ ? blink::WebMouseWheelEvent::kPhaseBegan
                                : blink::WebMouseWheelEvent::kPhaseChanged;
        ForwardMouseWheelEvent(target, delta, phase, event_timestamp);
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
                target, blink::WebGestureEvent::kGestureFlingStart);
          } else {
            // Forward a wheel event with phase ended and zero deltas.
            ForwardMouseWheelEvent(target, gfx::Vector2d(),
                                   blink::WebMouseWheelEvent::kPhaseEnded,
                                   event_timestamp);
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
    const base::TimeTicks& timestamp) const {
  blink::WebMouseWheelEvent mouse_wheel_event =
      SyntheticWebMouseWheelEventBuilder::Build(0, 0, delta.x(), delta.y(), 0,
                                                params_.granularity);

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
      SyntheticWebGestureEventBuilder::Build(
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

  float delta_length =
      params_.speed_in_pixels_s *
      (timestamp - current_move_segment_start_time_).InSecondsF();
  return ProjectScalarOntoVector(delta_length,
                                 params_.distances[current_move_segment_]);
}

void SyntheticSmoothMoveGesture::ComputeNextMoveSegment() {
  current_move_segment_++;
  DCHECK_LT(current_move_segment_, static_cast<int>(params_.distances.size()));
  int64_t total_duration_in_us = static_cast<int64_t>(
      1e6 * (params_.distances[current_move_segment_].Length() /
             params_.speed_in_pixels_s));
  DCHECK_GT(total_duration_in_us, 0);
  current_move_segment_start_time_ = current_move_segment_stop_time_;
  current_move_segment_stop_time_ =
      current_move_segment_start_time_ +
      base::TimeDelta::FromMicroseconds(total_duration_in_us);
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

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_touchscreen_pinch_gesture.h"

#include <stdint.h>

#include <cmath>

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "ui/latency/latency_info.h"

namespace content {

SyntheticTouchscreenPinchGesture::SyntheticTouchscreenPinchGesture(
    const SyntheticPinchGestureParams& gesture_params)
    : SyntheticGestureBase(gesture_params),
      start_y_0_(0.0f),
      start_y_1_(0.0f),
      max_pointer_delta_0_(0.0f),
      gesture_source_type_(content::mojom::GestureSourceType::kDefaultInput),
      state_(SETUP) {
  CHECK_EQ(SyntheticGestureParams::PINCH_GESTURE,
           gesture_params.GetGestureType());
  DCHECK_GT(params().scale_factor, 0.0f);
  if (params().gesture_source_type !=
      content::mojom::GestureSourceType::kTouchInput) {
    DCHECK_EQ(params().gesture_source_type,
              content::mojom::GestureSourceType::kDefaultInput);
    params_->gesture_source_type =
        content::mojom::GestureSourceType::kTouchInput;
  }
}

SyntheticTouchscreenPinchGesture::~SyntheticTouchscreenPinchGesture() {}

SyntheticGesture::Result SyntheticTouchscreenPinchGesture::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  CHECK(dispatching_controller_);
  // Keep this on the stack so we can check if the forwarded event caused the
  // deletion of the controller (which owns `this`).
  base::WeakPtr<SyntheticGestureController> weak_controller =
      dispatching_controller_;
  if (state_ == SETUP) {
    gesture_source_type_ = params().gesture_source_type;
    if (gesture_source_type_ ==
        content::mojom::GestureSourceType::kDefaultInput)
      gesture_source_type_ = target->GetDefaultSyntheticGestureSourceType();

    state_ = STARTED;
    start_time_ = timestamp;
  }

  DCHECK_NE(gesture_source_type_,
            content::mojom::GestureSourceType::kDefaultInput);

  if (!synthetic_pointer_driver_)
    synthetic_pointer_driver_ = SyntheticPointerDriver::Create(
        gesture_source_type_, params().from_devtools_debugger);

  if (gesture_source_type_ == content::mojom::GestureSourceType::kTouchInput) {
    ForwardTouchInputEvents(timestamp, target);
    // A pinch gesture cannot cause `this` to be destroyed.
    CHECK(weak_controller);
  } else {
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
  }

  return (state_ == DONE) ? SyntheticGesture::GESTURE_FINISHED
                          : SyntheticGesture::GESTURE_RUNNING;
}

void SyntheticTouchscreenPinchGesture::WaitForTargetAck(
    base::OnceClosure callback,
    SyntheticGestureTarget* target) const {
  target->WaitForTargetAck(params().GetGestureType(), gesture_source_type_,
                           std::move(callback));
}

void SyntheticTouchscreenPinchGesture::ForwardTouchInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  switch (state_) {
    case STARTED:
      // Check for an early finish.
      if (params().scale_factor == 1.0f) {
        state_ = DONE;
        break;
      }
      SetupCoordinatesAndStopTime(target);
      PressTouchPoints(target, timestamp);
      state_ = MOVING;
      break;
    case MOVING: {
      base::TimeTicks event_timestamp = ClampTimestamp(timestamp);
      float delta = GetDeltaForPointer0AtTime(event_timestamp);
      MoveTouchPoints(target, delta, event_timestamp);
      if (HasReachedTarget(event_timestamp)) {
        ReleaseTouchPoints(target, event_timestamp);
        state_ = DONE;
      }
    } break;
    case SETUP:
      NOTREACHED_IN_MIGRATION() << "State SETUP invalid for synthetic pinch.";
      break;
    case DONE:
      NOTREACHED_IN_MIGRATION() << "State DONE invalid for synthetic pinch.";
      break;
  }
}

void SyntheticTouchscreenPinchGesture::PressTouchPoints(
    SyntheticGestureTarget* target,
    const base::TimeTicks& timestamp) {
  synthetic_pointer_driver_->Press(params().anchor.x(), start_y_0_, 0);
  synthetic_pointer_driver_->DispatchEvent(target, timestamp);
  synthetic_pointer_driver_->Press(params().anchor.x(), start_y_1_, 1);
  synthetic_pointer_driver_->DispatchEvent(target, timestamp);
}

void SyntheticTouchscreenPinchGesture::MoveTouchPoints(
    SyntheticGestureTarget* target,
    float delta,
    const base::TimeTicks& timestamp) {
  // The two pointers move in opposite directions.
  float current_y_0 = start_y_0_ + delta;
  float current_y_1 = start_y_1_ - delta;

  synthetic_pointer_driver_->Move(params().anchor.x(), current_y_0, 0);
  synthetic_pointer_driver_->Move(params().anchor.x(), current_y_1, 1);
  synthetic_pointer_driver_->DispatchEvent(target, timestamp);
}

void SyntheticTouchscreenPinchGesture::ReleaseTouchPoints(
    SyntheticGestureTarget* target,
    const base::TimeTicks& timestamp) {
  synthetic_pointer_driver_->Release(1);
  synthetic_pointer_driver_->DispatchEvent(target, timestamp);
  synthetic_pointer_driver_->Release(0);
  synthetic_pointer_driver_->DispatchEvent(target, timestamp);
}

void SyntheticTouchscreenPinchGesture::SetupCoordinatesAndStopTime(
    SyntheticGestureTarget* target) {
  // To achieve the specified scaling factor, the ratio of the final to the
  // initial span (distance between the pointers) has to be equal to the scaling
  // factor. Since we're moving both pointers at the same speed, each pointer's
  // distance to the anchor is half the span.
  float initial_distance_to_anchor, final_distance_to_anchor;
  const float single_point_slop = target->GetSpanSlopInDips() / 2.0f;
  if (params().scale_factor > 1.0f) {  // zooming in
    initial_distance_to_anchor = target->GetMinScalingSpanInDips() / 2.0f;
    final_distance_to_anchor =
        (initial_distance_to_anchor + single_point_slop) *
        params().scale_factor;
  } else {  // zooming out
    final_distance_to_anchor = target->GetMinScalingSpanInDips() / 2.0f;
    initial_distance_to_anchor =
        (final_distance_to_anchor / params().scale_factor) + single_point_slop;
  }

  start_y_0_ = params().anchor.y() - initial_distance_to_anchor;
  start_y_1_ = params().anchor.y() + initial_distance_to_anchor;

  max_pointer_delta_0_ = initial_distance_to_anchor - final_distance_to_anchor;

  const auto duration =
      base::Seconds(double{std::abs(2 * max_pointer_delta_0_)} /
                    params().relative_pointer_speed_in_pixels_s);
  stop_time_ = start_time_ + duration;
}

float SyntheticTouchscreenPinchGesture::GetDeltaForPointer0AtTime(
    const base::TimeTicks& timestamp) const {
  // Make sure the final delta is correct. Using the computation below can lead
  // to issues with floating point precision.
  if (HasReachedTarget(timestamp))
    return max_pointer_delta_0_;

  float total_abs_delta = params().relative_pointer_speed_in_pixels_s *
                          (timestamp - start_time_).InSecondsF();
  float abs_delta_pointer_0 = total_abs_delta / 2.0f;
  return (params().scale_factor > 1.0f) ? -abs_delta_pointer_0
                                        : abs_delta_pointer_0;
}

base::TimeTicks SyntheticTouchscreenPinchGesture::ClampTimestamp(
    const base::TimeTicks& timestamp) const {
  return std::min(timestamp, stop_time_);
}

bool SyntheticTouchscreenPinchGesture::HasReachedTarget(
    const base::TimeTicks& timestamp) const {
  return timestamp >= stop_time_;
}

}  // namespace content

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/callback.h"
#include "content/browser/renderer_host/input/synthetic_touchpad_pinch_gesture.h"

namespace content {
namespace {

float Lerp(float start, float end, float progress) {
  return start + progress * (end - start);
}

}  // namespace

SyntheticTouchpadPinchGesture::SyntheticTouchpadPinchGesture(
    const SyntheticPinchGestureParams& params)
    : params_(params),
      gesture_source_type_(SyntheticGestureParams::DEFAULT_INPUT),
      state_(SETUP),
      current_scale_(1.0f) {
  DCHECK_GT(params_.scale_factor, 0.0f);
}

SyntheticTouchpadPinchGesture::~SyntheticTouchpadPinchGesture() {}

SyntheticGesture::Result SyntheticTouchpadPinchGesture::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  if (state_ == SETUP) {
    gesture_source_type_ = params_.gesture_source_type;
    if (gesture_source_type_ == SyntheticGestureParams::DEFAULT_INPUT)
      gesture_source_type_ = target->GetDefaultSyntheticGestureSourceType();

    state_ = STARTED;
    start_time_ = timestamp;
  }

  DCHECK_NE(gesture_source_type_, SyntheticGestureParams::DEFAULT_INPUT);
  if (gesture_source_type_ == SyntheticGestureParams::MOUSE_INPUT) {
    ForwardGestureEvents(timestamp, target);
  } else {
    // Touch input should be using SyntheticTouchscreenPinchGesture.
    return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
  }

  return (state_ == DONE) ? SyntheticGesture::GESTURE_FINISHED
                          : SyntheticGesture::GESTURE_RUNNING;
}

void SyntheticTouchpadPinchGesture::WaitForTargetAck(
    base::OnceClosure callback,
    SyntheticGestureTarget* target) const {
  target->WaitForTargetAck(params_.GetGestureType(), gesture_source_type_,
                           std::move(callback));
}

void SyntheticTouchpadPinchGesture::ForwardGestureEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  switch (state_) {
    case STARTED:
      // Check for an early finish.
      if (params_.scale_factor == 1.0f) {
        state_ = DONE;
        break;
      }

      CalculateEndTime(target);

      // Send the start event.
      target->DispatchInputEventToPlatform(
          SyntheticWebGestureEventBuilder::Build(
              blink::WebGestureEvent::kGesturePinchBegin,
              blink::WebGestureDevice::kTouchpad));
      state_ = IN_PROGRESS;
      break;
    case IN_PROGRESS: {
      base::TimeTicks event_timestamp = ClampTimestamp(timestamp);

      float target_scale = CalculateTargetScale(event_timestamp);
      float incremental_scale = target_scale / current_scale_;
      current_scale_ = target_scale;

      // Send the incremental scale event.
      target->DispatchInputEventToPlatform(
          SyntheticWebGestureEventBuilder::BuildPinchUpdate(
              incremental_scale, params_.anchor.x(), params_.anchor.y(),
              0 /* modifierFlags */, blink::WebGestureDevice::kTouchpad));

      if (HasReachedTarget(event_timestamp)) {
        target->DispatchInputEventToPlatform(
            SyntheticWebGestureEventBuilder::Build(
                blink::WebGestureEvent::kGesturePinchEnd,
                blink::WebGestureDevice::kTouchpad));
        state_ = DONE;
      }
      break;
    }
    case SETUP:
      NOTREACHED() << "State SETUP invalid for synthetic pinch.";
      break;
    case DONE:
      NOTREACHED() << "State DONE invalid for synthetic pinch.";
      break;
  }
}

float SyntheticTouchpadPinchGesture::CalculateTargetScale(
    const base::TimeTicks& timestamp) const {
  // Make sure the final delta is correct. Using the computation below can lead
  // to issues with floating point precision.
  if (HasReachedTarget(timestamp))
    return params_.scale_factor;

  float progress = (timestamp - start_time_).InSecondsF() /
                   (stop_time_ - start_time_).InSecondsF();
  return Lerp(1.0f, params_.scale_factor, progress);
}

// Calculate an end time based on the amount of scaling to be done and the
// |relative_pointer_speed_in_pixels_s|. Because we don't have an actual pixel
// delta, we assume that a pinch of 200 pixels is needed to double the screen
// size and generate a stop time based on that.
// TODO(ericrk): We should not calculate duration from
// |relative_pointer_speed_in_pixels_s|, but should instead get a duration from
// a SyntheticTouchpadPinchGestureParams type. crbug.com/534976
void SyntheticTouchpadPinchGesture::CalculateEndTime(
    SyntheticGestureTarget* target) {
  const int kPixelsNeededToDoubleOrHalve = 200;

  float scale_factor = params_.scale_factor;
  if (scale_factor < 1.0f) {
    // If we are scaling down, calculate the time based on the inverse so that
    // halving or doubling the scale takes the same amount of time.
    scale_factor = 1.0f / scale_factor;
  }
  float scale_factor_delta =
      (scale_factor - 1.0f) * kPixelsNeededToDoubleOrHalve;

  int64_t total_duration_in_us =
      static_cast<int64_t>(1e6 * (static_cast<double>(scale_factor_delta) /
                                  params_.relative_pointer_speed_in_pixels_s));
  DCHECK_GT(total_duration_in_us, 0);
  stop_time_ =
      start_time_ + base::TimeDelta::FromMicroseconds(total_duration_in_us);
}

base::TimeTicks SyntheticTouchpadPinchGesture::ClampTimestamp(
    const base::TimeTicks& timestamp) const {
  return std::min(timestamp, stop_time_);
}

bool SyntheticTouchpadPinchGesture::HasReachedTarget(
    const base::TimeTicks& timestamp) const {
  return timestamp >= stop_time_;
}

}  // namespace content

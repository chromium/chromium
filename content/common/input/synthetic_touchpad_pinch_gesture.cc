// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/functional/callback.h"
#include "content/common/input/input_injector.mojom.h"
#include "content/common/input/synthetic_touchpad_pinch_gesture.h"
#include "third_party/blink/public/common/input/synthetic_web_input_event_builders.h"

namespace content {
namespace {

float Lerp(float start, float end, float progress) {
  return start + progress * (end - start);
}

}  // namespace

SyntheticTouchpadPinchGesture::SyntheticTouchpadPinchGesture(
    const SyntheticPinchGestureParams& gesture_params)
    : SyntheticGestureBase(gesture_params),
      gesture_source_type_(content::mojom::GestureSourceType::kDefaultInput),
      state_(SETUP),
      current_scale_(1.0f) {
  CHECK_EQ(SyntheticGestureParams::PINCH_GESTURE,
           gesture_params.GetGestureType());
  DCHECK_GT(params().scale_factor, 0.0f);
  if (params().gesture_source_type !=
      content::mojom::GestureSourceType::kTouchpadInput) {
    DCHECK_EQ(params().gesture_source_type,
              content::mojom::GestureSourceType::kDefaultInput);
    params().gesture_source_type =
        content::mojom::GestureSourceType::kTouchpadInput;
  }
}

SyntheticTouchpadPinchGesture::~SyntheticTouchpadPinchGesture() {}

SyntheticGesture::Result SyntheticTouchpadPinchGesture::ForwardInputEvents(
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
  if (gesture_source_type_ == content::mojom::GestureSourceType::kMouseInput) {
    ForwardGestureEvents(timestamp, target);

    // A pinch gesture cannot cause `this` to be destroyed.
    CHECK(weak_controller);
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
  target->WaitForTargetAck(params().GetGestureType(), gesture_source_type_,
                           std::move(callback));
}

void SyntheticTouchpadPinchGesture::ForwardGestureEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  switch (state_) {
    case STARTED:
      // Check for an early finish.
      if (params().scale_factor == 1.0f) {
        state_ = DONE;
        break;
      }

      CalculateEndTime(target);

      // Send the start event.
      target->DispatchInputEventToPlatform(
          blink::SyntheticWebGestureEventBuilder::Build(
              blink::WebGestureEvent::Type::kGesturePinchBegin,
              blink::WebGestureDevice::kTouchpad,
              params().from_devtools_debugger
                  ? blink::WebInputEvent::kFromDebugger
                  : blink::WebInputEvent::kNoModifiers));
      state_ = IN_PROGRESS;
      break;
    case IN_PROGRESS: {
      base::TimeTicks event_timestamp = ClampTimestamp(timestamp);

      float target_scale = CalculateTargetScale(event_timestamp);
      float incremental_scale = target_scale / current_scale_;
      current_scale_ = target_scale;

      // Send the incremental scale event.
      target->DispatchInputEventToPlatform(
          blink::SyntheticWebGestureEventBuilder::BuildPinchUpdate(
              incremental_scale, params().anchor.x(), params().anchor.y(),
              params().from_devtools_debugger
                  ? blink::WebInputEvent::kFromDebugger
                  : blink::WebInputEvent::kNoModifiers,
              blink::WebGestureDevice::kTouchpad));

      if (HasReachedTarget(event_timestamp)) {
        target->DispatchInputEventToPlatform(
            blink::SyntheticWebGestureEventBuilder::Build(
                blink::WebGestureEvent::Type::kGesturePinchEnd,
                blink::WebGestureDevice::kTouchpad,
                params().from_devtools_debugger
                    ? blink::WebInputEvent::kFromDebugger
                    : blink::WebInputEvent::kNoModifiers));
        state_ = DONE;
      }
      break;
    }
    case SETUP:
      NOTREACHED_IN_MIGRATION() << "State SETUP invalid for synthetic pinch.";
      break;
    case DONE:
      NOTREACHED_IN_MIGRATION() << "State DONE invalid for synthetic pinch.";
      break;
  }
}

float SyntheticTouchpadPinchGesture::CalculateTargetScale(
    const base::TimeTicks& timestamp) const {
  // Make sure the final delta is correct. Using the computation below can lead
  // to issues with floating point precision.
  if (HasReachedTarget(timestamp))
    return params().scale_factor;

  const float progress = (timestamp - start_time_) / (stop_time_ - start_time_);
  return Lerp(1.0f, params().scale_factor, progress);
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

  float scale_factor = params().scale_factor;
  if (scale_factor < 1.0f) {
    // If we are scaling down, calculate the time based on the inverse so that
    // halving or doubling the scale takes the same amount of time.
    scale_factor = 1.0f / scale_factor;
  }
  float scale_factor_delta =
      (scale_factor - 1.0f) * kPixelsNeededToDoubleOrHalve;

  const base::TimeDelta total_duration = base::Seconds(
      scale_factor_delta / params().relative_pointer_speed_in_pixels_s);
  DCHECK_GT(total_duration, base::TimeDelta());
  stop_time_ = start_time_ + total_duration;
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

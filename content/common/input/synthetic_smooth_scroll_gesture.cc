// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_smooth_scroll_gesture.h"

namespace content {

SyntheticSmoothScrollGesture::SyntheticSmoothScrollGesture(
    const SyntheticSmoothScrollGestureParams& params)
    : SyntheticGestureBase(params) {
  CHECK_EQ(SyntheticGestureParams::SMOOTH_SCROLL_GESTURE,
           params.GetGestureType());
}

SyntheticSmoothScrollGesture::~SyntheticSmoothScrollGesture() = default;

SyntheticGesture::Result SyntheticSmoothScrollGesture::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  DCHECK(dispatching_controller_);
  if (!move_gesture_) {
    if (!InitializeMoveGesture(params().gesture_source_type, target)) {
      return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
    }
  }
  return move_gesture_->ForwardInputEvents(timestamp, target);
}

void SyntheticSmoothScrollGesture::WaitForTargetAck(
    base::OnceClosure callback,
    SyntheticGestureTarget* target) const {
  target->WaitForTargetAck(params().GetGestureType(),
                           params().gesture_source_type, std::move(callback));
}

SyntheticSmoothMoveGestureParams::InputType
SyntheticSmoothScrollGesture::GetInputSourceType(
    content::mojom::GestureSourceType gesture_source_type) {
  if (gesture_source_type == content::mojom::GestureSourceType::kMouseInput)
    return SyntheticSmoothMoveGestureParams::MOUSE_WHEEL_INPUT;
  else
    return SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
}

bool SyntheticSmoothScrollGesture::InitializeMoveGesture(
    content::mojom::GestureSourceType gesture_type,
    SyntheticGestureTarget* target) {
  if (gesture_type == content::mojom::GestureSourceType::kDefaultInput)
    gesture_type = target->GetDefaultSyntheticGestureSourceType();

  if (gesture_type == content::mojom::GestureSourceType::kTouchInput ||
      gesture_type == content::mojom::GestureSourceType::kMouseInput) {
    SyntheticSmoothMoveGestureParams move_params;
    move_params.start_point = params().anchor;
    move_params.distances = params().distances;
    move_params.speed_in_pixels_s = params().speed_in_pixels_s;
    move_params.fling_velocity_x = params().fling_velocity_x;
    move_params.fling_velocity_y = params().fling_velocity_y;
    move_params.prevent_fling = params().prevent_fling;
    move_params.input_type = GetInputSourceType(gesture_type);
    move_params.add_slop = true;
    move_params.granularity = params().granularity;
    move_params.modifiers = params().modifiers;
    move_params.vsync_offset_ms = params().vsync_offset_ms;
    move_params.input_event_pattern = params().input_event_pattern;
    move_params.from_devtools_debugger = params().from_devtools_debugger;
    move_gesture_ = std::make_unique<SyntheticSmoothMoveGesture>(move_params);
    move_gesture_->DidQueue(dispatching_controller_);
    return true;
  }
  return false;
}

}  // namespace content

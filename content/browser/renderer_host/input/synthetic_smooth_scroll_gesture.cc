// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"

namespace content {

SyntheticSmoothScrollGesture::SyntheticSmoothScrollGesture(
    const SyntheticSmoothScrollGestureParams& params)
    : params_(params) {
}

SyntheticSmoothScrollGesture::~SyntheticSmoothScrollGesture() = default;

SyntheticGesture::Result SyntheticSmoothScrollGesture::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  if (!move_gesture_) {
    if (!InitializeMoveGesture(params_.gesture_source_type, target))
      return SyntheticGesture::GESTURE_SOURCE_TYPE_NOT_IMPLEMENTED;
  }
  return move_gesture_->ForwardInputEvents(timestamp, target);
}

void SyntheticSmoothScrollGesture::WaitForTargetAck(
    base::OnceClosure callback,
    SyntheticGestureTarget* target) const {
  target->WaitForTargetAck(params_.GetGestureType(),
                           params_.gesture_source_type, std::move(callback));
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
    move_params.start_point = params_.anchor;
    move_params.distances = params_.distances;
    move_params.speed_in_pixels_s = params_.speed_in_pixels_s;
    move_params.fling_velocity_x = params_.fling_velocity_x;
    move_params.fling_velocity_y = params_.fling_velocity_y;
    move_params.prevent_fling = params_.prevent_fling;
    move_params.input_type = GetInputSourceType(gesture_type);
    move_params.add_slop = true;
    move_params.granularity = params_.granularity;
    move_params.modifiers = params_.modifiers;
    move_params.from_devtools_debugger = params_.from_devtools_debugger;
    move_gesture_ = std::make_unique<SyntheticSmoothMoveGesture>(move_params);
    return true;
  }
  return false;
}

}  // namespace content

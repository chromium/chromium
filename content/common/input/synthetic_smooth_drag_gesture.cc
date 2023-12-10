// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_smooth_drag_gesture.h"

#include <memory>

namespace content {

SyntheticSmoothDragGesture::SyntheticSmoothDragGesture(
    const SyntheticSmoothDragGestureParams& params)
    : SyntheticGestureBase(params) {
  CHECK_EQ(SyntheticGestureParams::SMOOTH_DRAG_GESTURE,
           params.GetGestureType());
}

SyntheticSmoothDragGesture::~SyntheticSmoothDragGesture() {
}

SyntheticGesture::Result SyntheticSmoothDragGesture::ForwardInputEvents(
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

void SyntheticSmoothDragGesture::WaitForTargetAck(
    base::OnceClosure callback,
    SyntheticGestureTarget* target) const {
  target->WaitForTargetAck(params().GetGestureType(),
                           params().gesture_source_type, std::move(callback));
}

SyntheticSmoothMoveGestureParams::InputType
SyntheticSmoothDragGesture::GetInputSourceType(
    content::mojom::GestureSourceType gesture_source_type) {
  if (gesture_source_type == content::mojom::GestureSourceType::kMouseInput)
    return SyntheticSmoothMoveGestureParams::MOUSE_DRAG_INPUT;
  else
    return SyntheticSmoothMoveGestureParams::TOUCH_INPUT;
}

bool SyntheticSmoothDragGesture::InitializeMoveGesture(
    content::mojom::GestureSourceType gesture_type,
    SyntheticGestureTarget* target) {
  if (gesture_type == content::mojom::GestureSourceType::kDefaultInput)
    gesture_type = target->GetDefaultSyntheticGestureSourceType();

  if (gesture_type == content::mojom::GestureSourceType::kTouchInput ||
      gesture_type == content::mojom::GestureSourceType::kMouseInput) {
    SyntheticSmoothMoveGestureParams move_params;
    move_params.start_point = params().start_point;
    move_params.distances = params().distances;
    move_params.speed_in_pixels_s = params().speed_in_pixels_s;
    move_params.prevent_fling = true;
    move_params.input_type = GetInputSourceType(gesture_type);
    move_params.add_slop = false;
    move_params.from_devtools_debugger = params().from_devtools_debugger;
    move_gesture_ = std::make_unique<SyntheticSmoothMoveGesture>(move_params);
    move_gesture_->DidQueue(dispatching_controller_);
    return true;
  }
  return false;
}

}  // namespace content

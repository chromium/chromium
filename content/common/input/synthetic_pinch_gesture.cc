// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_pinch_gesture.h"

#include <memory>

#include "content/common/input/synthetic_touchpad_pinch_gesture.h"
#include "content/common/input/synthetic_touchscreen_pinch_gesture.h"

namespace content {

SyntheticPinchGesture::SyntheticPinchGesture(
    const SyntheticPinchGestureParams& params)
    : SyntheticGestureBase(params) {
  CHECK_EQ(SyntheticGestureParams::PINCH_GESTURE, params.GetGestureType());
}

SyntheticPinchGesture::~SyntheticPinchGesture() = default;

SyntheticGesture::Result SyntheticPinchGesture::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  DCHECK(dispatching_controller_);
  if (!lazy_gesture_) {
    content::mojom::GestureSourceType source_type =
        params().gesture_source_type;
    if (source_type == content::mojom::GestureSourceType::kDefaultInput) {
      source_type = target->GetDefaultSyntheticGestureSourceType();
    }

    DCHECK_NE(content::mojom::GestureSourceType::kDefaultInput, source_type);
    if (source_type == content::mojom::GestureSourceType::kTouchInput) {
      lazy_gesture_ =
          std::make_unique<SyntheticTouchscreenPinchGesture>(params());
    } else {
      DCHECK_EQ(content::mojom::GestureSourceType::kMouseInput, source_type);
      lazy_gesture_ = std::make_unique<SyntheticTouchpadPinchGesture>(params());
    }
    lazy_gesture_->DidQueue(dispatching_controller_);
  }

  return lazy_gesture_->ForwardInputEvents(timestamp, target);
}

void SyntheticPinchGesture::WaitForTargetAck(
    base::OnceClosure callback,
    SyntheticGestureTarget* target) const {
  target->WaitForTargetAck(params().GetGestureType(),
                           params().gesture_source_type, std::move(callback));
}

}  // namespace content

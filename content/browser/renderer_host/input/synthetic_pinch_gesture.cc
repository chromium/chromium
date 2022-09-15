// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_pinch_gesture.h"

#include <memory>

#include "content/browser/renderer_host/input/synthetic_touchpad_pinch_gesture.h"
#include "content/browser/renderer_host/input/synthetic_touchscreen_pinch_gesture.h"

namespace content {

SyntheticPinchGesture::SyntheticPinchGesture(
    const SyntheticPinchGestureParams& params)
    : params_(params) {}
SyntheticPinchGesture::~SyntheticPinchGesture() {}

SyntheticGesture::Result SyntheticPinchGesture::ForwardInputEvents(
    const base::TimeTicks& timestamp,
    SyntheticGestureTarget* target) {
  if (!lazy_gesture_) {
    content::mojom::GestureSourceType source_type = params_.gesture_source_type;
    if (source_type == content::mojom::GestureSourceType::kDefaultInput) {
      source_type = target->GetDefaultSyntheticGestureSourceType();
    }

    DCHECK_NE(content::mojom::GestureSourceType::kDefaultInput, source_type);
    if (source_type == content::mojom::GestureSourceType::kTouchInput) {
      lazy_gesture_ =
          std::make_unique<SyntheticTouchscreenPinchGesture>(params_);
    } else {
      DCHECK_EQ(content::mojom::GestureSourceType::kMouseInput, source_type);
      lazy_gesture_ = std::make_unique<SyntheticTouchpadPinchGesture>(params_);
    }
  }

  return lazy_gesture_->ForwardInputEvents(timestamp, target);
}

void SyntheticPinchGesture::WaitForTargetAck(
    base::OnceClosure callback,
    SyntheticGestureTarget* target) const {
  target->WaitForTargetAck(params_.GetGestureType(),
                           params_.gesture_source_type, std::move(callback));
}

}  // namespace content

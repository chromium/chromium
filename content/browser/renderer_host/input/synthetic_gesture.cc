// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/input/synthetic_gesture.h"

#include "base/notreached.h"
#include "content/browser/renderer_host/input/synthetic_gesture_target.h"
#include "content/browser/renderer_host/input/synthetic_pinch_gesture.h"
#include "content/browser/renderer_host/input/synthetic_pointer_action.h"
#include "content/browser/renderer_host/input/synthetic_smooth_drag_gesture.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/renderer_host/input/synthetic_tap_gesture.h"

namespace content {
namespace {

template <typename GestureType, typename GestureParamsType>
static std::unique_ptr<SyntheticGesture> CreateGesture(
    const SyntheticGestureParams& gesture_params) {
  return std::unique_ptr<SyntheticGesture>(
      new GestureType(*GestureParamsType::Cast(&gesture_params)));
}

}  // namespace

SyntheticGesture::SyntheticGesture() {}

SyntheticGesture::~SyntheticGesture() {}

std::unique_ptr<SyntheticGesture> SyntheticGesture::Create(
    const SyntheticGestureParams& gesture_params) {
  switch (gesture_params.GetGestureType()) {
    case SyntheticGestureParams::SMOOTH_SCROLL_GESTURE:
      return CreateGesture<SyntheticSmoothScrollGesture,
                           SyntheticSmoothScrollGestureParams>(gesture_params);
    case SyntheticGestureParams::SMOOTH_DRAG_GESTURE:
      return CreateGesture<SyntheticSmoothDragGesture,
                           SyntheticSmoothDragGestureParams>(gesture_params);
    case SyntheticGestureParams::PINCH_GESTURE:
      return CreateGesture<SyntheticPinchGesture,
                           SyntheticPinchGestureParams>(gesture_params);
    case SyntheticGestureParams::TAP_GESTURE:
      return CreateGesture<SyntheticTapGesture,
                           SyntheticTapGestureParams>(gesture_params);
    case SyntheticGestureParams::POINTER_ACTION_LIST:
      return CreateGesture<SyntheticPointerAction,
                           SyntheticPointerActionListParams>(gesture_params);
    default:
      NOTREACHED() << "Invalid synthetic gesture type";
      return nullptr;
  }
}

bool SyntheticGesture::AllowHighFrequencyDispatch() const {
  return true;
}

void SyntheticGesture::WaitForTargetAck(base::OnceClosure callback,
                                        SyntheticGestureTarget* target) const {
  std::move(callback).Run();
}

}  // namespace content

// Copyright 2013 The Chromium Authors
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

SyntheticGesture::SyntheticGesture(
    std::unique_ptr<SyntheticGestureParams> params)
    : params_(std::move(params)) {}

SyntheticGesture::~SyntheticGesture() {}

bool SyntheticGesture::AllowHighFrequencyDispatch() const {
  return true;
}

void SyntheticGesture::WaitForTargetAck(base::OnceClosure callback,
                                        SyntheticGestureTarget* target) const {
  std::move(callback).Run();
}

void SyntheticGesture::DidQueue(
    base::WeakPtr<SyntheticGestureController> controller) {
  CHECK(controller);
  dispatching_controller_ = controller;
}

bool SyntheticGesture::IsFromDevToolsDebugger() const {
  return params_->from_devtools_debugger;
}

}  // namespace content

// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/synthetic_gesture.h"

#include "base/notreached.h"
#include "content/common/input/synthetic_gesture_target.h"

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

float SyntheticGesture::GetVsyncOffsetMs() const {
  return params_->vsync_offset_ms;
}

content::mojom::InputEventPattern SyntheticGesture::InputEventPattern() const {
  return params_->input_event_pattern;
}

}  // namespace content

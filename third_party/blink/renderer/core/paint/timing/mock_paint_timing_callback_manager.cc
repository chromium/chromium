// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/mock_paint_timing_callback_manager.h"

#include "base/check.h"
#include "components/viz/common/frame_timing_details.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/presentation_feedback.h"

namespace blink {

MockPaintTimingCallbackManager::MockPaintTimingCallbackManager() = default;

void MockPaintTimingCallbackManager::RegisterCallback(
    PaintTiming::ReportTimeCallback callback) {
  callbacks_.push_back(std::move(callback));
}

void MockPaintTimingCallbackManager::OnAnimationFrameComplete() {
  // Insert a fence to mark the end of the current frame.
  callbacks_.push_back(BindOnce(
      [](MockPaintTimingCallbackManager* self,
         const viz::FrameTimingDetails& frame_timing_details) {
        self->is_fence_set_ = true;
      },
      WrapWeakPersistent(this)));
}

void MockPaintTimingCallbackManager::InvokeCallbacksForOneAnimationFrame(
    base::TimeTicks presentation_time) {
  while (!callbacks_.empty() && !is_fence_set_) {
    InvokeCallback(presentation_time);
  }
  is_fence_set_ = false;
}

void MockPaintTimingCallbackManager::InvokeCallback(
    base::TimeTicks presentation_time) {
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = presentation_time;
  CHECK(!callbacks_.empty());
  std::move(callbacks_.TakeFirst()).Run(details);
}

}  // namespace blink

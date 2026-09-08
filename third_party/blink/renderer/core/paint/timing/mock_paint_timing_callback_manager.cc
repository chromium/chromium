// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/timing/mock_paint_timing_callback_manager.h"

#include "base/check.h"
#include "base/notreached.h"
#include "components/viz/common/frame_timing_details.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/presentation_feedback.h"

namespace blink {

MockPaintTimingCallbackManager::MockPaintTimingCallbackManager() = default;

void MockPaintTimingCallbackManager::RegisterCallback(
    PaintTiming::ReportTimeCallback callback) {
  current_frame_data_.callbacks.push_back(std::move(callback));
}

void MockPaintTimingCallbackManager::OnAnimationFrameComplete() {
  pending_frame_data_.push_back(std::move(current_frame_data_));
  current_frame_data_ = FrameData();
}

void MockPaintTimingCallbackManager::OnAnimationFramePresented(
    base::TimeTicks presentation_time) {
  for (auto& data : pending_frame_data_) {
    if (data.presentation_time.is_null()) {
      data.presentation_time = presentation_time;
      return;
    }
  }
  NOTREACHED();
}

void MockPaintTimingCallbackManager::InvokeCallbacksForNextAnimationFrame() {
  CHECK(!pending_frame_data_.empty());
  FrameData data = pending_frame_data_.TakeFirst();
  InvokeCallbacksForFrameData(data);
}

void MockPaintTimingCallbackManager::InvokeCallbacksForLastAnimationFrame() {
  CHECK(!pending_frame_data_.empty());
  FrameData data = pending_frame_data_.TakeLast();
  InvokeCallbacksForFrameData(data);
}

void MockPaintTimingCallbackManager::InvokeCallbacksForFrameData(
    FrameData& data) {
  CHECK(!data.presentation_time.is_null())
      << "OnAnimationFramePresented() must be called before invoking callbacks";
  viz::FrameTimingDetails details;
  details.presentation_feedback.timestamp = data.presentation_time;
  for (auto& callback : data.callbacks) {
    std::move(callback).Run(details);
  }
}

void MockPaintTimingCallbackManager::Shutdown() {
  pending_frame_data_.clear();
  current_frame_data_ = FrameData();
}

}  // namespace blink

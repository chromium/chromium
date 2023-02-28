// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/animation_frame_timing_monitor.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"

namespace blink {

namespace {
constexpr base::TimeDelta kLongAnimationFrameDuration = base::Milliseconds(50);
}

AnimationFrameTimingMonitor::AnimationFrameTimingMonitor(Client& client)
    : client_(client) {
  Thread::Current()->AddTaskTimeObserver(this);
}

void AnimationFrameTimingMonitor::Shutdown() {
  Thread::Current()->RemoveTaskTimeObserver(this);
}

void AnimationFrameTimingMonitor::WillBeginMainFrame() {
  base::TimeTicks now = base::TimeTicks::Now();
  if (!current_frame_timing_info_) {
    current_frame_timing_info_ =
        MakeGarbageCollected<AnimationFrameTimingInfo>(now);
  }

  current_frame_timing_info_->SetRenderStartTime(now);
  state_ = State::kRenderingFrame;
}

void AnimationFrameTimingMonitor::WillPerformStyleAndLayoutCalculation() {
  if (state_ != State::kRenderingFrame) {
    return;
  }
  DCHECK(current_frame_timing_info_);
  current_frame_timing_info_->SetStyleAndLayoutStartTime(
      base::TimeTicks::Now());
}

void AnimationFrameTimingMonitor::DidBeginMainFrame() {
  DCHECK(current_frame_timing_info_ && state_ == State::kRenderingFrame);
  current_frame_timing_info_->SetRenderEndTime(base::TimeTicks::Now());
  if (current_frame_timing_info_->Duration() >= kLongAnimationFrameDuration) {
    client_.ReportLongAnimationFrameTiming(current_frame_timing_info_);
  }
  current_frame_timing_info_.Clear();
  state_ = State::kIdle;
}

void AnimationFrameTimingMonitor::WillProcessTask(base::TimeTicks start_time) {
  if (state_ == State::kIdle) {
    state_ = State::kProcessingTask;
  }
}

void AnimationFrameTimingMonitor::OnTaskCompleted(base::TimeTicks start_time,
                                                  base::TimeTicks end_time,
                                                  LocalFrame* frame) {
  if (state_ != State::kProcessingTask) {
    return;
  }

  if (client_.RequestedMainFramePending()) {
    current_frame_timing_info_ =
        MakeGarbageCollected<AnimationFrameTimingInfo>(start_time);
    state_ = State::kPendingFrame;
    return;
  }

  state_ = State::kIdle;

  if (!client_.ShouldReportLongAnimationFrameTiming()) {
    return;
  }

  if (!frame || (end_time - start_time) < kLongAnimationFrameDuration) {
    return;
  }

  AnimationFrameTimingInfo* timing_info =
      MakeGarbageCollected<AnimationFrameTimingInfo>(start_time);
  timing_info->SetRenderEndTime(end_time);
  DOMWindowPerformance::performance(*frame->DomWindow())
      ->ReportLongAnimationFrameTiming(timing_info);
}

void AnimationFrameTimingMonitor::Trace(Visitor* visitor) const {
  visitor->Trace(current_frame_timing_info_);
}

}  // namespace blink

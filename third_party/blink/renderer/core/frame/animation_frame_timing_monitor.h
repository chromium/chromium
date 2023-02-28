// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ANIMATION_FRAME_TIMING_MONITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ANIMATION_FRAME_TIMING_MONITOR_H_

#include "base/task/sequence_manager/task_time_observer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"

namespace base {
class TimeTicks;
}

namespace blink {

class LocalFrame;

// Monitors long-animation-frame timing (LoAF).
// This object is a supplement to a WebFrameWidgetImpl. It handles the state
// machine related to capturing the timing for long animation frames, and
// reporting them back to the frames that observe it.
class CORE_EXPORT AnimationFrameTimingMonitor final
    : public GarbageCollected<AnimationFrameTimingMonitor>,
      public base::sequence_manager::TaskTimeObserver {
 public:
  class Client {
   public:
    virtual void ReportLongAnimationFrameTiming(AnimationFrameTimingInfo*) = 0;
    virtual bool ShouldReportLongAnimationFrameTiming() const = 0;
    virtual bool RequestedMainFramePending() = 0;
  };
  explicit AnimationFrameTimingMonitor(Client&);
  AnimationFrameTimingMonitor(const AnimationFrameTimingMonitor&) = delete;
  AnimationFrameTimingMonitor& operator=(const AnimationFrameTimingMonitor&) =
      delete;

  ~AnimationFrameTimingMonitor() override = default;

  virtual void Trace(Visitor*) const;

  void Shutdown();

  void WillBeginMainFrame();
  void WillPerformStyleAndLayoutCalculation();
  void DidBeginMainFrame();
  void OnTaskCompleted(base::TimeTicks start_time,
                       base::TimeTicks end_time,
                       LocalFrame* frame);

  // TaskTimeObsrver
  void WillProcessTask(base::TimeTicks start_time) override;

  void DidProcessTask(base::TimeTicks start_time,
                      base::TimeTicks end_time) override {
    OnTaskCompleted(start_time, end_time, /*frame=*/nullptr);
  }

 private:
  Member<AnimationFrameTimingInfo> current_frame_timing_info_;
  Client& client_;

  enum class State {
    // No task running, no pending frames.
    kIdle,

    // Task is currently running, might request a frame.
    kProcessingTask,

    // A task has already requested a frame.
    kPendingFrame,

    // Currently rendering, until DidBeginMainFrame.
    kRenderingFrame
  };
  State state_ = State::kIdle;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ANIMATION_FRAME_TIMING_MONITOR_H_

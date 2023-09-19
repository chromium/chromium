// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ANIMATION_FRAME_TIMING_MONITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ANIMATION_FRAME_TIMING_MONITOR_H_

#include "base/task/sequence_manager/task_time_observer.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/frame/frame.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
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
    virtual void ReportLongTaskTiming(base::TimeTicks start,
                                      base::TimeTicks end,
                                      ExecutionContext* context) = 0;
    virtual bool ShouldReportLongAnimationFrameTiming() const = 0;
    virtual bool RequestedMainFramePending() = 0;
    virtual ukm::UkmRecorder* MainFrameUkmRecorder() = 0;
    virtual ukm::SourceId MainFrameUkmSourceId() = 0;
    virtual bool IsMainFrameFullyLoaded() const = 0;
  };
  AnimationFrameTimingMonitor(Client&, CoreProbeSink*);
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
                       base::TimeTicks desired_execution_time,
                       LocalFrame* frame);

  // TaskTimeObsrver
  void WillProcessTask(base::TimeTicks start_time) override;

  void DidProcessTask(base::TimeTicks start_time,
                      base::TimeTicks end_time) override {
    OnTaskCompleted(start_time, end_time, base::TimeTicks(), /*frame=*/nullptr);
  }

  // probes
  void WillHandlePromise(ExecutionContext*,
                         ScriptState*,
                         bool resolving,
                         const char* class_like,
                         const String& property_like,
                         const String& script_url);
  void Will(const probe::CompileAndRunScript&);
  void Did(const probe::CompileAndRunScript&);
  void Will(const probe::ExecuteScript&);
  void Did(const probe::ExecuteScript&);
  void Will(const probe::RecalculateStyle&);
  void Did(const probe::RecalculateStyle&);
  void Will(const probe::UpdateLayout&);
  void Did(const probe::UpdateLayout&);
  void Will(const probe::InvokeCallback&);
  void Did(const probe::InvokeCallback&);
  void Will(const probe::InvokeEventHandler&);
  void Did(const probe::InvokeEventHandler&);
  void WillRunJavaScriptDialog();
  void DidRunJavaScriptDialog();
  void DidFinishSyncXHR(base::TimeDelta);

  void SetDesiredRenderStartTime(base::TimeTicks time) {
    desired_render_start_time_ = time;
  }

 private:
  Member<AnimationFrameTimingInfo> current_frame_timing_info_;
  HeapVector<Member<ScriptTimingInfo>> current_scripts_;
  struct PendingScriptInfo {
    ScriptTimingInfo::Type type;
    base::TimeTicks start_time;
    base::TimeTicks queue_time;
    base::TimeTicks execution_start_time;
    base::TimeDelta style_duration;
    base::TimeDelta layout_duration;
    base::TimeDelta pause_duration;
    int layout_depth = 0;
    const char* class_like_name = nullptr;
    String property_like_name;
    ScriptTimingInfo::ScriptSourceLocation source_location;
  };

  ScriptTimingInfo* DidExecuteScript(const probe::ProbeBase& probe,
                                     ExecutionContext* context);
  ScriptTimingInfo* MaybeAddScript(ExecutionContext* context,
                                   base::TimeTicks end_time);
  void OnMicrotasksCompleted(ExecutionContext*);
  bool ShouldAddScript(ExecutionContext*);
  template <typename Probe>
  ScriptTimingInfo* DidExecuteScript(const Probe& probe) {
    return DidExecuteScript(probe, probe.context);
  }

  void RecordLongAnimationFrameUKMAndTrace(const AnimationFrameTimingInfo&);
  void ApplyTaskDuration(base::TimeDelta task_duration);

  absl::optional<PendingScriptInfo> pending_script_info_;
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

  int user_callback_depth_ = 0;

  base::TimeTicks desired_render_start_time_;
  base::TimeTicks first_ui_event_timestamp_;
  base::TimeTicks javascript_dialog_start_;
  base::TimeDelta total_blocking_time_excluding_longest_task_;
  base::TimeDelta longest_task_duration_;
  bool did_pause_ = false;

  bool enabled_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ANIMATION_FRAME_TIMING_MONITOR_H_

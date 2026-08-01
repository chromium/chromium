// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ANIMATION_FRAME_TIMING_MONITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ANIMATION_FRAME_TIMING_MONITOR_H_

#include <variant>

#include "base/task/sequence_manager/task_time_observer.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"
#include "third_party/blink/renderer/core/timing/performance_entry.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {
class TimeTicks;
}

namespace blink {

// When enabled, long-animation-frame events will always include the sourceURL,
// regardless of protocol. This is useful during development when using `file:`
// URLs or custom protocols defined by embedders.
CORE_EXPORT BASE_DECLARE_FEATURE(kAlwaysLogLOAFURL);

class LocalFrame;

// Monitors long-animation-frame timing (LoAF).
// On the main thread, this object is owned by a WebFrameWidgetImpl (which also
// acts as its Client). It handles the state machine related to capturing the
// timing for long animation frames, and reporting them back to the frames that
// observe it.
// In worker mode (owned by WorkerGlobalScope, which also acts as its Client,
// for a dedicated worker) it has no rendering lifecycle; instead it observes
// the worker's task loop and reports a long task blocking the event loop as a
// congested moment via Client::ReportCongestedMoment().
class CORE_EXPORT AnimationFrameTimingMonitor final
    : public GarbageCollected<AnimationFrameTimingMonitor>,
      public base::sequence_manager::TaskTimeObserver {
 public:
  class Client {
   public:
    virtual void ReportLongTaskTiming(base::TimeTicks start,
                                      base::TimeTicks end,
                                      ExecutionContext* context) = 0;
    virtual void ReportCongestedMoment(AnimationFrameTimingInfo*) {}
    virtual bool ShouldReportLongAnimationFrameTiming() const = 0;
    virtual bool RequestedMainFramePending() = 0;
    virtual ukm::UkmRecorder* MainFrameUkmRecorder() = 0;
    virtual ukm::SourceId MainFrameUkmSourceId() = 0;
  };
  AnimationFrameTimingMonitor(Client&, CoreProbeSink*);
  AnimationFrameTimingMonitor(const AnimationFrameTimingMonitor&) = delete;
  AnimationFrameTimingMonitor& operator=(const AnimationFrameTimingMonitor&) =
      delete;

  ~AnimationFrameTimingMonitor() override = default;

  void Trace(Visitor*) const;

  void Shutdown();

  void BeginMainFrame(LocalDOMWindow& local_root_window,
                      viz::BeginFrameId frame_id);
  void WillPerformStyleAndLayoutCalculation();
  AnimationFrameTimingInfo* RecordRenderingUpdateEndTime(
      LocalDOMWindow& local_root_window,
      base::TimeTicks);
  void OnMainThreadTaskCompleted(base::TimeTicks start_time,
                                 base::TimeTicks end_time,
                                 LocalFrame* frame);

  // TaskTimeObserver
  void WillProcessTask(base::TimeTicks start_time) override;

  // `desired_execution_time` is when the task was meant to run (see
  // TaskMetadata::GetDesiredExecutionTime()); congested-moment detection uses
  // it to measure queuing delay. It is null when the sequence manager does not
  // stamp these times.
  void DidProcessTask(base::TimeTicks start_time,
                      base::TimeTicks end_time,
                      base::TimeTicks desired_execution_time) override;

  // probes
  void WillHandlePromise(ScriptState*,
                         bool resolving,
                         const char* class_like,
                         std::variant<const char*, String> property_like,
                         LazySourceLocation* location);
  void Will(const probe::EvaluateScriptBlock&);
  void Did(const probe::EvaluateScriptBlock& probe_data) {
    PopScriptEntryPoint(&probe_data.script_state, &probe_data);
  }
  void Will(const probe::ExecuteScript&);
  void Did(const probe::ExecuteScript& probe_data) {
    v8::Isolate* isolate = probe_data.context->GetIsolate();
    ScriptState* script_state =
        ScriptState::From(isolate, probe_data.v8_context);
    PopScriptEntryPoint(script_state, &probe_data);
  }
  void Will(const probe::RecalculateStyle&);
  void Did(const probe::RecalculateStyle&);
  void Will(const probe::UpdateLayout&);
  void Did(const probe::UpdateLayout&);
  void Will(const probe::InvokeCallback&);
  void Did(const probe::InvokeCallback& probe_data) {
    PopScriptEntryPoint(&probe_data.script_state, &probe_data);
  }
  void Will(const probe::FrameRelatedTask& probe) { probe.CaptureStartTime(); }
  void Did(const probe::FrameRelatedTask& probe);
  void Will(const probe::UserEntryPoint&);
  void Did(const probe::UserEntryPoint&);
  void Will(const probe::InvokeEventHandler&);
  void Did(const probe::InvokeEventHandler&);
  void WillRunJavaScriptDialog();
  void DidRunJavaScriptDialog();
  void DidFinishSyncXHR(base::TimeDelta);
  void WillHandleInput(LocalFrame*);

  void MarkConditional(const AtomicString& name, base::TimeTicks start_time);

 private:
  Member<AnimationFrameTimingInfo> current_frame_timing_info_;
  HeapVector<Member<ScriptTimingInfo>> current_scripts_;
  Vector<ConditionalMarkInfo> conditional_marks_;
  viz::BeginFrameId current_begin_frame_id_;
  struct PendingScriptInfo {
    ScriptTimingInfo::InvokerType invoker_type;
    base::TimeTicks start_time;
    base::TimeTicks queue_time;
    base::TimeTicks execution_start_time;
    base::TimeDelta style_duration;
    base::TimeDelta layout_duration;
    // Tracks style duration accumulated during layout (e.g. from container
    // query style recalc). This is subtracted from layout_duration when the
    // outermost layout scope ends to avoid double-counting.
    base::TimeDelta style_duration_during_layout;
    base::TimeDelta pause_duration;
    int layout_depth = 0;
    const char* class_like_name = nullptr;
    std::variant<const char*, String> property_like_name;
    ScriptTimingInfo::ScriptSourceLocation source_location;
  };

  ScriptTimingInfo* PopScriptEntryPoint(
      ScriptState* script_state,
      const probe::ProbeBase* probe,
      base::TimeTicks end_time = base::TimeTicks());
  ScriptTimingInfo* PopScriptEntryPointInternal(
      ExecutionContext* context,
      base::TimeTicks end_time,
      const PendingScriptInfo& script_info);

  bool PushScriptEntryPoint(ScriptState*);

  void OnWorkerTaskCompleted(base::TimeTicks start_time,
                             base::TimeTicks end_time,
                             base::TimeTicks desired_execution_time);
  // Accumulates the just-completed task's script attribution (scriptCount and
  // long scripts) into the open congested moment.
  void AccumulateCurrentTaskScripts();
  // Closes the open congested moment, reporting it as an entry only if it
  // stayed saturated for at least the congestion threshold and folded at least
  // one script entry point; then resets the run state. Also called when the
  // queue drains and at Shutdown, so shorter or attribution-less moments are
  // simply discarded.
  // TODO(crbug.com/534893134): also track internal browser tasks (e.g. GC, IPC)
  // that carry no JS attribution, so congestion dominated by native work is not
  // dropped by the scriptCount > 0 guard.
  void FinalizeCongestedMoment();

  void RecordLongAnimationFrameUKMAndTrace(const AnimationFrameTimingInfo&,
                                           LocalDOMWindow& window);
  void RecordLongAnimationFrameTrace(const AnimationFrameTimingInfo& info,
                                     LocalDOMWindow& window);
  void RequestPresentationTimeForTracing(LocalFrame& frame);
  void ReportPresentationTimeToTrace(
      uint64_t trace_id,
      const viz::FrameTimingDetails& presentation_details);
  void ApplyTaskDuration(base::TimeDelta task_duration);

  std::optional<PendingScriptInfo> pending_script_info_;
  HashMap<size_t, PendingScriptInfo> user_entry_points_;
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

  base::TimeTicks first_ui_event_timestamp_;
  base::TimeTicks javascript_dialog_start_;
  base::TimeTicks current_task_start_;
  base::TimeDelta total_blocking_time_excluding_longest_task_;
  base::TimeDelta longest_task_duration_;
  base::TimeDelta render_style_duration_;
  base::TimeDelta render_layout_duration_;
  // Tracks style duration accumulated during render-phase layout (e.g. from
  // container query style recalc). Subtracted from render_layout_duration_
  // when the outermost layout scope ends.
  base::TimeDelta render_style_duration_during_layout_;
  int render_layout_depth_ = 0;
  bool did_pause_ = false;
  bool did_see_ui_events_ = false;
  WeakMember<LocalFrame> frame_handling_input_;
  bool multiple_focused_frames_in_same_task_ = false;

  WeakMember<LocalDOMWindow> task_attributed_window_;
  bool task_has_multiple_attributed_windows_ = false;
  bool task_longtask_reported_ = false;

  unsigned entry_point_depth_ = 0;

  // Top-level script entry points in the current reporting interval, counted
  // regardless of duration (so it can exceed current_scripts_.size()).
  uint32_t script_count_ = 0;

  // Actual start and scheduled start of the previous task, used to decide
  // whether this task was already queued before the previous one began
  // (backlog depth >= 2 => congestion).
  base::TimeTicks prev_task_start_;
  base::TimeTicks prev_scheduled_start_;

  // Open congested moment: the interval during which the task queue stayed
  // congested — a task was scheduled but not yet handled (queued while an
  // earlier task was still running). Such moments bind the tasks together into
  // one potential long animation frame.
  base::TimeTicks congestion_run_start_;
  // End of the most recent task folded into the congested moment. Null when no
  // moment is open.
  base::TimeTicks congestion_run_end_;
  // Top-level script entry points accumulated across the congested moment.
  uint32_t congestion_script_count_ = 0;
  // Long scripts accumulated across the congested moment.
  HeapVector<Member<ScriptTimingInfo>> congestion_scripts_;

  bool enabled_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_ANIMATION_FRAME_TIMING_MONITOR_H_

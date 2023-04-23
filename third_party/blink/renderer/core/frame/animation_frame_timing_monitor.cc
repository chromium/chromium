// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/animation_frame_timing_monitor.h"
#include "base/time/time.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/js_based_event_listener.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"

namespace blink {

namespace {
constexpr base::TimeDelta kLongAnimationFrameDuration = base::Milliseconds(50);
constexpr base::TimeDelta kLongScriptDuration = base::Milliseconds(5);
}  // namespace

AnimationFrameTimingMonitor::AnimationFrameTimingMonitor(Client& client,
                                                         CoreProbeSink* sink)
    : client_(client) {
  Thread::Current()->AddTaskTimeObserver(this);
  sink->AddAnimationFrameTimingMonitor(this);
  enabled_ = true;
}

void AnimationFrameTimingMonitor::Shutdown() {
  enabled_ = false;
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
  DCHECK(!desired_render_start_time_.is_null());
  current_frame_timing_info_->SetRenderEndTime(base::TimeTicks::Now());

  if (did_pause_) {
    current_frame_timing_info_->SetDidPause();
  }
  did_pause_ = false;

  // These would be (non-event) scripts that are handled while rendering, e.g.
  // ResizeObserver and requestAnimationFrame callbacks.
  // Their desired execution time would be set to the frame's desired render
  // start time.
  for (ScriptTimingInfo* script : current_scripts_) {
    if (script->DesiredExecutionStartTime().is_null()) {
      script->SetDesiredExecutionStartTime(desired_render_start_time_);
    }
  }
  current_frame_timing_info_->SetScripts(current_scripts_);
  if (current_frame_timing_info_->Duration() >= kLongAnimationFrameDuration) {
    current_frame_timing_info_->SetDesiredRenderStartTime(
        desired_render_start_time_);
    if (!first_ui_event_timestamp_.is_null()) {
      current_frame_timing_info_->SetFirstUIEventTime(
          first_ui_event_timestamp_);
    }

    // Blocking duration is computed as such:
    // - Count the render duration as part of the longest task's duration
    // - Sum the durations of the long tasks, reducing 50ms from each.
    base::TimeDelta render_duration =
        current_frame_timing_info_->RenderEndTime() -
        current_frame_timing_info_->RenderStartTime();

    base::TimeDelta render_blocking_duration =
        longest_task_duration_ + render_duration;

    base::TimeDelta blocking_duration =
        total_blocking_time_excluding_longest_task_;
    if (render_blocking_duration > kLongAnimationFrameDuration) {
      blocking_duration +=
          render_blocking_duration - kLongAnimationFrameDuration;
    }

    current_frame_timing_info_->SetTotalBlockingDuration(blocking_duration);

    client_.ReportLongAnimationFrameTiming(current_frame_timing_info_);
    RecordLongAnimationFrameUKM(*current_frame_timing_info_);
  }

  desired_render_start_time_ = base::TimeTicks();
  first_ui_event_timestamp_ = base::TimeTicks();
  current_frame_timing_info_.Clear();
  current_scripts_.clear();
  longest_task_duration_ = total_blocking_time_excluding_longest_task_ =
      base::TimeDelta();
  state_ = State::kIdle;
}

void AnimationFrameTimingMonitor::WillProcessTask(base::TimeTicks start_time) {
  if (state_ == State::kIdle) {
    state_ = State::kProcessingTask;
  }
}

void AnimationFrameTimingMonitor::ApplyTaskDuration(
    base::TimeDelta task_duration) {
  // Instead of saving the list of task durations, we keep the sum of durations
  // excluding the longest, and the longest separately, and replace the longest
  // if a newer task duration is longer.
  if (task_duration > longest_task_duration_) {
    // New task duration is now the longest, and we apply the previous longest
    // duration to the sum.
    std::swap(task_duration, longest_task_duration_);
  }

  if (task_duration > kLongAnimationFrameDuration) {
    total_blocking_time_excluding_longest_task_ +=
        task_duration - kLongAnimationFrameDuration;
  }
}

void AnimationFrameTimingMonitor::OnTaskCompleted(
    base::TimeTicks start_time,
    base::TimeTicks end_time,
    base::TimeTicks desired_execution_time,
    LocalFrame* frame) {
  HeapVector<Member<ScriptTimingInfo>> scripts;

  bool did_pause = false;
  std::swap(did_pause, did_pause_);

  base::TimeDelta task_duration = end_time - start_time;

  // If we already need an update and a new task is processed, count its
  // duration towards blockingTime.
  if (frame && state_ == State::kPendingFrame) {
    ApplyTaskDuration(task_duration);
  }

  if (state_ != State::kProcessingTask) {
    return;
  }

  bool should_report = client_.ShouldReportLongAnimationFrameTiming();
  if (should_report && !desired_execution_time.is_null()) {
    // These would be (non-event) scripts that are executed outside of the
    // rendering phase. e.g. a timer callback or deferred script blocks.
    // Their desired execution time would be set to the time the task was
    // posted to the event loop - in the case of a timer this would be the
    // timer expiry time.
    for (ScriptTimingInfo* script : current_scripts_) {
      if (script->DesiredExecutionStartTime().is_null()) {
        script->SetDesiredExecutionStartTime(desired_execution_time);
      }
    }
  }

  if (client_.RequestedMainFramePending() && should_report) {
    current_frame_timing_info_ =
        MakeGarbageCollected<AnimationFrameTimingInfo>(start_time);
    state_ = State::kPendingFrame;
    if (frame) {
      ApplyTaskDuration(task_duration);
    }
    return;
  }

  std::swap(scripts, current_scripts_);
  current_scripts_.clear();
  longest_task_duration_ = total_blocking_time_excluding_longest_task_ =
      base::TimeDelta();

  state_ = State::kIdle;

  if (!should_report) {
    return;
  }

  if (!frame || (task_duration < kLongAnimationFrameDuration)) {
    return;
  }

  AnimationFrameTimingInfo* timing_info =
      MakeGarbageCollected<AnimationFrameTimingInfo>(start_time);
  timing_info->SetRenderEndTime(end_time);
  timing_info->SetScripts(scripts);
  timing_info->SetTotalBlockingDuration(task_duration -
                                        kLongAnimationFrameDuration);
  if (did_pause) {
    timing_info->SetDidPause();
  }

  if (RuntimeEnabledFeatures::LongAnimationFrameTimingEnabled()) {
    DOMWindowPerformance::performance(*frame->DomWindow())
        ->ReportLongAnimationFrameTiming(timing_info);
  }

  if (frame->IsMainFrame()) {
    RecordLongAnimationFrameUKM(*timing_info);
  }
}

void AnimationFrameTimingMonitor::RecordLongAnimationFrameUKM(
    const AnimationFrameTimingInfo& info) {
  if (!RuntimeEnabledFeatures::LongAnimationFrameUKMEnabled()) {
    return;
  }

  ukm::UkmRecorder* recorder = client_.MainFrameUkmRecorder();
  ukm::SourceId source_id = client_.MainFrameUkmSourceId();
  if (!recorder || source_id == ukm::kInvalidSourceId) {
    return;
  }

  ukm::builders::PerformanceAPI_LongAnimationFrame builder(source_id);
  builder.SetDuration_Total(info.Duration().InMilliseconds());
  base::TimeTicks desired_start_time = info.FrameStartTime();
  if (!info.FirstUIEventTime().is_null()) {
    desired_start_time = info.FirstUIEventTime();
  }
  if (!info.DesiredRenderStartTime().is_null()) {
    desired_start_time = info.DesiredRenderStartTime() < desired_start_time
                             ? info.DesiredRenderStartTime()
                             : desired_start_time;
  }

  builder.SetDuration_DelayDefer(
      (info.FrameStartTime() - desired_start_time).InMilliseconds());
  builder.SetDuration_EffectiveBlocking(
      info.TotalBlockingDuration().InMilliseconds());
  builder.SetDuration_StyleAndLayout_RenderPhase(
      (info.RenderEndTime() - info.StyleAndLayoutStartTime()).InMilliseconds());
  base::TimeDelta total_compilation_duration;
  base::TimeDelta total_execution_duration;
  base::TimeDelta total_forced_style_and_layout_duration;
  base::TimeDelta script_type_duration_user_callback;
  base::TimeDelta script_type_duration_event_listener;
  base::TimeDelta script_type_duration_promise_handler;
  base::TimeDelta script_type_duration_script_block;
  for (const Member<ScriptTimingInfo>& script : info.Scripts()) {
    total_compilation_duration +=
        (script->ExecutionStartTime() - script->StartTime());
    base::TimeDelta execution_duration =
        (script->EndTime() - script->ExecutionStartTime());
    total_execution_duration += execution_duration;
    total_forced_style_and_layout_duration += script->StyleDuration();
    total_forced_style_and_layout_duration += script->LayoutDuration();
    switch (script->GetType()) {
      case ScriptTimingInfo::Type::kClassicScript:
      case ScriptTimingInfo::Type::kModuleScript:
      case ScriptTimingInfo::Type::kExecuteScript:
        script_type_duration_script_block += execution_duration;
        break;
      case ScriptTimingInfo::Type::kEventHandler:
        script_type_duration_event_listener += execution_duration;
        break;
      case ScriptTimingInfo::Type::kPromiseResolve:
      case ScriptTimingInfo::Type::kPromiseReject:
        script_type_duration_promise_handler += execution_duration;
        break;
      case ScriptTimingInfo::Type::kUserCallback:
        script_type_duration_user_callback += execution_duration;
        break;
    }
  }
  builder.SetDuration_LongScript_JSCompilation(
      total_compilation_duration.InMilliseconds());
  builder.SetDuration_LongScript_JSExecution(
      total_execution_duration.InMilliseconds());
  builder.SetDuration_LongScript_JSExecution_ScriptBlocks(
      script_type_duration_script_block.InMilliseconds());
  builder.SetDuration_LongScript_JSExecution_EventListeners(
      script_type_duration_event_listener.InMilliseconds());
  builder.SetDuration_LongScript_JSExecution_PromiseHandlers(
      script_type_duration_promise_handler.InMilliseconds());
  builder.SetDuration_LongScript_JSExecution_UserCallbacks(
      script_type_duration_user_callback.InMilliseconds());
  builder.SetDuration_StyleAndLayout_Forced(
      total_forced_style_and_layout_duration.InMilliseconds());
  builder.SetDidPause(info.DidPause());
  builder.Record(recorder);
}

void AnimationFrameTimingMonitor::Trace(Visitor* visitor) const {
  visitor->Trace(current_frame_timing_info_);
  visitor->Trace(current_scripts_);
}

ScriptTimingInfo* AnimationFrameTimingMonitor::MaybeAddScript(
    ExecutionContext* context,
    base::TimeTicks end_time) {
  DCHECK(pending_script_info_);

  if ((end_time - pending_script_info_->start_time) < kLongScriptDuration) {
    pending_script_info_ = absl::nullopt;
    return nullptr;
  }

  ScriptTimingInfo* script_timing_info = MakeGarbageCollected<ScriptTimingInfo>(
      context, pending_script_info_->type, pending_script_info_->start_time,
      pending_script_info_->execution_start_time, end_time,
      pending_script_info_->style_duration,
      pending_script_info_->layout_duration);

  script_timing_info->SetSourceLocation(pending_script_info_->source_location);
  if (pending_script_info_->class_like_name) {
    script_timing_info->SetClassLikeName(pending_script_info_->class_like_name);
  }

  if (pending_script_info_->property_like_name) {
    script_timing_info->SetPropertyLikeName(
        pending_script_info_->property_like_name);
  }

  script_timing_info->SetPauseDuration(pending_script_info_->pause_duration);

  current_scripts_.push_back(script_timing_info);
  pending_script_info_ = absl::nullopt;
  return script_timing_info;
}

bool AnimationFrameTimingMonitor::ShouldAddScript(ExecutionContext* context) {
  return enabled_ && pending_script_info_ && context && context->IsWindow() &&
         client_.ShouldReportLongAnimationFrameTiming() &&
         state_ != State::kIdle;
}

ScriptTimingInfo* AnimationFrameTimingMonitor::DidExecuteScript(
    const probe::ProbeBase& probe,
    ExecutionContext* context) {
  if (pending_script_info_ && pending_script_info_->start_time.is_null()) {
    pending_script_info_->start_time = probe.CaptureStartTime();
  }

  if (!ShouldAddScript(context)) {
    pending_script_info_ = absl::nullopt;
    return nullptr;
  }

  return MaybeAddScript(context, probe.CaptureEndTime());
}

void AnimationFrameTimingMonitor::OnMicrotasksCompleted(
    ExecutionContext* context) {
  if (!ShouldAddScript(context)) {
    pending_script_info_ = absl::nullopt;
    return;
  }

  DCHECK(pending_script_info_->type ==
             ScriptTimingInfo::Type::kPromiseResolve ||
         pending_script_info_->type == ScriptTimingInfo::Type::kPromiseReject);

  MaybeAddScript(context, base::TimeTicks::Now());
}

void AnimationFrameTimingMonitor::WillHandlePromise(
    ExecutionContext* context,
    bool resolving,
    const char* class_like_name,
    const char* property_like_name) {
  // Make sure we only monitor top-level promise resolvers that are outside the
  // update-the-rendering phase (promise resolvers directly handled from a
  // posted task).
  if (!context->IsWindow() || pending_script_info_ ||
      state_ != State::kProcessingTask) {
    return;
  }

  // The only "end" of resolved promises is the end of the microtask queue.
  // A promise can have many callbacks (e.g. called with .then() and .catch()
  // multiple times) and those callbacks are "mixed" in the microtask queue.
  DCHECK(context->GetAgent());
  DCHECK(context->GetAgent()->event_loop());
  context->GetAgent()->event_loop()->EnqueueEndOfMicrotaskCheckpointTask(
      WTF::BindOnce(
          [](WeakPersistent<AnimationFrameTimingMonitor> self,
             WeakPersistent<ExecutionContext> context) {
            if (self && context) {
              self->OnMicrotasksCompleted(context);
            }
          },
          WrapWeakPersistent(this), WrapWeakPersistent(context)));

  base::TimeTicks now = base::TimeTicks::Now();
  pending_script_info_ = PendingScriptInfo{
      .type = resolving ? ScriptTimingInfo::Type::kPromiseResolve
                        : ScriptTimingInfo::Type::kPromiseReject,
      .start_time = now,
      .execution_start_time = now,
      .class_like_name = class_like_name,
      .property_like_name = property_like_name};
}

void AnimationFrameTimingMonitor::Will(
    const probe::CompileAndRunScript& probe_data) {
  if (!probe_data.context->IsWindow()) {
    return;
  }

  KURL url = probe_data.script->SourceUrl();
  if (url.IsEmpty() || url.IsNull()) {
    url = probe_data.context->Url();
  }

  pending_script_info_ =
      PendingScriptInfo{.type = probe_data.script->GetScriptType() ==
                                        mojom::blink::ScriptType::kClassic
                                    ? ScriptTimingInfo::Type::kClassicScript
                                    : ScriptTimingInfo::Type::kModuleScript,
                        .start_time = probe_data.CaptureStartTime(),
                        .source_location = {.url = url}};
}

void AnimationFrameTimingMonitor::Did(
    const probe::CompileAndRunScript& probe_data) {
  if (!pending_script_info_ ||
      (pending_script_info_->type != ScriptTimingInfo::Type::kClassicScript &&
       pending_script_info_->type != ScriptTimingInfo::Type::kModuleScript)) {
    return;
  }

  if (ScriptTimingInfo* script_timing_info = DidExecuteScript(probe_data)) {
    script_timing_info->SetSourceLocation(
        ScriptTimingInfo::ScriptSourceLocation{
            .url = probe_data.script->SourceUrl(),
            .line_number = static_cast<unsigned int>(
                probe_data.script->StartPosition().line_.OneBasedInt()),
            .column_number = static_cast<unsigned int>(
                probe_data.script->StartPosition().column_.OneBasedInt()),
        });
  }
}
void AnimationFrameTimingMonitor::WillRunJavaScriptDialog() {
  javascript_dialog_start_ = base::TimeTicks::Now();
  did_pause_ = true;
}
void AnimationFrameTimingMonitor::DidRunJavaScriptDialog() {
  // javascript_dialog_start_ can be null if DidRunJavaScriptDialog was run
  // without WillRunJavaScriptDialog, which can happen in the case of
  // WebView/browser-initiated dialogs.
  if (!pending_script_info_ || javascript_dialog_start_.is_null()) {
    return;
  }

  pending_script_info_->pause_duration +=
      (base::TimeTicks::Now() - javascript_dialog_start_);
  javascript_dialog_start_ = base::TimeTicks();
}

void AnimationFrameTimingMonitor::DidFinishSyncXHR(
    base::TimeDelta blocking_time) {
  if (pending_script_info_) {
    pending_script_info_->pause_duration += blocking_time;
  }

  // We record did_pause_ regardless of having long scripts (e.g. short scripts
  // with a sync XHR.
  did_pause_ = true;
}

void AnimationFrameTimingMonitor::Will(const probe::ExecuteScript& probe_data) {
  // In some cases we get here without a CompileAndRunScript, e.g. when
  // executing an imported module script.
  if (!pending_script_info_) {
    pending_script_info_ =
        PendingScriptInfo{.type = ScriptTimingInfo::Type::kExecuteScript};
  }

  // This is true for both imported and element-created scripts.
  pending_script_info_->execution_start_time = probe_data.CaptureStartTime();
  if (!pending_script_info_->source_location.url) {
    pending_script_info_->source_location = {.url = probe_data.script_url};
  }
}

void AnimationFrameTimingMonitor::Did(const probe::ExecuteScript& probe_data) {
  if (pending_script_info_ &&
      pending_script_info_->type == ScriptTimingInfo::Type::kExecuteScript) {
    DidExecuteScript(probe_data);
  }
}

void AnimationFrameTimingMonitor::Will(
    const probe::RecalculateStyle& probe_data) {
  if (pending_script_info_) {
    probe_data.CaptureStartTime();
  }
}
void AnimationFrameTimingMonitor::Did(
    const probe::RecalculateStyle& probe_data) {
  if (pending_script_info_) {
    probe_data.CaptureEndTime();
    pending_script_info_->style_duration += probe_data.Duration();
  }
}
void AnimationFrameTimingMonitor::Will(const probe::UpdateLayout& probe_data) {
  if (!pending_script_info_) {
    return;
  }

  if (!pending_script_info_->layout_depth) {
    probe_data.CaptureStartTime();
  }

  pending_script_info_->layout_depth++;
}

void AnimationFrameTimingMonitor::Did(const probe::UpdateLayout& probe_data) {
  if (!pending_script_info_) {
    return;
  }

  pending_script_info_->layout_depth--;

  if (!pending_script_info_->layout_depth) {
    probe_data.CaptureEndTime();
    pending_script_info_->layout_duration += probe_data.Duration();
  }
}

void AnimationFrameTimingMonitor::Will(
    const probe::InvokeCallback& probe_data) {
  // Callbacks can be recursive. We only want the top-level one. We need to
  // keep track of the depth so that we report only when the top-levle one is
  // done.
  user_callback_depth_++;
  if (pending_script_info_) {
    return;
  }

  if (!probe_data.context->IsWindow() ||
      !client_.ShouldReportLongAnimationFrameTiming()) {
    return;
  }
  pending_script_info_ =
      PendingScriptInfo{.type = ScriptTimingInfo::Type::kUserCallback,
                        .start_time = probe_data.CaptureStartTime(),
                        .execution_start_time = probe_data.CaptureStartTime()};
}

namespace {

ScriptTimingInfo::ScriptSourceLocation CaptureScriptSourceLocation(
    v8::MaybeLocal<v8::Value> maybe_value) {
  v8::Local<v8::Value> value;

  if (!maybe_value.ToLocal(&value)) {
    return ScriptTimingInfo::ScriptSourceLocation();
  }

  if (!value->IsFunction()) {
    return ScriptTimingInfo::ScriptSourceLocation();
  }

  v8::Local<v8::Value> bound = value.As<v8::Function>()->GetBoundFunction();
  if (bound.IsEmpty() || !bound->IsFunction()) {
    return ScriptTimingInfo::ScriptSourceLocation();
  }

  if (std::unique_ptr<SourceLocation> location =
          CaptureSourceLocation(bound.As<v8::Function>())) {
    return ScriptTimingInfo::ScriptSourceLocation{
        location->Url(), location->Function(), location->LineNumber(),
        location->ColumnNumber()};
  }

  return ScriptTimingInfo::ScriptSourceLocation();
}

}  // namespace

void AnimationFrameTimingMonitor::Did(const probe::InvokeCallback& probe_data) {
  user_callback_depth_--;
  if (user_callback_depth_) {
    return;
  }

  ScriptTimingInfo* info = DidExecuteScript(probe_data);
  if (!info) {
    return;
  }

  info->SetPropertyLikeName(probe_data.name);
  v8::HandleScope handle_scope(probe_data.context->GetIsolate());
  if (probe_data.callback) {
    info->SetSourceLocation(
        CaptureScriptSourceLocation(probe_data.callback->CallbackObject()));
  } else {
    info->SetSourceLocation(CaptureScriptSourceLocation(probe_data.function));
  }
}

void AnimationFrameTimingMonitor::Will(
    const probe::InvokeEventHandler& probe_data) {
  user_callback_depth_++;
  if (pending_script_info_) {
    return;
  }

  if (!probe_data.context->IsWindow() ||
      !client_.ShouldReportLongAnimationFrameTiming()) {
    return;
  }
  pending_script_info_ =
      PendingScriptInfo{.type = ScriptTimingInfo::Type::kEventHandler,
                        .start_time = probe_data.CaptureStartTime(),
                        .execution_start_time = probe_data.CaptureStartTime()};
}

void AnimationFrameTimingMonitor::Did(
    const probe::InvokeEventHandler& probe_data) {
  user_callback_depth_--;
  if (user_callback_depth_) {
    return;
  }

  if (probe_data.event->IsUIEvent() && first_ui_event_timestamp_.is_null()) {
    first_ui_event_timestamp_ = probe_data.event->PlatformTimeStamp();
  }

  ScriptTimingInfo* info = DidExecuteScript(probe_data);
  if (!info) {
    return;
  }

  info->SetPropertyLikeName(probe_data.event->type());
  info->SetDesiredExecutionStartTime(probe_data.event->PlatformTimeStamp());
  if (Node* node = probe_data.event_target->ToNode()) {
    StringBuilder builder;
    builder.Append(node->nodeName());
    if (Element* element = DynamicTo<Element>(node)) {
      if (element->HasID()) {
        builder.Append("#");
        builder.Append(element->GetIdAttribute());
      } else if (element->hasAttribute(html_names::kSrcAttr)) {
        builder.Append("[src=");
        builder.Append(element->getAttribute(html_names::kSrcAttr));
        builder.Append("]");
      }
    }

    info->SetClassLikeName(builder.ToAtomicString());
  } else {
    info->SetClassLikeName(probe_data.event_target->InterfaceName());
  }

  if (!probe_data.listener->IsJSBasedEventListener()) {
    return;
  }

  v8::HandleScope handle_scope(probe_data.context->GetIsolate());
  info->SetSourceLocation(CaptureScriptSourceLocation(
      To<JSBasedEventListener>(probe_data.listener)
          ->GetListenerObject(*probe_data.event_target)));
}

}  // namespace blink

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/animation_frame_timing_monitor.h"

#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "components/viz/common/frame_timing_details.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink-forward.h"
#include "third_party/blink/renderer/bindings/core/v8/js_based_event_listener.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/third_party_script_detector.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/event_loop.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "v8-local-handle.h"
#include "v8-message.h"

namespace blink {

namespace {
constexpr base::TimeDelta kLongAnimationFrameDuration = base::Milliseconds(50);
constexpr base::TimeDelta kLongTaskDuration = base::Milliseconds(50);
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
  frame_handling_input_ = nullptr;
  Thread::Current()->RemoveTaskTimeObserver(this);
}

void AnimationFrameTimingMonitor::WillHandleInput(LocalFrame* frame) {
  CHECK(frame);
  if (frame == frame_handling_input_) {
    return;
  }

  if (frame_handling_input_) {
    multiple_focused_frames_in_same_task_ = true;
  }

  frame_handling_input_ = frame;
}

void AnimationFrameTimingMonitor::BeginMainFrame(
    LocalDOMWindow& local_root_window) {
  base::TimeTicks now = base::TimeTicks::Now();
  if (!current_frame_timing_info_) {
    current_frame_timing_info_ =
        MakeGarbageCollected<AnimationFrameTimingInfo>(now);
  }

  current_frame_timing_info_->SetRenderStartTime(now);
  state_ = State::kRenderingFrame;
  ApplyTaskDuration(now - current_task_start_);

  RequestPresentationTimeForTracing(*local_root_window.GetFrame());
}

void AnimationFrameTimingMonitor::WillPerformStyleAndLayoutCalculation() {
  if (state_ != State::kRenderingFrame) {
    return;
  }
  DCHECK(current_frame_timing_info_);
  current_frame_timing_info_->SetStyleAndLayoutStartTime(
      base::TimeTicks::Now());
}

void AnimationFrameTimingMonitor::DidBeginMainFrame(
    LocalDOMWindow& local_root_window) {
  // This can happen if the AnimationFrameTimingMonitor instance is created
  // in the middle of a frame.
  if (!current_frame_timing_info_) {
    return;
  }

  CHECK(state_ == State::kRenderingFrame);
  current_frame_timing_info_->SetRenderEndTime(base::TimeTicks::Now());

  if (did_pause_) {
    current_frame_timing_info_->SetDidPause();
  }
  did_pause_ = false;

  current_frame_timing_info_->SetScripts(current_scripts_);
  if (current_frame_timing_info_->Duration() >= kLongAnimationFrameDuration) {
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
  }
  RecordLongAnimationFrameUKMAndTrace(*current_frame_timing_info_,
                                      local_root_window);

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
  current_task_start_ = start_time;
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
    LocalFrame* frame) {
  HeapVector<Member<ScriptTimingInfo>> scripts;

  bool did_pause = false;
  bool did_see_ui_events = false;
  bool multiple_focused_frames_in_same_task = false;
  std::swap(did_pause, did_pause_);
  std::swap(did_see_ui_events, did_see_ui_events_);
  std::swap(multiple_focused_frames_in_same_task,
            multiple_focused_frames_in_same_task_);

  // Input tasks are not attributed to a frame, so we manually attribute it to
  // the focused frame as received from WebFrameWidgetImpl.
  LocalFrame* frame_handling_input = frame_handling_input_.Release();
  if (!frame) {
    frame = frame_handling_input;
  }

  current_task_start_ = base::TimeTicks();

  base::TimeDelta task_duration = end_time - start_time;
  if (pending_script_info_ &&
      ((pending_script_info_->invoker_type ==
        ScriptTimingInfo::InvokerType::kPromiseResolve) ||
       (pending_script_info_->invoker_type ==
        ScriptTimingInfo::InvokerType::kPromiseReject))) {
    if (frame && !frame->IsDetached()) {
      DCHECK(frame->DomWindow());
      PopScriptEntryPoint(ToScriptStateForMainWorld(frame),
                          /*probe_data=*/nullptr, end_time);
    }
  }
  entry_point_depth_ = 0;
  pending_script_info_ = std::nullopt;

  if (RuntimeEnabledFeatures::LongTaskFromLongAnimationFrameEnabled() &&
      frame && frame->DomWindow() && task_duration >= kLongTaskDuration) {
    client_.ReportLongTaskTiming(start_time, end_time, frame->DomWindow());
  }

  // If we already need an update and a new task is processed, count its
  // duration towards blockingTime.
  if ((frame || did_see_ui_events) && (state_ == State::kPendingFrame)) {
    ApplyTaskDuration(task_duration);
  }

  if (state_ != State::kProcessingTask) {
    return;
  }

  bool should_report = client_.ShouldReportLongAnimationFrameTiming();

  // Changing the focused frame mid-task should also schedule rendering.
  // Marking as DUMP_WILL_BE_CHECK because failing this assumption is not
  // critical.
  // TODO(crbug/352077677): Verify this assumption if no dumps are created and
  // turn into a CHECK.
  DUMP_WILL_BE_CHECK(!multiple_focused_frames_in_same_task_ ||
                     client_.RequestedMainFramePending());

  if (client_.RequestedMainFramePending() && should_report) {
    current_frame_timing_info_ =
        MakeGarbageCollected<AnimationFrameTimingInfo>(start_time);
    state_ = State::kPendingFrame;
    if (frame || did_see_ui_events) {
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

  if (RuntimeEnabledFeatures::LongAnimationFrameTimingEnabled(
          frame->DomWindow())) {
    DOMWindowPerformance::performance(*frame->DomWindow())
        ->ReportLongAnimationFrameTiming(timing_info);
  }
  RecordLongAnimationFrameUKMAndTrace(*timing_info, *frame->DomWindow());
}

namespace {

perfetto::protos::pbzero::AnimationFrameScriptTimingInfo::InvokerType
ToProtoEnum(ScriptTimingInfo::InvokerType type) {
  using ProtoType =
      perfetto::protos::pbzero::AnimationFrameScriptTimingInfo::InvokerType;
  switch (type) {
    case ScriptTimingInfo::InvokerType::kClassicScript:
      return ProtoType::CLASSIC_SCRIPT;
    case ScriptTimingInfo::InvokerType::kModuleScript:
      return ProtoType::MODULE_SCRIPT;
    case ScriptTimingInfo::InvokerType::kUserCallback:
      return ProtoType::USER_CALLBACK;
    case ScriptTimingInfo::InvokerType::kEventHandler:
      return ProtoType::EVENT_HANDLER;
    case ScriptTimingInfo::InvokerType::kPromiseResolve:
      return ProtoType::PROMISE_RESOLVE;
    case ScriptTimingInfo::InvokerType::kPromiseReject:
      return ProtoType::PROMISE_REJECT;
  }
  return ProtoType::UNDEFINED;
}

perfetto::protos::pbzero::AnimationFrameScriptTimingInfo::ThirdPartyTechnology
ToProtoEnum(ThirdPartyScriptDetector::Technology technology) {
  return static_cast<perfetto::protos::pbzero::AnimationFrameScriptTimingInfo::
                         ThirdPartyTechnology>(
      std::bit_width(static_cast<uint64_t>(technology)) + 1);
}

}  // namespace

void AnimationFrameTimingMonitor::RequestPresentationTimeForTracing(
    LocalFrame& frame) {
  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("devtools.timeline", &tracing_enabled);
  if (tracing_enabled) {
    frame.GetChromeClient().NotifyPresentationTime(
        frame, CrossThreadBindOnce(
                   &AnimationFrameTimingMonitor::ReportPresentationTimeToTrace,
                   WrapCrossThreadWeakPersistent(this),
                   current_frame_timing_info_->GetTraceId()));
  }
}

void AnimationFrameTimingMonitor::ReportPresentationTimeToTrace(
    uint64_t trace_id,
    const viz::FrameTimingDetails& presentation_details) {
  auto track_id = perfetto::Track::ThreadScoped(this);
  auto flow_id = perfetto::Flow::ProcessScoped(trace_id);
  TRACE_EVENT_INSTANT("devtools.timeline", "AnimationFrame::Presentation",
                      track_id,
                      presentation_details.presentation_feedback.timestamp,
                      flow_id, "id", String::Format("%016" PRIx64, trace_id));
}

void AnimationFrameTimingMonitor::RecordLongAnimationFrameTrace(
    const AnimationFrameTimingInfo& info,
    LocalDOMWindow& window) {
  bool tracing_enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("devtools.timeline", &tracing_enabled);
  if (!tracing_enabled) {
    return;
  }

  uint64_t trace_id = info.GetTraceId();
  auto track_id = perfetto::Track::ThreadScoped(this);
  auto flow_id = perfetto::Flow::ProcessScoped(trace_id);
  if (!info.FirstUIEventTime().is_null()) {
    TRACE_EVENT_INSTANT("devtools.timeline", "AnimationFrame::FirstUIEvent",
                        track_id, info.FirstUIEventTime(), flow_id);
  }
  TRACE_EVENT_BEGIN(
      "devtools.timeline", "AnimationFrame", track_id, info.FrameStartTime(),
      flow_id, "id", String::Format("%016" PRIx64, trace_id),
      [&](perfetto::EventContext ctx) {
        auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
        auto* data = event->set_animation_frame_timing_info();
        data->set_blocking_duration_ms(
            info.TotalBlockingDuration().InMilliseconds());
        data->set_duration_ms(info.Duration().InMilliseconds());
        data->set_num_scripts(info.Scripts().size());
      });
  for (ScriptTimingInfo* script : info.Scripts()) {
    if (script->StartTime() < script->ExecutionStartTime()) {
      TRACE_EVENT_BEGIN("devtools.timeline", "AnimationFrame::Script::Compile",
                        track_id, script->StartTime());
      TRACE_EVENT_END("devtools.timeline", track_id,
                      script->ExecutionStartTime());
    }
    ThirdPartyScriptDetector::Technology third_party_technology =
        ThirdPartyScriptDetector::From(window).Detect(
            script->GetSourceLocation().url);
    TRACE_EVENT_BEGIN(
        "devtools.timeline", "AnimationFrame::Script::Execute", track_id,
        script->ExecutionStartTime(), [&](perfetto::EventContext ctx) {
          auto* event = ctx.event<perfetto::protos::pbzero::ChromeTrackEvent>();
          auto* data = event->set_animation_frame_script_timing_info();
          data->set_style_duration_ms(script->StyleDuration().InMilliseconds());
          data->set_layout_duration_ms(
              script->LayoutDuration().InMilliseconds());
          data->set_pause_duration_ms(script->PauseDuration().InMilliseconds());
          data->set_class_like_name(script->ClassLikeName().Utf8());
          data->set_property_like_name(script->PropertyLikeName().Utf8());
          data->set_source_location_url(script->GetSourceLocation().url.Utf8());
          data->set_source_location_function_name(
              script->GetSourceLocation().function_name.Utf8());
          data->set_source_location_char_position(
              script->GetSourceLocation().char_position);
          data->set_invoker_type(ToProtoEnum(script->GetInvokerType()));
          data->set_third_party_technology(ToProtoEnum(third_party_technology));
        });
    TRACE_EVENT_END("devtools.timeline", track_id, script->EndTime());
  }
  if (!info.RenderStartTime().is_null()) {
    TRACE_EVENT_BEGIN("devtools.timeline", "AnimationFrame::Render", track_id,
                      info.RenderStartTime());
    TRACE_EVENT_END("devtools.timeline", track_id, info.RenderEndTime());
  }
  if (!info.StyleAndLayoutStartTime().is_null()) {
    TRACE_EVENT_BEGIN("devtools.timeline", "AnimationFrame::StyleAndLayout",
                      track_id, info.StyleAndLayoutStartTime());
    TRACE_EVENT_END("devtools.timeline", track_id, info.RenderEndTime());
  }

  TRACE_EVENT_END("devtools.timeline", track_id, info.RenderEndTime());
}

void AnimationFrameTimingMonitor::RecordLongAnimationFrameUKMAndTrace(
    const AnimationFrameTimingInfo& info,
    LocalDOMWindow& window) {
  // Record all animation frames to traces, but only long ones to UKM.
  RecordLongAnimationFrameTrace(info, window);
  if (info.Duration() < kLongAnimationFrameDuration) {
    return;
  }

  ukm::UkmRecorder* recorder = client_.MainFrameUkmRecorder();
  ukm::SourceId source_id = client_.MainFrameUkmSourceId();
  if (!recorder || source_id == ukm::kInvalidSourceId) {
    return;
  }

  ukm::builders::PerformanceAPI_LongAnimationFrame builder(source_id);
  builder.SetDuration_Total(info.Duration().InMilliseconds());
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
  uint64_t third_party_script_callback_contributors = 0;
  uint64_t third_party_script_execution_contributors = 0;
  for (const Member<ScriptTimingInfo>& script : info.Scripts()) {
    total_compilation_duration +=
        (script->ExecutionStartTime() - script->StartTime());
    base::TimeDelta execution_duration =
        (script->EndTime() - script->ExecutionStartTime());
    total_execution_duration += execution_duration;
    total_forced_style_and_layout_duration += script->StyleDuration();
    total_forced_style_and_layout_duration += script->LayoutDuration();
    ThirdPartyScriptDetector::Technology third_party_technology =
        ThirdPartyScriptDetector::From(window).Detect(
            script->GetSourceLocation().url);
    uint64_t technology_bits = static_cast<uint64_t>(third_party_technology);
    switch (script->GetInvokerType()) {
      case ScriptTimingInfo::InvokerType::kClassicScript:
      case ScriptTimingInfo::InvokerType::kModuleScript:
        script_type_duration_script_block += execution_duration;
        third_party_script_execution_contributors |= technology_bits;
        break;
      case ScriptTimingInfo::InvokerType::kEventHandler:
        script_type_duration_event_listener += execution_duration;
        third_party_script_callback_contributors |= technology_bits;
        break;
      case ScriptTimingInfo::InvokerType::kPromiseResolve:
      case ScriptTimingInfo::InvokerType::kPromiseReject:
        script_type_duration_promise_handler += execution_duration;
        third_party_script_callback_contributors |= technology_bits;
        break;
      case ScriptTimingInfo::InvokerType::kUserCallback:
        script_type_duration_user_callback += execution_duration;
        third_party_script_callback_contributors |= technology_bits;
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
  builder.SetCategorized3PScriptLongAnimationFrameCallbackContributors(
      third_party_script_callback_contributors);
  builder.SetCategorized3PScriptLongAnimationFrameScriptExecutionContributors(
      third_party_script_execution_contributors);
  builder.Record(recorder);
}

void AnimationFrameTimingMonitor::Trace(Visitor* visitor) const {
  visitor->Trace(current_frame_timing_info_);
  visitor->Trace(current_scripts_);
  visitor->Trace(frame_handling_input_);
}

namespace {
bool ShouldAllowScriptURL(const WTF::String& url) {
  KURL kurl(url);
  return kurl.ProtocolIsData() || kurl.ProtocolIsInHTTPFamily() ||
         kurl.ProtocolIs("blob") || kurl.IsEmpty();
}

}  // namespace

bool AnimationFrameTimingMonitor::PushScriptEntryPoint(
    ScriptState* script_state) {
  entry_point_depth_++;
  // This will return true if there's a potential long animation frame, i.e.
  // we're in a visible window, and this is the script entry point rather than
  // a nested script (entry_point_depth is 1).
  return enabled_ && entry_point_depth_ == 1 &&
         script_state->World().IsMainWorld() &&
         ToExecutionContext(script_state)->IsWindow() &&
         client_.ShouldReportLongAnimationFrameTiming();
}

ScriptTimingInfo* AnimationFrameTimingMonitor::PopScriptEntryPoint(
    ScriptState* script_state,
    const probe::ProbeBase* probe,
    base::TimeTicks end_time) {
  if (!entry_point_depth_) {
    return nullptr;
  }
  entry_point_depth_--;
  if (entry_point_depth_ > 0 || !pending_script_info_) {
    return nullptr;
  }

  std::optional<PendingScriptInfo> script_info;
  std::swap(script_info, pending_script_info_);

  // script_state can be null in situations such as the frame being in a
  // provisional state.
  ExecutionContext* context =
      script_state ? ToExecutionContext(script_state) : nullptr;

  if (!enabled_ || !context || !context->IsWindow() ||
      !client_.ShouldReportLongAnimationFrameTiming()) {
    return nullptr;
  }

  CHECK(probe || !end_time.is_null());

  if (probe && end_time.is_null()) {
    end_time = probe->CaptureEndTime();
  }

  if ((end_time - script_info->start_time) < kLongScriptDuration) {
    return nullptr;
  }

  if (!ShouldAllowScriptURL(script_info->source_location.url) ||
      state_ == State::kIdle) {
    return nullptr;
  }

  ScriptTimingInfo* script_timing_info = MakeGarbageCollected<ScriptTimingInfo>(
      context, script_info->invoker_type, script_info->start_time,
      script_info->execution_start_time, end_time, script_info->style_duration,
      script_info->layout_duration);

  script_timing_info->SetSourceLocation(script_info->source_location);
  if (script_info->class_like_name) {
    script_timing_info->SetClassLikeName(
        AtomicString(script_info->class_like_name));
  }

  if (const auto* property_name =
          std::get_if<const char*>(&script_info->property_like_name)) {
    script_timing_info->SetPropertyLikeName(AtomicString(*property_name));
  } else if (auto* property_name_string =
                 std::get_if<String>(&script_info->property_like_name)) {
    if (!property_name_string->IsNull()) {
      script_timing_info->SetPropertyLikeName(
          AtomicString(*property_name_string));
    }
  }

  script_timing_info->SetPauseDuration(script_info->pause_duration);

  current_scripts_.push_back(script_timing_info);
  return script_timing_info;
}

void AnimationFrameTimingMonitor::WillHandlePromise(
    ScriptState* script_state,
    bool resolving,
    const char* class_like_name,
    std::variant<const char*, String> property_like_name,
    const String& script_url) {
  // Unlike other script entry points, promise resolvers don't have a "Did"
  // probe, so we keep its depth at 1 and reset only at task end.
  if (entry_point_depth_) {
    return;
  }

  if (!PushScriptEntryPoint(script_state)) {
    return;
  }

  // Make sure we only monitor top-level promise resolvers that are outside the
  // update-the-rendering phase (promise resolvers directly handled from a
  // posted task).
  if (state_ != State::kProcessingTask) {
    return;
  }

  base::TimeTicks now = base::TimeTicks::Now();
  pending_script_info_ = PendingScriptInfo{
      .invoker_type = resolving ? ScriptTimingInfo::InvokerType::kPromiseResolve
                                : ScriptTimingInfo::InvokerType::kPromiseReject,
      .start_time = now,
      .execution_start_time = now,
      .class_like_name = class_like_name,
      .property_like_name = property_like_name,
      .source_location = {.url = script_url}};
}

void AnimationFrameTimingMonitor::Will(
    const probe::EvaluateScriptBlock& probe_data) {
  if (!PushScriptEntryPoint(probe_data.script_state)) {
    return;
  }
  KURL url(probe_data.source_url);
  if (url.IsEmpty() || url.IsNull()) {
    url = ToExecutionContext(probe_data.script_state)->Url();
  }

  pending_script_info_ = PendingScriptInfo{
      .invoker_type = probe_data.is_module
                          ? ScriptTimingInfo::InvokerType::kModuleScript
                          : ScriptTimingInfo::InvokerType::kClassicScript,
      .start_time = probe_data.CaptureStartTime(),
      .source_location = {.url = url, .char_position = 0}};
  if (probe_data.sanitize) {
    pending_script_info_->execution_start_time =
        pending_script_info_->start_time;
  }
}

void AnimationFrameTimingMonitor::Will(const probe::ExecuteScript& probe_data) {
  // In some cases we get here without a EvaluateScriptBlock, e.g. when
  // executing an imported module script.
  // This is true for both imported and element-created scripts.
  v8::Isolate* isolate = probe_data.context->GetIsolate();
  ScriptState* script_state = ScriptState::From(isolate, probe_data.v8_context);
  if (PushScriptEntryPoint(script_state)) {
    pending_script_info_ = PendingScriptInfo{
        .invoker_type = ScriptTimingInfo::InvokerType::kModuleScript,
        .start_time = probe_data.CaptureStartTime(),
        .source_location = {.url = probe_data.script_url}};
  }

  if (pending_script_info_ &&
      pending_script_info_->execution_start_time.is_null()) {
    pending_script_info_->execution_start_time = probe_data.CaptureStartTime();
  }
}

namespace {

ScriptTimingInfo::ScriptSourceLocation CaptureScriptSourceLocation(
    v8::Isolate* isolate,
    v8::MaybeLocal<v8::Value> maybe_value) {
  v8::Local<v8::Value> value;

  if (!maybe_value.ToLocal(&value)) {
    return ScriptTimingInfo::ScriptSourceLocation();
  }

  if (!value->IsFunction()) {
    return ScriptTimingInfo::ScriptSourceLocation();
  }

  v8::Local<v8::Value> bound = value.As<v8::Function>()->GetBoundFunction();
  if (!bound.IsEmpty() && bound->IsFunction()) {
    value = bound;
  }

  v8::Local<v8::Function> function = value.As<v8::Function>();
  if (!function->IsFunction()) {
    return ScriptTimingInfo::ScriptSourceLocation();
  }

  v8::ScriptOrigin origin = function->GetScriptOrigin();

  ScriptTimingInfo::ScriptSourceLocation source_location{
      .url =
          ToCoreStringWithUndefinedOrNullCheck(isolate, origin.ResourceName())};

  // Opaque scripts don't report character index/function name.
  if (origin.Options().IsOpaque()) {
    return source_location;
  }

  source_location.function_name =
      ToCoreStringWithUndefinedOrNullCheck(isolate, function->GetName());
  source_location.char_position = function->GetScriptStartPosition();
  return source_location;
}

}  // namespace

void AnimationFrameTimingMonitor::Will(
    const probe::InvokeCallback& probe_data) {
  if (!PushScriptEntryPoint(probe_data.script_state)) {
    return;
  }

  ScriptState::Scope scope(probe_data.script_state);
  pending_script_info_ = PendingScriptInfo{
      .invoker_type = ScriptTimingInfo::InvokerType::kUserCallback,
      .start_time = probe_data.CaptureStartTime(),
      .execution_start_time = probe_data.CaptureStartTime(),
      .property_like_name = probe_data.name,
      .source_location = CaptureScriptSourceLocation(
          probe_data.script_state->GetIsolate(),
          probe_data.callback ? probe_data.callback->CallbackObject()
                              : probe_data.function)};
}

void AnimationFrameTimingMonitor::Will(
    const probe::InvokeEventHandler& probe_data) {
  ScriptState::Scope scope(probe_data.script_state);
  if (!PushScriptEntryPoint(probe_data.script_state)) {
    return;
  }

  pending_script_info_ = PendingScriptInfo{
      .invoker_type = ScriptTimingInfo::InvokerType::kEventHandler,
      .start_time = probe_data.CaptureStartTime(),
      .execution_start_time = probe_data.CaptureStartTime()};
}

void AnimationFrameTimingMonitor::Did(
    const probe::InvokeEventHandler& probe_data) {
  if (probe_data.event->IsUIEvent() && first_ui_event_timestamp_.is_null()) {
    first_ui_event_timestamp_ = probe_data.event->PlatformTimeStamp();
  }
  did_see_ui_events_ = true;

  ScriptTimingInfo* info =
      PopScriptEntryPoint(probe_data.script_state, &probe_data);
  if (!info) {
    return;
  }

  info->SetPropertyLikeName(probe_data.event->type());
  EventTarget* target = probe_data.event->currentTarget();
  if (Node* node = target->ToNode()) {
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
    info->SetClassLikeName(target->InterfaceName());
  }

  v8::HandleScope scope(probe_data.script_state->GetIsolate());
  info->SetSourceLocation(CaptureScriptSourceLocation(
      probe_data.script_state->GetIsolate(),
      probe_data.listener->GetListenerObject(*target)));
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

}  // namespace blink

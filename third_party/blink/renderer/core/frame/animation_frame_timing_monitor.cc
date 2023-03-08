// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/animation_frame_timing_monitor.h"
#include "base/time/time.h"
#include "third_party/blink/public/mojom/script/script_type.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/script/script.h"
#include "third_party/blink/renderer/core/timing/animation_frame_timing_info.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
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
  current_frame_timing_info_->SetScripts(current_scripts_);
  if (current_frame_timing_info_->Duration() >= kLongAnimationFrameDuration) {
    client_.ReportLongAnimationFrameTiming(current_frame_timing_info_);
  }
  current_frame_timing_info_.Clear();
  current_scripts_.clear();
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
  HeapVector<Member<ScriptTimingInfo>> scripts;

  if (state_ != State::kProcessingTask) {
    return;
  }

  bool should_report = client_.ShouldReportLongAnimationFrameTiming();

  if (client_.RequestedMainFramePending() && should_report) {
    current_frame_timing_info_ =
        MakeGarbageCollected<AnimationFrameTimingInfo>(start_time);
    state_ = State::kPendingFrame;
    return;
  }

  std::swap(scripts, current_scripts_);
  current_scripts_.clear();

  state_ = State::kIdle;

  if (!should_report) {
    return;
  }

  if (!frame || (end_time - start_time) < kLongAnimationFrameDuration) {
    return;
  }

  AnimationFrameTimingInfo* timing_info =
      MakeGarbageCollected<AnimationFrameTimingInfo>(start_time);
  timing_info->SetRenderEndTime(end_time);
  timing_info->SetScripts(scripts);
  DOMWindowPerformance::performance(*frame->DomWindow())
      ->ReportLongAnimationFrameTiming(timing_info);
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

  current_scripts_.push_back(script_timing_info);
  pending_script_info_ = absl::nullopt;
  return script_timing_info;
}

bool AnimationFrameTimingMonitor::ShouldAddScript(ExecutionContext* context) {
  return pending_script_info_ && context && context->IsWindow() &&
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
      WTF::BindOnce(&AnimationFrameTimingMonitor::OnMicrotasksCompleted,
                    WrapPersistent(this), WrapPersistent(context)));

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

void AnimationFrameTimingMonitor::Will(const probe::UserCallback& probe_data) {
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
  pending_script_info_ = PendingScriptInfo{
      .type = probe_data.event_target ? ScriptTimingInfo::Type::kEventHandler
                                      : ScriptTimingInfo::Type::kUserCallback,
      .start_time = probe_data.CaptureStartTime()};
}

namespace {
AtomicString GetClassLikeNameForEventTarget(EventTarget* event_target) {
  DCHECK(event_target);
  if (Node* node = event_target->ToNode()) {
    // TODO: maybe use constructor name (e.g. HTMLImgElement) instead of node
    // name (e.g. IMG)?
    return AtomicString(node->nodeName());
  } else {
    return event_target->InterfaceName();
  }
}
}  // namespace

void AnimationFrameTimingMonitor::Did(const probe::UserCallback& probe_data) {
  user_callback_depth_--;
  if (user_callback_depth_) {
    return;
  }
  ScriptTimingInfo* info = DidExecuteScript(probe_data);
  if (!info) {
    return;
  }

  info->SetClassLikeName(
      probe_data.event_target
          ? GetClassLikeNameForEventTarget(probe_data.event_target)
          : probe_data.class_like_name);
  info->SetPropertyLikeName(probe_data.name ? probe_data.name
                                            : probe_data.atomic_name);
}

void AnimationFrameTimingMonitor::Will(const probe::CallFunction& probe_data) {
  base::TimeTicks start_time = probe_data.CaptureStartTime();
  if (pending_script_info_ && probe_data.depth == 0 &&
      pending_script_info_->execution_start_time.is_null()) {
    pending_script_info_->execution_start_time = start_time;
  }
}

void AnimationFrameTimingMonitor::Did(const probe::CallFunction& probe_data) {
  // We use this probe callback only to capture source location.
  if (probe_data.depth || !pending_script_info_) {
    return;
  }

  probe_data.CaptureEndTime();
  if (probe_data.Duration() < kLongScriptDuration) {
    return;
  }

  if (pending_script_info_->source_location.url) {
    return;
  }

  v8::HandleScope handle_scope(probe_data.context->GetIsolate());
  std::unique_ptr<SourceLocation> source_location =
      CaptureSourceLocation(probe_data.function);
  pending_script_info_->source_location =
      ScriptTimingInfo::ScriptSourceLocation::FromSourceLocation(
          *source_location);
}

}  // namespace blink

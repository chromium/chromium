// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_task.h"

#include <utility>

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/public/common/scheduler/task_attribution_id.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_scheduler_post_task_callback.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/modules/scheduler/script_wrappable_task_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace blink {

namespace {

void GenericTaskData(perfetto::TracedDictionary& dict,
                     ExecutionContext* context,
                     const uint64_t task_id) {
  dict.Add("taskId", task_id);
  if (auto* window = DynamicTo<LocalDOMWindow>(context)) {
    if (auto* frame = window->GetFrame()) {
      dict.Add("frame", IdentifiersFactory::FrameId(frame));
    }
  }
}

void SchedulePostTaskCallbackTraceEventData(perfetto::TracedValue trace_context,
                                            ExecutionContext* execution_context,
                                            const uint64_t task_id,
                                            const String& priority,
                                            const double delay) {
  auto dict = std::move(trace_context).WriteDictionary();
  GenericTaskData(dict, execution_context, task_id);
  dict.Add("priority", priority);
  dict.Add("delay", delay);
  SetCallStack(dict);
}

void RunPostTaskCallbackTraceEventData(perfetto::TracedValue trace_context,
                                       ExecutionContext* execution_context,
                                       const uint64_t task_id,
                                       const String& priority,
                                       const double delay) {
  auto dict = std::move(trace_context).WriteDictionary();
  GenericTaskData(dict, execution_context, task_id);
  dict.Add("priority", priority);
  dict.Add("delay", delay);
}

void AbortPostTaskCallbackTraceEventData(perfetto::TracedValue trace_context,
                                         ExecutionContext* execution_context,
                                         uint64_t task_id) {
  auto dict = std::move(trace_context).WriteDictionary();
  GenericTaskData(dict, execution_context, task_id);
  SetCallStack(dict);
}

}  // namespace

DOMTask::DOMTask(ScriptPromiseResolver* resolver,
                 V8SchedulerPostTaskCallback* callback,
                 DOMTaskSignal* signal,
                 DOMScheduler::DOMTaskQueue* task_queue,
                 base::TimeDelta delay)
    : callback_(callback),
      resolver_(resolver),
      signal_(signal),
      task_queue_(task_queue),
      delay_(delay),
      task_id_for_tracing_(NextIdForTracing()) {
  CHECK(task_queue_);
  CHECK(callback_);
  CHECK(signal_);

  if (signal_->CanAbort()) {
    abort_handle_ = signal_->AddAlgorithm(
        WTF::BindOnce(&DOMTask::OnAbort, WrapWeakPersistent(this)));
  }

  task_handle_ = PostDelayedCancellableTask(
      task_queue_->GetTaskRunner(), FROM_HERE,
      WTF::BindOnce(&DOMTask::Invoke, WrapPersistent(this)), delay);

  ScriptState* script_state =
      callback_->CallbackRelevantScriptStateOrReportError("DOMTask", "Create");
  DCHECK(script_state && script_state->ContextIsValid());

  if (script_state->World().IsMainWorld()) {
    if (auto* tracker =
            ThreadScheduler::Current()->GetTaskAttributionTracker()) {
      parent_task_id_ = tracker->RunningTaskAttributionId(script_state);
    }
  }

  auto* context = ExecutionContext::From(script_state);
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "SchedulePostTaskCallback", SchedulePostTaskCallbackTraceEventData,
      context, task_id_for_tracing_,
      WebSchedulingPriorityToString(task_queue_->GetPriority()),
      delay_.InMillisecondsF());
  async_task_context_.Schedule(context, "postTask");
}

void DOMTask::Trace(Visitor* visitor) const {
  visitor->Trace(callback_);
  visitor->Trace(resolver_);
  visitor->Trace(signal_);
  visitor->Trace(abort_handle_);
  visitor->Trace(task_queue_);
}

void DOMTask::Invoke() {
  DCHECK(callback_);

  // Tasks are not runnable if the document associated with this task's
  // scheduler's global is not fully active, which happens if the
  // ExecutionContext is detached. Note that this context can be different
  // from the the callback's relevant context.
  ExecutionContext* scheduler_context = resolver_->GetExecutionContext();
  if (!scheduler_context || scheduler_context->IsContextDestroyed())
    return;

  ScriptState* script_state =
      callback_->CallbackRelevantScriptStateOrReportError("DOMTask", "Invoke");
  if (!script_state || !script_state->ContextIsValid()) {
    DCHECK(resolver_->GetExecutionContext() &&
           !resolver_->GetExecutionContext()->IsContextDestroyed());
    // The scheduler's context is still attached, but the task's callback's
    // relvant context is not. This happens, for example, if an attached main
    // frame's scheduler schedules a task that runs a callback defined in a
    // detached child frame. The callback's relvant context must be valid to run
    // the callback (enforced in the bindings layer). Since we can't run this
    // task, and therefore won't settle the associated promise, we need to clean
    // up the ScriptPromiseResolver since it is associated with a different
    // context.
    resolver_->Detach();
    return;
  }

  InvokeInternal(script_state);
  callback_.Release();
}

void DOMTask::InvokeInternal(ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  ScriptState::Scope scope(script_state);
  v8::TryCatch try_catch(isolate);

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context);
  DEVTOOLS_TIMELINE_TRACE_EVENT(
      "RunPostTaskCallback", RunPostTaskCallbackTraceEventData, context,
      task_id_for_tracing_,
      WebSchedulingPriorityToString(task_queue_->GetPriority()),
      delay_.InMillisecondsF());
  probe::AsyncTask async_task(context, &async_task_context_);

  std::unique_ptr<scheduler::TaskAttributionTracker::TaskScope>
      task_attribution_scope;
  // For the main thread (tracker exists), create the task scope with the signal
  // to set up propagation. On workers, set the current context here since there
  // is no tracker.
  if (auto* tracker = ThreadScheduler::Current()->GetTaskAttributionTracker()) {
    task_attribution_scope = tracker->CreateTaskScope(
        script_state, parent_task_id_,
        scheduler::TaskAttributionTracker::TaskScopeType::kSchedulerPostTask,
        signal_);
  } else if (RuntimeEnabledFeatures::SchedulerYieldEnabled(
                 ExecutionContext::From(script_state))) {
    ScriptWrappableTaskState::SetCurrent(
        script_state, MakeGarbageCollected<ScriptWrappableTaskState>(
                          scheduler::TaskAttributionId(), signal_));
  }

  ScriptValue result;
  if (callback_->Invoke(nullptr).To(&result)) {
    resolver_->Resolve(result.V8Value());
  } else if (try_catch.HasCaught()) {
    resolver_->Reject(try_catch.Exception());
  }
}

void DOMTask::OnAbort() {
  // If the task has already finished running, the promise is either resolved or
  // rejected, in which case abort will no longer have any effect.
  if (!callback_)
    return;

  task_handle_.Cancel();
  async_task_context_.Cancel();

  DCHECK(resolver_);

  ScriptState* const resolver_script_state = resolver_->GetScriptState();

  if (!IsInParallelAlgorithmRunnable(resolver_->GetExecutionContext(),
                                     resolver_script_state)) {
    return;
  }

  // Switch to the resolver's context to let DOMException pick up the resolver's
  // JS stack.
  ScriptState::Scope script_state_scope(resolver_script_state);

  auto* context = ExecutionContext::From(resolver_script_state);
  DCHECK(context);
  DEVTOOLS_TIMELINE_TRACE_EVENT("AbortPostTaskCallback",
                                AbortPostTaskCallbackTraceEventData, context,
                                task_id_for_tracing_);

  // TODO(crbug.com/1293949): Add an error message.
  resolver_->Reject(
      ToV8Traits<IDLAny>::ToV8(resolver_script_state,
                               signal_->reason(resolver_script_state))
          .ToLocalChecked());
}

}  // namespace blink

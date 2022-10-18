// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_task.h"

#include <utility>

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_scheduler_post_task_callback.h"
#include "third_party/blink/renderer/core/dom/abort_signal.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_priority.h"
#include "third_party/blink/renderer/platform/scheduler/public/web_scheduling_task_queue.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/perfetto/include/perfetto/tracing/traced_value_forward.h"

namespace blink {

#define QUEUEING_TIME_PER_PRIORITY_METRIC_NAME \
  "DOMScheduler.QueueingDurationPerPriority"

#define PRIORITY_CHANGED_HISTOGRAM_NAME \
  "DOMSchedler.TaskSignalPriorityWasChanged"

// Same as UMA_HISTOGRAM_TIMES but for a broader view of this metric we end
// at 1 minute instead of 10 seconds.
#define QUEUEING_TIME_HISTOGRAM(name, sample)                                 \
  UMA_HISTOGRAM_CUSTOM_TIMES(QUEUEING_TIME_PER_PRIORITY_METRIC_NAME name,     \
                             sample, base::Milliseconds(1), base::Minutes(1), \
                             50)

namespace {
void PostTaskCallbackTraceEventData(perfetto::TracedValue context,
                                    const String& priority,
                                    const double delay) {
  auto dict = std::move(context).WriteDictionary();
  dict.Add("priority", priority);
  dict.Add("delay", delay);
  SetCallStack(dict);
}
}  // namespace

DOMTask::DOMTask(ScriptPromiseResolver* resolver,
                 V8SchedulerPostTaskCallback* callback,
                 AbortSignal* signal,
                 DOMScheduler::DOMTaskQueue* task_queue,
                 base::TimeDelta delay)
    : callback_(callback),
      resolver_(resolver),
      signal_(signal),
      task_queue_(task_queue),
      // TODO(crbug.com/1291798): Expose queuing time from
      // base::sequence_manager so we don't have to recalculate it here.
      queue_time_(delay.is_zero() ? base::TimeTicks::Now() : base::TimeTicks()),
      delay_(delay) {
  DCHECK(task_queue_);
  DCHECK(callback_);

  if (signal_) {
    abort_handle_ = signal_->AddAlgorithm(
        WTF::BindOnce(&DOMTask::OnAbort, WrapWeakPersistent(this)));
  }

  task_handle_ = PostDelayedCancellableTask(
      task_queue_->GetTaskRunner(), FROM_HERE,
      WTF::BindOnce(&DOMTask::Invoke, WrapPersistent(this)), delay);

  ScriptState* script_state =
      callback_->CallbackRelevantScriptStateOrReportError("DOMTask", "Create");
  DCHECK(script_state && script_state->ContextIsValid());
  async_task_context_.Schedule(ExecutionContext::From(script_state),
                               "postTask");
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

  RecordTaskStartMetrics();
  InvokeInternal(script_state);
  callback_.Release();
}

void DOMTask::InvokeInternal(ScriptState* script_state) {
  v8::Isolate* isolate = script_state->GetIsolate();
  ScriptState::Scope scope(script_state);
  v8::TryCatch try_catch(isolate);

  DEVTOOLS_TIMELINE_TRACE_EVENT(
      "RunPostTaskCallback", PostTaskCallbackTraceEventData,
      WebSchedulingPriorityToString(task_queue_->GetPriority()),
      delay_.InMillisecondsF());

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context);
  probe::AsyncTask async_task(context, &async_task_context_);
  probe::UserCallback probe(context, "postTask", AtomicString(), true);

  ScriptValue result;
  if (callback_->Invoke(nullptr).To(&result))
    resolver_->Resolve(result.V8Value());
  else if (try_catch.HasCaught())
    resolver_->Reject(try_catch.Exception());
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

  // TODO(crbug.com/1293949): Add an error message.
  resolver_->Reject(
      ToV8Traits<IDLAny>::ToV8(resolver_script_state,
                               signal_->reason(resolver_script_state))
          .ToLocalChecked());
}

void DOMTask::RecordTaskStartMetrics() {
  auto status =
      (signal_ && IsA<DOMTaskSignal>(signal_.Get()))
          ? To<DOMTaskSignal>(signal_.Get())->GetPriorityChangeStatus()
          : DOMTaskSignal::PriorityChangeStatus::kNoPriorityChange;
  UMA_HISTOGRAM_ENUMERATION(PRIORITY_CHANGED_HISTOGRAM_NAME, status);

  if (queue_time_ > base::TimeTicks()) {
    base::TimeDelta queue_duration = base::TimeTicks::Now() - queue_time_;
    DCHECK_GT(queue_duration, base::TimeDelta());
    if (status == DOMTaskSignal::PriorityChangeStatus::kNoPriorityChange) {
      switch (task_queue_->GetPriority()) {
        case WebSchedulingPriority::kUserBlockingPriority:
          QUEUEING_TIME_HISTOGRAM(".UserBlocking", queue_duration);
          break;
        case WebSchedulingPriority::kUserVisiblePriority:
          QUEUEING_TIME_HISTOGRAM(".UserVisable", queue_duration);
          break;
        case WebSchedulingPriority::kBackgroundPriority:
          QUEUEING_TIME_HISTOGRAM(".Background", queue_duration);
          break;
      }
    }
  }
}

}  // namespace blink

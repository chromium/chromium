// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/scheduler/dom_task.h"

#include <utility>

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_scheduler_post_task_callback.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/modules/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
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
                 DOMTaskSignal* signal,
                 base::SingleThreadTaskRunner* task_runner,
                 base::TimeDelta delay)
    : callback_(callback),
      resolver_(resolver),
      signal_(signal),
      // TODO(kdillon): Expose queuing time from base::sequence_manager so we
      // don't have to recalculate it here.
      queue_time_(delay.is_zero() ? base::TimeTicks::Now() : base::TimeTicks()),
      delay_(delay) {
  DCHECK(signal_);
  DCHECK(task_runner);
  DCHECK(callback_);
  signal_->AddAlgorithm(WTF::Bind(&DOMTask::OnAbort, WrapWeakPersistent(this)));

  task_handle_ = PostDelayedCancellableTask(
      *task_runner, FROM_HERE,
      WTF::Bind(&DOMTask::Invoke, WrapPersistent(this)), delay);

  ScriptState* script_state =
      callback_->CallbackRelevantScriptStateOrReportError("DOMTask", "Create");
  DCHECK(script_state && script_state->ContextIsValid());
  probe::AsyncTaskScheduled(ExecutionContext::From(script_state), "postTask",
                            &async_task_id_);
}

void DOMTask::Trace(Visitor* visitor) const {
  visitor->Trace(callback_);
  visitor->Trace(resolver_);
  visitor->Trace(signal_);
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

  DEVTOOLS_TIMELINE_TRACE_EVENT("RunPostTaskCallback",
                                PostTaskCallbackTraceEventData,
                                signal_->priority(), delay_.InMillisecondsF());

  ExecutionContext* context = ExecutionContext::From(script_state);
  DCHECK(context);
  probe::AsyncTask async_task(context, &async_task_id_);
  probe::UserCallback probe(context, "postTask", AtomicString(), true);

  v8::Local<v8::Context> v8_context = script_state->GetContext();
  v8_context->SetContinuationPreservedEmbedderData(
      ToV8(signal_.Get(), v8_context->Global(), isolate));
  ScriptValue result;
  if (callback_->Invoke(nullptr).To(&result))
    resolver_->Resolve(result.V8Value());
  else if (try_catch.HasCaught())
    resolver_->Reject(try_catch.Exception());
  v8_context->SetContinuationPreservedEmbedderData(v8::Local<v8::Object>());
}

void DOMTask::OnAbort() {
  // If the task has already finished running, the promise is either resolved or
  // rejected, in which case abort will no longer have any effect.
  if (!callback_)
    return;

  task_handle_.Cancel();
  resolver_->Reject(
      MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError));

  ScriptState* script_state =
      callback_->CallbackRelevantScriptStateOrReportError("DOMTask", "Abort");
  DCHECK(script_state && script_state->ContextIsValid());
  probe::AsyncTaskCanceled(ExecutionContext::From(script_state),
                           &async_task_id_);
}

void DOMTask::RecordTaskStartMetrics() {
  UMA_HISTOGRAM_ENUMERATION(PRIORITY_CHANGED_HISTOGRAM_NAME,
                            signal_->GetPriorityChangeStatus());

  if (queue_time_ > base::TimeTicks()) {
    base::TimeDelta queue_duration = base::TimeTicks::Now() - queue_time_;
    DCHECK_GT(queue_duration, base::TimeDelta());
    if (signal_->GetPriorityChangeStatus() ==
        DOMTaskSignal::PriorityChangeStatus::kNoPriorityChange) {
      WebSchedulingPriority priority =
          WebSchedulingPriorityFromString(signal_->priority());
      switch (priority) {
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

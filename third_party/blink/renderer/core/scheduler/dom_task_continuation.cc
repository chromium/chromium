// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/dom_task_continuation.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {

DOMTaskContinuation::DOMTaskContinuation(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    AbortSignal* signal,
    DOMScheduler::DOMTaskQueue* task_queue,
    uint64_t task_id_for_tracing)
    : resolver_(resolver),
      signal_(signal),
      task_queue_(task_queue),
      task_id_for_tracing_(task_id_for_tracing) {
  CHECK(task_queue_);

  if (signal_ && signal_->CanAbort()) {
    CHECK(!signal_->aborted());
    abort_handle_ = signal_->AddAlgorithm(
        WTF::BindOnce(&DOMTaskContinuation::OnAbort, WrapWeakPersistent(this)));
  }

  task_handle_ = PostCancellableTask(
      task_queue_->GetTaskRunner(), FROM_HERE,
      WTF::BindOnce(&DOMTaskContinuation::Invoke, WrapPersistent(this)));

  auto* context = ExecutionContext::From(resolver->GetScriptState());
  CHECK(context);
  DEVTOOLS_TIMELINE_TRACE_EVENT_INSTANT(
      "ScheduleYieldContinuation", inspector_scheduler_schedule_event::Data,
      context, task_id_for_tracing_, task_queue_->GetPriority());
  async_task_context_.Schedule(context, "yield");
}

void DOMTaskContinuation::Trace(Visitor* visitor) const {
  visitor->Trace(resolver_);
  visitor->Trace(signal_);
  visitor->Trace(abort_handle_);
  visitor->Trace(task_queue_);
}

void DOMTaskContinuation::Invoke() {
  CHECK(resolver_);
  if (ExecutionContext* context = resolver_->GetExecutionContext()) {
    DEVTOOLS_TIMELINE_TRACE_EVENT(
        "RunYieldContinuation", inspector_scheduler_run_event::Data, context,
        task_id_for_tracing_, task_queue_->GetPriority());
    probe::AsyncTask async_task(context, &async_task_context_);
    resolver_->Resolve();
  }
  if (abort_handle_) {
    signal_->RemoveAlgorithm(abort_handle_);
    abort_handle_ = nullptr;
  }
}

void DOMTaskContinuation::OnAbort() {
  task_handle_.Cancel();
  async_task_context_.Cancel();

  CHECK(resolver_);
  ScriptState* const resolver_script_state = resolver_->GetScriptState();
  if (!IsInParallelAlgorithmRunnable(resolver_->GetExecutionContext(),
                                     resolver_script_state)) {
    return;
  }

  // Switch to the resolver's context to let DOMException pick up the resolver's
  // JS stack.
  ScriptState::Scope script_state_scope(resolver_script_state);

  auto* context = ExecutionContext::From(resolver_script_state);
  CHECK(context);
  DEVTOOLS_TIMELINE_TRACE_EVENT("AbortYieldContinuation",
                                inspector_scheduler_abort_event::Data, context,
                                task_id_for_tracing_);

  // TODO(crbug.com/1293949): Add an error message.
  CHECK(signal_);
  resolver_->Reject(
      signal_->reason(resolver_script_state).V8ValueFor(resolver_script_state));
}

}  // namespace blink

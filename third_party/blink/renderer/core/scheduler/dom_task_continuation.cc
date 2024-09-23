// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/scheduler/dom_task_continuation.h"

#include <utility>

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/scheduler/dom_task_signal.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"

namespace blink {

DOMTaskContinuation::DOMTaskContinuation(
    ScriptPromiseResolver<IDLUndefined>* resolver,
    AbortSignal* signal,
    DOMScheduler::DOMTaskQueue* task_queue)
    : resolver_(resolver), signal_(signal), task_queue_(task_queue) {
  CHECK(task_queue_);

  if (signal_ && signal_->CanAbort()) {
    CHECK(!signal_->aborted());
    abort_handle_ = signal_->AddAlgorithm(
        WTF::BindOnce(&DOMTaskContinuation::OnAbort, WrapWeakPersistent(this)));
  }

  task_handle_ = PostCancellableTask(
      task_queue_->GetTaskRunner(), FROM_HERE,
      WTF::BindOnce(&DOMTaskContinuation::Invoke, WrapPersistent(this)));
  async_task_context_.Schedule(
      ExecutionContext::From(resolver->GetScriptState()), "yield");
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
  // TODO(crbug.com/1293949): Add an error message.
  CHECK(signal_);
  resolver_->Reject(
      signal_->reason(resolver_script_state).V8ValueFor(resolver_script_state));
}

}  // namespace blink

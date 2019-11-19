// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

#if DCHECK_IS_ON()
#include "base/debug/alias.h"
#include "components/crash/core/common/crash_key.h"
#endif

namespace blink {

ScriptPromiseResolver::ScriptPromiseResolver(ScriptState* script_state)
    : ContextLifecycleObserver(ExecutionContext::From(script_state)),
      state_(kPending),
      script_state_(script_state),
      resolver_(script_state),
      keep_alive_(PERSISTENT_FROM_HERE) {
  if (GetExecutionContext()->IsContextDestroyed()) {
    state_ = kDetached;
    resolver_.Clear();
  }
}

ScriptPromiseResolver::~ScriptPromiseResolver() = default;

void ScriptPromiseResolver::Dispose() {
#if DCHECK_IS_ON()
  // This assertion fails if:
  //  - promise() is called at least once and
  //  - this resolver is destructed before it is resolved, rejected,
  //    detached, the V8 isolate is terminated or the associated
  //    ExecutionContext is stopped.
  const bool is_properly_detached =
      state_ == kDetached || !is_promise_called_ ||
      !GetScriptState()->ContextIsValid() || !GetExecutionContext() ||
      GetExecutionContext()->IsContextDestroyed();
  if (!is_properly_detached) {
    // This is here to make it easier to track down which promise resolvers are
    // being abandoned. See https://crbug.com/873980.
    static crash_reporter::CrashKeyString<1024> trace_key(
        "scriptpromiseresolver-trace");
    crash_reporter::SetCrashKeyStringToStackTrace(&trace_key,
                                                  create_stack_trace_);
    DCHECK(false)
        << "ScriptPromiseResolver was not properly detached; created at\n"
        << create_stack_trace_.ToString();
  }
#endif
  deferred_resolve_task_.Cancel();
  resolver_.Clear();
  value_.Clear();
}

void ScriptPromiseResolver::Reject(ExceptionState& exception_state) {
  DCHECK(exception_state.HadException());
  Reject(exception_state.GetException());
  exception_state.ClearException();
}

void ScriptPromiseResolver::Detach() {
  if (state_ == kDetached)
    return;
  deferred_resolve_task_.Cancel();
  state_ = kDetached;
  resolver_.Clear();
  value_.Clear();
  keep_alive_.Clear();
}

void ScriptPromiseResolver::KeepAliveWhilePending() {
  // keepAliveWhilePending() will be called twice if the resolver
  // is created in a suspended execution context and the resolver
  // is then resolved/rejected while in that suspended state.
  if (state_ == kDetached || keep_alive_)
    return;

  // Keep |this| around while the promise is Pending;
  // see detach() for the dual operation.
  keep_alive_ = this;
}

void ScriptPromiseResolver::ResolveOrRejectImmediately() {
  DCHECK(!GetExecutionContext()->IsContextDestroyed());
  DCHECK(!GetExecutionContext()->IsContextPaused());
  {
    if (state_ == kResolving) {
      resolver_.Resolve(value_.NewLocal(script_state_->GetIsolate()));
    } else {
      DCHECK_EQ(state_, kRejecting);
      resolver_.Reject(value_.NewLocal(script_state_->GetIsolate()));
    }
  }
  Detach();
}

void ScriptPromiseResolver::ScheduleResolveOrReject() {
  deferred_resolve_task_ = PostCancellableTask(
      *GetExecutionContext()->GetTaskRunner(TaskType::kMicrotask), FROM_HERE,
      WTF::Bind(&ScriptPromiseResolver::ResolveOrRejectDeferred,
                WrapPersistent(this)));
}

void ScriptPromiseResolver::ResolveOrRejectDeferred() {
  DCHECK(state_ == kResolving || state_ == kRejecting);
  if (!GetScriptState()->ContextIsValid()) {
    Detach();
    return;
  }

  ScriptState::Scope scope(script_state_);
  ResolveOrRejectImmediately();
}

void ScriptPromiseResolver::Trace(blink::Visitor* visitor) {
  visitor->Trace(script_state_);
  visitor->Trace(resolver_);
  visitor->Trace(value_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink

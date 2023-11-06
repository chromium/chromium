// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

#if DCHECK_IS_ON()
#include "base/debug/alias.h"
#include "components/crash/core/common/crash_key.h"
#endif

namespace blink {

class ScriptPromiseResolver::ExceptionStateScope final : public ExceptionState {
  STACK_ALLOCATED();

 public:
  explicit ExceptionStateScope(ScriptPromiseResolver* resolver)
      : ExceptionState(resolver->script_state_->GetIsolate(),
                       resolver->exception_context_),
        resolver_(resolver) {}
  ~ExceptionStateScope() {
    DCHECK(HadException());
    resolver_->Reject(GetException());
    ClearException();
  }

 private:
  ScriptPromiseResolver* resolver_;
};

ScriptPromiseResolver::ScriptPromiseResolver(ScriptState* script_state)
    : ScriptPromiseResolver(
          script_state,
          ExceptionContext(ExceptionContextType::kUnknown, nullptr, nullptr)) {}

ScriptPromiseResolver::ScriptPromiseResolver(
    ScriptState* script_state,
    const ExceptionContext& exception_context)
    : ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      state_(kPending),
      script_state_(script_state),
      resolver_(script_state),
      exception_context_(exception_context) {
  if (GetExecutionContext()->IsContextDestroyed()) {
    state_ = kDetached;
    resolver_.Clear();
  }
  script_url_ = GetCurrentScriptUrl(script_state->GetIsolate());
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
  if (!is_properly_detached && !suppress_detach_check_) {
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
}

void ScriptPromiseResolver::Reject(ExceptionState& exception_state) {
  DCHECK(exception_state.HadException());
  Reject(exception_state.GetException());
  exception_state.ClearException();
}

void ScriptPromiseResolver::RejectWithDOMException(
    DOMExceptionCode exception_code,
    const String& message) {
  ExceptionStateScope(this).ThrowDOMException(exception_code, message);
}

void ScriptPromiseResolver::RejectWithSecurityError(
    const String& sanitized_message,
    const String& unsanitized_message) {
  ExceptionStateScope(this).ThrowSecurityError(sanitized_message,
                                               unsanitized_message);
}

void ScriptPromiseResolver::RejectWithTypeError(const String& message) {
  ExceptionStateScope(this).ThrowTypeError(message);
}

void ScriptPromiseResolver::RejectWithRangeError(const String& message) {
  ExceptionStateScope(this).ThrowRangeError(message);
}

void ScriptPromiseResolver::RejectWithWasmCompileError(const String& message) {
  ExceptionStateScope(this).ThrowWasmCompileError(message);
}

void ScriptPromiseResolver::Detach() {
  if (state_ == kDetached)
    return;
  deferred_resolve_task_.Cancel();
  state_ = kDetached;
  resolver_.Clear();
  value_.Reset();
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

  probe::WillHandlePromise(GetExecutionContext(), script_state_,
                           state_ == kResolving,
                           exception_context_.GetClassName(),
                           exception_context_.GetPropertyName(), script_url_);
  if (state_ == kResolving) {
    resolver_.Resolve(value_.Get(script_state_->GetIsolate()));
  } else {
    DCHECK_EQ(state_, kRejecting);
    resolver_.Reject(value_.Get(script_state_->GetIsolate()));
  }
  Detach();
}

void ScriptPromiseResolver::ScheduleResolveOrReject() {
  deferred_resolve_task_ = PostCancellableTask(
      *GetExecutionContext()->GetTaskRunner(TaskType::kMicrotask), FROM_HERE,
      WTF::BindOnce(&ScriptPromiseResolver::ResolveOrRejectDeferred,
                    WrapPersistent(this)));
}

void ScriptPromiseResolver::ResolveOrRejectDeferred() {
  DCHECK(state_ == kResolving || state_ == kRejecting);
  if (!GetScriptState()->ContextIsValid()) {
    Detach();
    return;
  }

  ScriptState::Scope scope(script_state_.Get());
  ResolveOrRejectImmediately();
}

void ScriptPromiseResolver::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(resolver_);
  visitor->Trace(value_);
  ExecutionContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"

#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/capture_source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_throw_dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/bindings/source_location.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

#if DCHECK_IS_ON()
#include "base/debug/alias.h"
#include "components/crash/core/common/crash_key.h"
#endif

namespace blink {

ScriptPromiseResolverBase::ScriptPromiseResolverBase(
    ScriptState* script_state,
    const ExceptionContext& exception_context)
    : resolver_(script_state->GetIsolate(),
                v8::Promise::Resolver::New(script_state->GetContext())
                    .ToLocalChecked()),
      state_(kPending),
      script_state_(script_state),
      exception_context_(exception_context),
      script_url_(GetCurrentScriptUrl(script_state->GetIsolate())) {}

ScriptPromiseResolverBase::~ScriptPromiseResolverBase() = default;

#if DCHECK_IS_ON()
void ScriptPromiseResolverBase::Dispose() {
  // This assertion fails if:
  //  - promise() is called at least once and
  //  - this resolver is destructed before it is resolved, rejected,
  //    detached, the V8 isolate is terminated or the associated
  //    ExecutionContext is stopped.
  const bool is_properly_detached = state_ == kDone || !is_promise_called_ ||
                                    !GetScriptState()->ContextIsValid() ||
                                    !GetExecutionContext() ||
                                    GetExecutionContext()->IsContextDestroyed();
  if (!is_properly_detached && !suppress_detach_check_) {
    // This is here to make it easier to track down which promise resolvers are
    // being abandoned. See https://crbug.com/873980.
    static crash_reporter::CrashKeyString<1024> trace_key(
        "scriptpromiseresolver-trace");
    crash_reporter::SetCrashKeyStringToStackTrace(&trace_key,
                                                  create_stack_trace_);
    DCHECK(false)
        << "ScriptPromiseResolverBase was not properly detached; created at\n"
        << create_stack_trace_.ToString();
  }
}
#endif

void ScriptPromiseResolverBase::Reject(DOMException* value) {
  Reject<DOMException>(value);
}

void ScriptPromiseResolverBase::Reject(v8::Local<v8::Value> value) {
  Reject<IDLAny>(value);
}

void ScriptPromiseResolverBase::Reject(const ScriptValue& value) {
  Reject<IDLAny>(value);
}

void ScriptPromiseResolverBase::Reject(const char* value) {
  Reject<IDLString>(value);
}

void ScriptPromiseResolverBase::Reject(bool value) {
  Reject<IDLBoolean>(value);
}

void ScriptPromiseResolverBase::RejectWithDOMException(
    DOMExceptionCode exception_code,
    const String& message) {
  ScriptState::Scope scope(script_state_.Get());
  v8::Isolate* isolate = script_state_->GetIsolate();
  auto exception =
      V8ThrowDOMException::CreateOrDie(isolate, exception_code, message);
  ApplyContextToException(script_state_, exception, exception_context_);
  Reject(exception);
}

void ScriptPromiseResolverBase::RejectWithSecurityError(
    const String& sanitized_message,
    const String& unsanitized_message) {
  ScriptState::Scope scope(script_state_.Get());
  v8::Isolate* isolate = script_state_->GetIsolate();
  auto exception = V8ThrowDOMException::CreateOrDie(
      isolate, DOMExceptionCode::kSecurityError, sanitized_message,
      unsanitized_message);
  ApplyContextToException(script_state_, exception, exception_context_);
  Reject(exception);
}

void ScriptPromiseResolverBase::RejectWithTypeError(const String& message) {
  ScriptState::Scope scope(script_state_.Get());
  Reject(V8ThrowException::CreateTypeError(
      script_state_->GetIsolate(),
      ExceptionMessages::AddContextToMessage(exception_context_, message)));
}

void ScriptPromiseResolverBase::RejectWithRangeError(const String& message) {
  ScriptState::Scope scope(script_state_.Get());
  Reject(V8ThrowException::CreateRangeError(
      script_state_->GetIsolate(),
      ExceptionMessages::AddContextToMessage(exception_context_, message)));
}

void ScriptPromiseResolverBase::RejectWithWasmCompileError(
    const String& message) {
  ScriptState::Scope scope(script_state_.Get());
  Reject(V8ThrowException::CreateWasmCompileError(
      script_state_->GetIsolate(),
      ExceptionMessages::AddContextToMessage(exception_context_, message)));
}

void ScriptPromiseResolverBase::Detach() {
  // Reset state even if we're already kDone. The resolver_ will not have been
  // reset yet if this was marked kDone due to resolve/reject, and an explicit
  // Detach() should really clear everything.
  state_ = kDone;
  resolver_.Reset();
  value_.Reset();
}

void ScriptPromiseResolverBase::NotifyResolveOrReject() {
  if (GetExecutionContext()->IsContextPaused()) {
    ScheduleResolveOrReject();
    return;
  }
  // TODO(esprehn): This is a hack, instead we should CHECK that
  // script is allowed, and v8 should be running the entry hooks below and
  // crashing if script is forbidden. We should then audit all users of
  // ScriptPromiseResolverBase and the related specs and switch to an async
  // resolve.
  // See: http://crbug.com/663476
  if (ScriptForbiddenScope::IsScriptForbidden()) {
    ScheduleResolveOrReject();
    return;
  }
  ResolveOrRejectImmediately();
}

void ScriptPromiseResolverBase::ResolveOrRejectImmediately() {
  DCHECK(!GetExecutionContext()->IsContextDestroyed());
  DCHECK(!GetExecutionContext()->IsContextPaused());

  probe::WillHandlePromise(
      GetExecutionContext(), script_state_, state_ == kResolving,
      exception_context_.GetClassName(),
      exception_context_.GetPropertyNameVariant(), script_url_);

  v8::MicrotasksScope microtasks_scope(
      script_state_->GetIsolate(), ToMicrotaskQueue(script_state_),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  auto resolver = resolver_.Get(script_state_->GetIsolate());
  if (state_ == kResolving) {
    std::ignore = resolver->Resolve(script_state_->GetContext(),
                                    value_.Get(script_state_->GetIsolate()));
  } else {
    DCHECK_EQ(state_, kRejecting);
    std::ignore = resolver->Reject(script_state_->GetContext(),
                                   value_.Get(script_state_->GetIsolate()));
  }

  // Don't reset `resolver_`, so that Promise() still works.
  state_ = kDone;
  value_.Reset();
}

void ScriptPromiseResolverBase::ScheduleResolveOrReject() {
  GetExecutionContext()
      ->GetTaskRunner(TaskType::kMicrotask)
      ->PostTask(
          FROM_HERE,
          WTF::BindOnce(&ScriptPromiseResolverBase::ResolveOrRejectDeferred,
                        WrapPersistent(this)));
}

void ScriptPromiseResolverBase::ResolveOrRejectDeferred() {
  DCHECK(state_ == kResolving || state_ == kRejecting);
  if (!GetExecutionContext()) {
    return;
  }

  ScriptState::Scope scope(script_state_.Get());
  ResolveOrRejectImmediately();
}

void ScriptPromiseResolverBase::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(resolver_);
  visitor->Trace(value_);
}

ExecutionContext* ScriptPromiseResolverBase::GetExecutionContext() {
  if (!GetScriptState()->ContextIsValid()) {
    return nullptr;
  }
  auto* execution_context = ExecutionContext::From(script_state_);
  return execution_context->IsContextDestroyed() ? nullptr : execution_context;
}

}  // namespace blink

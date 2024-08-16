// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/callback_function_base.h"

#include "third_party/blink/renderer/platform/bindings/binding_security_for_platform.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink {

CallbackFunctionBase::CallbackFunctionBase(
    v8::Local<v8::Object> callback_function) {
  DCHECK(!callback_function.IsEmpty());

  v8::Isolate* isolate = callback_function->GetIsolate();
  callback_function_.Reset(isolate, callback_function);

  incumbent_script_state_ =
      ScriptState::From(isolate, isolate->GetIncumbentContext());

  // Set |callback_relevant_script_state_| iff the creation context and the
  // incumbent context are the same origin-domain. Otherwise, leave it as
  // nullptr.
  if (callback_function->IsFunction()) {
    // If the callback object is a function, it's guaranteed to be the same
    // origin at least, and very likely to be the same origin-domain. Even if
    // it's not the same origin-domain, it's already been possible for the
    // callsite to run arbitrary script in the context. No need to protect it.
    // This is an optimization faster than ShouldAllowAccessToV8Context below.
    callback_relevant_script_state_ =
        ScriptState::ForRelevantRealm(isolate, callback_function);
  } else {
    v8::MaybeLocal<v8::Context> creation_context =
        callback_function->GetCreationContext();
    if (BindingSecurityForPlatform::ShouldAllowAccessToV8Context(
            incumbent_script_state_->GetContext(), creation_context)) {
      callback_relevant_script_state_ =
          ScriptState::From(isolate, creation_context.ToLocalChecked());
    }
  }
}

void CallbackFunctionBase::Trace(Visitor* visitor) const {
  visitor->Trace(callback_function_);
  visitor->Trace(callback_relevant_script_state_);
  visitor->Trace(incumbent_script_state_);
}

ScriptState* CallbackFunctionBase::CallbackRelevantScriptStateOrReportError(
    const char* interface_name,
    const char* operation_name) const {
  if (callback_relevant_script_state_) [[likely]] {
    return callback_relevant_script_state_;
  }

  // Report a SecurityError due to a cross origin callback object.
  ScriptState::Scope incumbent_scope(incumbent_script_state_);
  v8::TryCatch try_catch(GetIsolate());
  try_catch.SetVerbose(true);
  ExceptionState exception_state(GetIsolate(), v8::ExceptionContext::kOperation,
                                 interface_name, operation_name);
  exception_state.ThrowSecurityError(
      "An invocation of the provided callback failed due to cross origin "
      "access.");
  return nullptr;
}

ScriptState* CallbackFunctionBase::CallbackRelevantScriptStateOrThrowException(
    const char* interface_name,
    const char* operation_name) const {
  if (callback_relevant_script_state_) [[likely]] {
    return callback_relevant_script_state_;
  }

  // Throw a SecurityError due to a cross origin callback object.
  ScriptState::Scope incumbent_scope(incumbent_script_state_);
  ExceptionState exception_state(GetIsolate(), v8::ExceptionContext::kOperation,
                                 interface_name, operation_name);
  exception_state.ThrowSecurityError(
      "An invocation of the provided callback failed due to cross origin "
      "access.");
  return nullptr;
}

void CallbackFunctionBase::EvaluateAsPartOfCallback(
    base::OnceCallback<void(ScriptState*)> closure) {
  if (!callback_relevant_script_state_) [[unlikely]] {
    return;
  }

  // https://webidl.spec.whatwg.org/#es-invoking-callback-functions
  // step 8: Prepare to run script with relevant settings.
  ScriptState::Scope callback_relevant_context_scope(
      callback_relevant_script_state_);
  // step 9: Prepare to run a callback with stored settings.
  v8::Context::BackupIncumbentScope backup_incumbent_scope(
      IncumbentScriptState()->GetContext());

  std::move(closure).Run(callback_relevant_script_state_);
}

void CallbackFunctionWithTaskAttributionBase::Trace(Visitor* visitor) const {
  CallbackFunctionBase::Trace(visitor);
  visitor->Trace(parent_task_);
}

}  // namespace blink

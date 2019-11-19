// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/callback_function_base.h"

#include "third_party/blink/renderer/platform/bindings/binding_security_for_platform.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

CallbackFunctionBase::CallbackFunctionBase(
    v8::Local<v8::Object> callback_function) {
  DCHECK(!callback_function.IsEmpty());

  v8::Isolate* isolate = callback_function->GetIsolate();
  callback_function_.Set(isolate, callback_function);

  incumbent_script_state_ = ScriptState::From(isolate->GetIncumbentContext());

  // Set |callback_relevant_script_state_| iff the creation context and the
  // incumbent context are the same origin-domain. Otherwise, leave it as
  // nullptr.
  v8::Local<v8::Context> creation_context =
      callback_function->CreationContext();
  if (BindingSecurityForPlatform::ShouldAllowAccessToV8Context(
          incumbent_script_state_->GetContext(), creation_context,
          BindingSecurityForPlatform::ErrorReportOption::kDoNotReport)) {
    callback_relevant_script_state_ = ScriptState::From(creation_context);
  }
}

void CallbackFunctionBase::Trace(Visitor* visitor) {
  visitor->Trace(callback_function_);
  visitor->Trace(callback_relevant_script_state_);
  visitor->Trace(incumbent_script_state_);
}

ScriptState* CallbackFunctionBase::CallbackRelevantScriptStateOrReportError(
    const char* interface,
    const char* operation) {
  if (callback_relevant_script_state_)
    return callback_relevant_script_state_;

  // Report a SecurityError due to a cross origin callback object.
  ScriptState::Scope incumbent_scope(incumbent_script_state_);
  v8::TryCatch try_catch(GetIsolate());
  try_catch.SetVerbose(true);
  ExceptionState exception_state(
      GetIsolate(), ExceptionState::kExecutionContext, interface, operation);
  exception_state.ThrowSecurityError(
      "An invocation of the provided callback failed due to cross origin "
      "access.");
  return nullptr;
}

ScriptState* CallbackFunctionBase::CallbackRelevantScriptStateOrThrowException(
    const char* interface,
    const char* operation) {
  if (callback_relevant_script_state_)
    return callback_relevant_script_state_;

  // Throw a SecurityError due to a cross origin callback object.
  ScriptState::Scope incumbent_scope(incumbent_script_state_);
  ExceptionState exception_state(
      GetIsolate(), ExceptionState::kExecutionContext, interface, operation);
  exception_state.ThrowSecurityError(
      "An invocation of the provided callback failed due to cross origin "
      "access.");
  return nullptr;
}

void CallbackFunctionBase::EvaluateAsPartOfCallback(
    base::OnceCallback<void()> closure) {
  if (!callback_relevant_script_state_)
    return;

  // https://heycam.github.io/webidl/#es-invoking-callback-functions
  // step 8: Prepare to run script with relevant settings.
  ScriptState::Scope callback_relevant_context_scope(
      callback_relevant_script_state_);
  // step 9: Prepare to run a callback with stored settings.
  v8::Context::BackupIncumbentScope backup_incumbent_scope(
      IncumbentScriptState()->GetContext());

  std::move(closure).Run();
}

}  // namespace blink

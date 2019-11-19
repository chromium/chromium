// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/bindings/callback_interface_base.h"

#include "third_party/blink/renderer/platform/bindings/binding_security_for_platform.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

CallbackInterfaceBase::CallbackInterfaceBase(
    v8::Local<v8::Object> callback_object,
    SingleOperationOrNot single_op_or_not) {
  DCHECK(!callback_object.IsEmpty());

  v8::Isolate* isolate = callback_object->GetIsolate();
  callback_object_.Set(isolate, callback_object);

  incumbent_script_state_ = ScriptState::From(isolate->GetIncumbentContext());
  is_callback_object_callable_ =
      (single_op_or_not == kSingleOperation) && callback_object->IsCallable();

  // Set |callback_relevant_script_state_| iff the creation context and the
  // incumbent context are the same origin-domain. Otherwise, leave it as
  // nullptr.
  v8::Local<v8::Context> creation_context = callback_object->CreationContext();
  if (BindingSecurityForPlatform::ShouldAllowAccessToV8Context(
          incumbent_script_state_->GetContext(), creation_context,
          BindingSecurityForPlatform::ErrorReportOption::kDoNotReport)) {
    callback_relevant_script_state_ = ScriptState::From(creation_context);
  }
}

void CallbackInterfaceBase::Trace(Visitor* visitor) {
  visitor->Trace(callback_object_);
  visitor->Trace(callback_relevant_script_state_);
  visitor->Trace(incumbent_script_state_);
}

ScriptState* CallbackInterfaceBase::CallbackRelevantScriptStateOrReportError(
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

ScriptState* CallbackInterfaceBase::CallbackRelevantScriptStateOrThrowException(
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

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/callback_function.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#include "third_party/blink/renderer/bindings/tests/results/core/v8_treat_non_object_as_null_void_function.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

const char* V8TreatNonObjectAsNullVoidFunction::NameInHeapSnapshot() const {
  return "V8TreatNonObjectAsNullVoidFunction";
}

v8::Maybe<void> V8TreatNonObjectAsNullVoidFunction::Invoke(ScriptWrappable* callback_this_value) {
  if (!IsCallbackFunctionRunnable(CallbackRelevantScriptState(),
                                  IncumbentScriptState())) {
    // Wrapper-tracing for the callback function makes the function object and
    // its creation context alive. Thus it's safe to use the creation context
    // of the callback function here.
    v8::HandleScope handle_scope(GetIsolate());
    v8::Local<v8::Object> callback_object = CallbackFunction();
    CHECK(!callback_object.IsEmpty());
    v8::Context::Scope context_scope(callback_object->CreationContext());
    V8ThrowException::ThrowError(
        GetIsolate(),
        ExceptionMessages::FailedToExecute(
            "invoke",
            "TreatNonObjectAsNullVoidFunction",
            "The provided callback is no longer runnable."));
    return v8::Nothing<void>();
  }

  // step: Prepare to run script with relevant settings.
  ScriptState::Scope callback_relevant_context_scope(
      CallbackRelevantScriptState());
  // step: Prepare to run a callback with stored settings.
  v8::Context::BackupIncumbentScope backup_incumbent_scope(
      IncumbentScriptState()->GetContext());

  v8::Local<v8::Function> function;
  // callback function's invoke:
  // step 4. If ! IsCallable(F) is false:
  if (!CallbackObject()->IsFunction()) {
    // Handle the special case of [TreatNonObjectAsNull].
    //
    // step 4.1. If the callback function's return type is void, return.
    return v8::JustVoid();
  }
  function = CallbackFunction();

  v8::Local<v8::Value> this_arg;
  this_arg = ToV8(callback_this_value, CallbackRelevantScriptState());

  // step: Let esArgs be the result of converting args to an ECMAScript
  //   arguments list. If this throws an exception, set completion to the
  //   completion value representing the thrown exception and jump to the step
  //   labeled return.
  const int argc = 0;
  v8::Local<v8::Value> *argv = nullptr;

  v8::Local<v8::Value> call_result;
  // step: Let callResult be Call(X, thisArg, esArgs).
  if (!V8ScriptRunner::CallFunction(
          function,
          ExecutionContext::From(CallbackRelevantScriptState()),
          this_arg,
          argc,
          argv,
          GetIsolate()).ToLocal(&call_result)) {
    // step: If callResult is an abrupt completion, set completion to callResult
    //   and jump to the step labeled return.
    return v8::Nothing<void>();
  }

  // step: Set completion to the result of converting callResult.[[Value]] to
  //   an IDL value of the same type as the operation's return type.
  return v8::JustVoid();
}

void V8TreatNonObjectAsNullVoidFunction::InvokeAndReportException(ScriptWrappable* callback_this_value) {
  v8::TryCatch try_catch(GetIsolate());
  try_catch.SetVerbose(true);

  v8::Maybe<void> maybe_result =
      Invoke(callback_this_value);
  // An exception if any is killed with the v8::TryCatch above.
  ALLOW_UNUSED_LOCAL(maybe_result);
}

v8::Maybe<void> V8PersistentCallbackFunction<V8TreatNonObjectAsNullVoidFunction>::Invoke(ScriptWrappable* callback_this_value) {
  return Proxy()->Invoke(
      callback_this_value);
}

void V8PersistentCallbackFunction<V8TreatNonObjectAsNullVoidFunction>::InvokeAndReportException(ScriptWrappable* callback_this_value) {
  Proxy()->InvokeAndReportException(
      callback_this_value);
}

}  // namespace blink

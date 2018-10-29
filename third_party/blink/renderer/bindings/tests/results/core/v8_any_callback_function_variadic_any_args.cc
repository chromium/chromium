// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has been auto-generated from the Jinja2 template
// third_party/blink/renderer/bindings/templates/callback_function.cc.tmpl
// by the script code_generator_v8.py.
// DO NOT MODIFY!

// clang-format off

#include "third_party/blink/renderer/bindings/tests/results/core/v8_any_callback_function_variadic_any_args.h"

#include "base/stl_util.h"
#include "third_party/blink/renderer/bindings/core/v8/generated_code_helper.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"

namespace blink {

const char* V8AnyCallbackFunctionVariadicAnyArgs::NameInHeapSnapshot() const {
  return "V8AnyCallbackFunctionVariadicAnyArgs";
}

v8::Maybe<ScriptValue> V8AnyCallbackFunctionVariadicAnyArgs::Invoke(ScriptWrappable* callback_this_value, const Vector<ScriptValue>& arguments) {
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
            "AnyCallbackFunctionVariadicAnyArgs",
            "The provided callback is no longer runnable."));
    return v8::Nothing<ScriptValue>();
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
  //
  // No [TreatNonObjectAsNull] presents.  Must be always callable.
  DCHECK(CallbackObject()->IsFunction());
  function = CallbackFunction();

  v8::Local<v8::Value> this_arg;
  this_arg = ToV8(callback_this_value, CallbackRelevantScriptState());

  // step: Let esArgs be the result of converting args to an ECMAScript
  //   arguments list. If this throws an exception, set completion to the
  //   completion value representing the thrown exception and jump to the step
  //   labeled return.
  v8::Local<v8::Object> argument_creation_context =
      CallbackRelevantScriptState()->GetContext()->Global();
  ALLOW_UNUSED_LOCAL(argument_creation_context);
  const int argc = 0 + arguments.size();
  v8::Local<v8::Value> argv[argc];
  for (wtf_size_t i = 0; i < arguments.size(); ++i) {
    argv[0 + i] = ToV8(arguments[i], argument_creation_context, GetIsolate());
  }

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
    return v8::Nothing<ScriptValue>();
  }

  // step: Set completion to the result of converting callResult.[[Value]] to
  //   an IDL value of the same type as the operation's return type.
  {
    ExceptionState exception_state(GetIsolate(),
                                   ExceptionState::kExecutionContext,
                                   "AnyCallbackFunctionVariadicAnyArgs",
                                   "invoke");
    auto native_result =
        NativeValueTraits<ScriptValue>::NativeValue(
            GetIsolate(), call_result, exception_state);
    if (exception_state.HadException())
      return v8::Nothing<ScriptValue>();
    else
      return v8::Just<ScriptValue>(native_result);
  }
}

v8::Maybe<ScriptValue> V8AnyCallbackFunctionVariadicAnyArgs::Construct(const Vector<ScriptValue>& arguments) {
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
            "construct",
            "AnyCallbackFunctionVariadicAnyArgs",
            "The provided callback is no longer runnable."));
    return v8::Nothing<ScriptValue>();
  }

  // step: Prepare to run script with relevant settings.
  ScriptState::Scope callback_relevant_context_scope(
      CallbackRelevantScriptState());
  // step: Prepare to run a callback with stored settings.
  v8::Context::BackupIncumbentScope backup_incumbent_scope(
      IncumbentScriptState()->GetContext());

  // step 3. If ! IsConstructor(F) is false, throw a TypeError exception.
  //
  // Note that step 7. and 8. are side effect free (except for a very rare
  // exception due to no incumbent realm), so it's okay to put step 3. after
  // step 7. and 8.
  if (!IsConstructor()) {
    V8ThrowException::ThrowTypeError(
        GetIsolate(),
        ExceptionMessages::FailedToExecute(
            "construct",
            "AnyCallbackFunctionVariadicAnyArgs",
            "The provided callback is not a constructor."));
    return v8::Nothing<ScriptValue>();
  }

  v8::Local<v8::Function> function;
  // callback function's invoke:
  // step 4. If ! IsCallable(F) is false:
  //
  // No [TreatNonObjectAsNull] presents.  Must be always callable.
  DCHECK(CallbackObject()->IsFunction());
  function = CallbackFunction();

  // step: Let esArgs be the result of converting args to an ECMAScript
  //   arguments list. If this throws an exception, set completion to the
  //   completion value representing the thrown exception and jump to the step
  //   labeled return.
  v8::Local<v8::Object> argument_creation_context =
      CallbackRelevantScriptState()->GetContext()->Global();
  ALLOW_UNUSED_LOCAL(argument_creation_context);
  const int argc = 0 + arguments.size();
  v8::Local<v8::Value> argv[argc];
  for (wtf_size_t i = 0; i < arguments.size(); ++i) {
    argv[0 + i] = ToV8(arguments[i], argument_creation_context, GetIsolate());
  }

  v8::Local<v8::Value> call_result;
  if (!V8ScriptRunner::CallAsConstructor(
          GetIsolate(),
          function,
          ExecutionContext::From(CallbackRelevantScriptState()),
          argc,
          argv).ToLocal(&call_result)) {
    // step 11. If callResult is an abrupt completion, set completion to
    //   callResult and jump to the step labeled return.
    return v8::Nothing<ScriptValue>();
  }

  // step: Set completion to the result of converting callResult.[[Value]] to
  //   an IDL value of the same type as the operation's return type.
  {
    ExceptionState exception_state(GetIsolate(),
                                   ExceptionState::kExecutionContext,
                                   "AnyCallbackFunctionVariadicAnyArgs",
                                   "construct");
    auto native_result =
        NativeValueTraits<ScriptValue>::NativeValue(
            GetIsolate(), call_result, exception_state);
    if (exception_state.HadException())
      return v8::Nothing<ScriptValue>();
    else
      return v8::Just<ScriptValue>(native_result);
  }
}

v8::Maybe<ScriptValue> V8PersistentCallbackFunction<V8AnyCallbackFunctionVariadicAnyArgs>::Invoke(ScriptWrappable* callback_this_value, const Vector<ScriptValue>& arguments) {
  return Proxy()->Invoke(
      callback_this_value, arguments);
}

}  // namespace blink

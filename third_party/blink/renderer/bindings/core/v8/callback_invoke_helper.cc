// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/callback_invoke_helper.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/callback_function_base.h"
#include "third_party/blink/renderer/platform/bindings/callback_interface_base.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"

namespace blink {

namespace bindings {

namespace {

// These tricks will no longer be necessary once Chromium allows use of
// constexpr if statement (C++17 feature).
inline bool IsCallbackConstructor(CallbackFunctionBase* callback) {
  return callback->IsConstructor();
}
inline bool IsCallbackConstructor(CallbackInterfaceBase* callback) {
  NOTREACHED();
  return false;
}
inline bool IsCallbackObjectCallable(CallbackFunctionBase* callback) {
  NOTREACHED();
  return callback->CallbackObject()->IsFunction();
}
inline bool IsCallbackObjectCallable(CallbackInterfaceBase* callback) {
  return callback->IsCallbackObjectCallable();
}

}  // namespace

template <class CallbackBase, CallbackInvokeHelperMode mode>
bool CallbackInvokeHelper<CallbackBase, mode>::PrepareForCall(
    V8ValueOrScriptWrappableAdapter callback_this) {
  if (UNLIKELY(ScriptForbiddenScope::IsScriptForbidden())) {
    ScriptForbiddenScope::ThrowScriptForbiddenException(
        callback_->GetIsolate());
    return Abort();
  }

  if (mode == CallbackInvokeHelperMode::kConstructorCall) {
    // step 3. If ! IsConstructor(F) is false, throw a TypeError exception.
    if (!IsCallbackConstructor(callback_)) {
      ExceptionState exception_state(callback_->GetIsolate(),
                                     ExceptionState::kExecutionContext,
                                     class_like_name_, property_name_);
      exception_state.ThrowTypeError(
          "The provided callback is not a constructor.");
      return Abort();
    }
  }

  if (std::is_same<CallbackBase, CallbackFunctionBase>::value) {
    if (mode == CallbackInvokeHelperMode::kLegacyTreatNonObjectAsNull) {
      // step 4. If ! IsCallable(F) is false:
      if (!callback_->CallbackObject()->IsFunction()) {
        // step 4.2. Return the result of converting undefined to the callback
        // function's return type.
        result_ = v8::Undefined(callback_->GetIsolate());
        return false;
      }
    }
    DCHECK(callback_->CallbackObject()->IsFunction());
    function_ = callback_->CallbackObject().template As<v8::Function>();
  }
  if (std::is_same<CallbackBase, CallbackInterfaceBase>::value) {
    if (IsCallbackObjectCallable(callback_)) {
      function_ = callback_->CallbackObject().template As<v8::Function>();
    } else {
      // step 10. If ! IsCallable(O) is false, then:
      v8::Local<v8::Value> value;
      if (!callback_->CallbackObject()
               ->Get(callback_->CallbackRelevantScriptState()->GetContext(),
                     V8String(callback_->GetIsolate(), property_name_))
               .ToLocal(&value)) {
        return Abort();
      }
      if (!value->IsFunction()) {
        V8ThrowException::ThrowTypeError(
            callback_->GetIsolate(),
            ExceptionMessages::FailedToExecute(
                property_name_, class_like_name_,
                "The provided callback is not callable."));
        return Abort();
      }
      function_ = value.As<v8::Function>();
    }
  }

  if (mode == CallbackInvokeHelperMode::kConstructorCall) {
    // Do nothing.
  } else if (std::is_same<CallbackBase, CallbackInterfaceBase>::value &&
             !IsCallbackObjectCallable(callback_)) {
    // step 10.5. Set thisArg to O (overriding the provided value).
    callback_this_ = callback_->CallbackObject();
  } else if (callback_this.IsEmpty()) {
    // step 2. If thisArg was not given, let thisArg be undefined.
    callback_this_ = v8::Undefined(callback_->GetIsolate());
  } else {
    callback_this_ =
        callback_this.V8Value(callback_->CallbackRelevantScriptState());
  }

  return true;
}

template <class CallbackBase, CallbackInvokeHelperMode mode>
bool CallbackInvokeHelper<CallbackBase, mode>::Call(
    int argc,
    v8::Local<v8::Value>* argv) {
  if (mode == CallbackInvokeHelperMode::kConstructorCall) {
    // step 10. Let callResult be Construct(F, esArgs).
    if (!V8ScriptRunner::CallAsConstructor(
             callback_->GetIsolate(), function_,
             ExecutionContext::From(callback_->CallbackRelevantScriptState()),
             argc, argv)
             .ToLocal(&result_)) {
      return Abort();
    }
  } else {
    // step 12. Let callResult be Call(X, thisArg, esArgs).
    // or
    // step 11. Let callResult be Call(F, thisArg, esArgs).
    if (!V8ScriptRunner::CallFunction(
             function_,
             ExecutionContext::From(callback_->CallbackRelevantScriptState()),
             callback_this_, argc, argv, callback_->GetIsolate())
             .ToLocal(&result_)) {
      return Abort();
    }
  }
  return true;
}

template class CORE_TEMPLATE_EXPORT CallbackInvokeHelper<CallbackFunctionBase>;
template class CORE_TEMPLATE_EXPORT
    CallbackInvokeHelper<CallbackFunctionBase,
                         CallbackInvokeHelperMode::kConstructorCall>;
template class CORE_TEMPLATE_EXPORT
    CallbackInvokeHelper<CallbackFunctionBase,
                         CallbackInvokeHelperMode::kLegacyTreatNonObjectAsNull>;
template class CORE_TEMPLATE_EXPORT CallbackInvokeHelper<CallbackInterfaceBase>;

}  // namespace bindings

}  // namespace blink

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/bindings/core/v8/callback_invoke_helper.h"

#include "base/trace_event/trace_event.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_script_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/core_probes_inl.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/platform/bindings/callback_function_base.h"
#include "third_party/blink/renderer/platform/bindings/callback_interface_base.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/task_attribution_tracker.h"

namespace blink {

namespace bindings {

template <class CallbackBase,
          CallbackInvokeHelperMode mode,
          CallbackReturnTypeIsPromise return_type_is_promise>
bool CallbackInvokeHelper<CallbackBase, mode, return_type_is_promise>::
    PrepareForCall(V8ValueOrScriptWrappableAdapter callback_this) {
  v8::Isolate* isolate = callback_->GetIsolate();
  if (ScriptForbiddenScope::IsScriptForbidden()) [[unlikely]] {
    ScriptForbiddenScope::ThrowScriptForbiddenException(isolate);
    return Abort();
  }
  if (RuntimeEnabledFeatures::BlinkLifecycleScriptForbiddenEnabled()) {
    CHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
  } else {
    DCHECK(!ScriptForbiddenScope::WillBeScriptForbidden());
  }

  if constexpr (mode == CallbackInvokeHelperMode::kConstructorCall) {
    // step 3. If ! IsConstructor(F) is false, throw a TypeError exception.
    if (!callback_->IsConstructor()) {
      ExceptionState exception_state(isolate, v8::ExceptionContext::kOperation,
                                     class_like_name_, property_name_);
      exception_state.ThrowTypeError(
          "The provided callback is not a constructor.");
      return Abort();
    }
  }

  if constexpr (std::is_same<CallbackBase, CallbackFunctionBase>::value ||
                std::is_same<CallbackBase,
                             CallbackFunctionWithTaskAttributionBase>::value) {
    if constexpr (mode ==
                  CallbackInvokeHelperMode::kLegacyTreatNonObjectAsNull) {
      // step 4. If ! IsCallable(F) is false:
      if (!callback_->CallbackObject()->IsFunction()) {
        // step 4.2. Return the result of converting undefined to the callback
        // function's return type.
        result_ = v8::Undefined(isolate);
        return false;
      }
    }
    DCHECK(callback_->CallbackObject()->IsFunction());
    function_ = callback_->CallbackObject().template As<v8::Function>();
  }
  if constexpr (std::is_same<CallbackBase, CallbackInterfaceBase>::value) {
    if (callback_->IsCallbackObjectCallable()) {
      function_ = callback_->CallbackObject().template As<v8::Function>();
    } else {
      // step 10. If ! IsCallable(O) is false, then:
      v8::MicrotaskQueue* microtask_queue =
          ToMicrotaskQueue(callback_->CallbackRelevantScriptState());
      v8::MicrotasksScope microtasks_scope(isolate, microtask_queue,
                                           v8::MicrotasksScope::kRunMicrotasks);

      v8::Local<v8::Value> value;
      if (!callback_->CallbackObject()
               ->Get(callback_->CallbackRelevantScriptState()->GetContext(),
                     V8String(isolate, property_name_))
               .ToLocal(&value)) {
        return Abort();
      }
      if (!value->IsFunction()) {
        V8ThrowException::ThrowTypeError(
            isolate, ExceptionMessages::FailedToExecute(
                         property_name_, class_like_name_,
                         "The provided callback is not callable."));
        return Abort();
      }
      function_ = value.As<v8::Function>();
    }
  }

  if constexpr (mode != CallbackInvokeHelperMode::kConstructorCall) {
    bool is_callable = true;
    if constexpr (std::is_same<CallbackBase, CallbackInterfaceBase>::value)
      is_callable = callback_->IsCallbackObjectCallable();
    if (!is_callable) {
      // step 10.5. Set thisArg to O (overriding the provided value).
      callback_this_ = callback_->CallbackObject();
    } else if (callback_this.IsEmpty()) {
      // step 2. If thisArg was not given, let thisArg be undefined.
      callback_this_ = v8::Undefined(isolate);
    } else {
      callback_this_ =
          callback_this.V8Value(callback_->CallbackRelevantScriptState());
    }
    if (auto* tracker = scheduler::TaskAttributionTracker::From(isolate)) {
      scheduler::TaskAttributionInfo* task_state_to_propagate = nullptr;
      if constexpr (std::is_same<
                        CallbackBase,
                        CallbackFunctionWithTaskAttributionBase>::value) {
        task_state_to_propagate = callback_->GetParentTask();
      }
      task_attribution_scope_ = tracker->MaybeCreateTaskScopeForCallback(
          callback_->CallbackRelevantScriptState(), task_state_to_propagate);
    }
  }

  return true;
}

template <class CallbackBase,
          CallbackInvokeHelperMode mode,
          CallbackReturnTypeIsPromise return_type_is_promise>
bool CallbackInvokeHelper<CallbackBase, mode, return_type_is_promise>::
    CallInternal(int argc, v8::Local<v8::Value>* argv) {
  ScriptState* script_state = callback_->CallbackRelevantScriptState();
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  probe::InvokeCallback probe_scope(script_state, class_like_name_,
                                    /*callback=*/nullptr, function_);

  if constexpr (mode == CallbackInvokeHelperMode::kConstructorCall) {
    // step 10. Let callResult be Construct(F, esArgs).
    return V8ScriptRunner::CallAsConstructor(callback_->GetIsolate(), function_,
                                             execution_context, argc, argv)
        .ToLocal(&result_);
  } else {
    // step 12. Let callResult be Call(X, thisArg, esArgs).
    // or
    // step 11. Let callResult be Call(F, thisArg, esArgs).
    return V8ScriptRunner::CallFunction(function_, execution_context,
                                        callback_this_, argc, argv,
                                        callback_->GetIsolate())
        .ToLocal(&result_);
  }
}

template <class CallbackBase,
          CallbackInvokeHelperMode mode,
          CallbackReturnTypeIsPromise return_type_is_promise>
bool CallbackInvokeHelper<CallbackBase, mode, return_type_is_promise>::Call(
    int argc,
    v8::Local<v8::Value>* argv) {
  if constexpr (return_type_is_promise == CallbackReturnTypeIsPromise::kYes) {
    v8::TryCatch block(callback_->GetIsolate());
    if (!CallInternal(argc, argv)) {
      result_ = ScriptPromiseUntyped::Reject(
                    callback_->CallbackRelevantScriptState(), block.Exception())
                    .V8Value();
    }
  } else {
    if (!CallInternal(argc, argv))
      return Abort();
  }
  return true;
}

template class CORE_TEMPLATE_EXPORT
    CallbackInvokeHelper<CallbackFunctionBase,
                         CallbackInvokeHelperMode::kDefault>;
template class CORE_TEMPLATE_EXPORT
    CallbackInvokeHelper<CallbackFunctionBase,
                         CallbackInvokeHelperMode::kDefault,
                         CallbackReturnTypeIsPromise::kYes>;
template class CORE_TEMPLATE_EXPORT
    CallbackInvokeHelper<CallbackFunctionBase,
                         CallbackInvokeHelperMode::kConstructorCall>;
template class CORE_TEMPLATE_EXPORT
    CallbackInvokeHelper<CallbackFunctionBase,
                         CallbackInvokeHelperMode::kConstructorCall,
                         CallbackReturnTypeIsPromise::kYes>;
template class CORE_TEMPLATE_EXPORT
    CallbackInvokeHelper<CallbackFunctionBase,
                         CallbackInvokeHelperMode::kLegacyTreatNonObjectAsNull>;

template class CORE_TEMPLATE_EXPORT
    CallbackInvokeHelper<CallbackFunctionWithTaskAttributionBase,
                         CallbackInvokeHelperMode::kDefault>;
template class CORE_TEMPLATE_EXPORT
    CallbackInvokeHelper<CallbackFunctionWithTaskAttributionBase,
                         CallbackInvokeHelperMode::kDefault,
                         CallbackReturnTypeIsPromise::kYes>;
template class CORE_TEMPLATE_EXPORT
    CallbackInvokeHelper<CallbackFunctionWithTaskAttributionBase,
                         CallbackInvokeHelperMode::kConstructorCall>;
template class CORE_TEMPLATE_EXPORT
    CallbackInvokeHelper<CallbackFunctionWithTaskAttributionBase,
                         CallbackInvokeHelperMode::kConstructorCall,
                         CallbackReturnTypeIsPromise::kYes>;
template class CORE_TEMPLATE_EXPORT
    CallbackInvokeHelper<CallbackFunctionWithTaskAttributionBase,
                         CallbackInvokeHelperMode::kLegacyTreatNonObjectAsNull>;

template class CORE_TEMPLATE_EXPORT CallbackInvokeHelper<CallbackInterfaceBase>;

}  // namespace bindings

}  // namespace blink

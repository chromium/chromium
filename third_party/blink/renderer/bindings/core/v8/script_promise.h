/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/stack_allocated.h"
#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/native_value_traits_impl.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/world_safe_v8_reference.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;

template <typename IDLResolvedType>
class ScriptPromise;

// Defined here rather than in native_value_traits_impl.h to avoid a circular
// dependency.
template <typename T>
struct NativeValueTraits<IDLPromise<T>>
    : public NativeValueTraitsBase<IDLPromise<T>> {
  static ScriptPromise<T> NativeValue(v8::Isolate* isolate,
                                      v8::Local<v8::Value> value,
                                      ExceptionState&) {
    return ScriptPromise<T>::FromV8Value(ScriptState::ForCurrentRealm(isolate),
                                         std::move(value));
  }
};

// Defined here rather than in to_v8_traits.h to avoid a circular dependency.
template <typename T>
struct ToV8Traits<IDLPromise<T>> {
  [[nodiscard]] static v8::Local<v8::Value> ToV8(
      ScriptState* script_state,
      const ScriptPromise<T>& script_promise) {
    DCHECK(!script_promise.IsEmpty());
    return script_promise.V8Promise();
  }
  [[nodiscard]] static v8::Local<v8::Value> ToV8(
      ScriptState* script_state,
      v8::Local<v8::Promise> script_promise) {
    return script_promise;
  }
};

// Base class for passing in to ScriptPromise::Then()/React(), and being
// notified of promise resolution. Handles v8->blink type conversions, and
// converts type mismatches into rejections.
// All subclasses must implement `React()`, taking a ScriptState*, and the
// expected blink type (unless listening to an undefined promise, in which case
// the second parameter is omitted).
// `IDLType` must match ScriptPromise<IDLType>::Then()/React().
// `Derived` is the name of your derived class.
// `ThenReturnType` is the return type of your React() function. Only required
// when calling `Then()`, and your React() handling must return a blink
// type that ToV8Traits<> knows how to convert to `ThenReturnType`.
template <typename IDLType,
          typename Derived,
          typename ThenReturnType = IDLUndefined>
class CORE_EXPORT ThenCallable : public ScriptFunction {
 public:
  ~ThenCallable() override = default;

  void SetTypingFailureCallable(ScriptFunction* callable) {
    typing_failure_callable_ = callable;
  }

  void SetExceptionContext(const ExceptionContext& context) {
    context_ = context;
  }

  void Trace(Visitor* visitor) const override {
    ScriptFunction::Trace(visitor);
    visitor->Trace(typing_failure_callable_);
  }

 private:
  ScriptValue Call(ScriptState* script_state, ScriptValue value) final {
    v8::Isolate* isolate = script_state->GetIsolate();
    v8::TryCatch try_catch(isolate);
    ScriptValue return_value;

    if constexpr (std::is_same_v<IDLType, IDLUndefined>) {
      if constexpr (!std::is_same_v<IDLPromise<IDLUndefined>, ThenReturnType>) {
        // Base undefined case: no input value, no return value.
        static_cast<Derived*>(this)->React(script_state);
      } else {
        // Chain promises that resolve with undefined.
        return_value = ScriptValue(
            isolate, ToV8Traits<ThenReturnType>::ToV8(
                         script_state,
                         static_cast<Derived*>(this)->React(script_state)));
      }
    } else {
      // Resolve type is not undefined - convert it to the expected type.
      auto&& blink_value = NativeValueTraits<IDLType>::NativeValue(
          isolate, value.V8Value(), PassThroughException(isolate));

      if (try_catch.HasCaught()) [[unlikely]] {
        // Typing failure: convert to promise rejection.
        DCHECK(typing_failure_callable_);
        return_value = typing_failure_callable_->Call(
            script_state, ScriptValue(isolate, try_catch.Exception()));
      } else {
        if constexpr (std::is_same_v<IDLUndefined, ThenReturnType>) {
          // Promise resolves with a value, but this callable is not expected to
          // return anything (no chaining).
          static_cast<Derived*>(this)->React(script_state,
                                             std::move(blink_value));
        } else {
          // Promise resolves with a value, and is chaining.
          return_value = ScriptValue(
              isolate,
              ToV8Traits<ThenReturnType>::ToV8(
                  script_state, static_cast<Derived*>(this)->React(
                                    script_state, std::move(blink_value))));
        }
      }
    }

    // Finally: apply exception context and rethrow if needed, and return the
    // result.
    if (try_catch.HasCaught()) [[unlikely]] {
      ApplyContextToException(script_state, try_catch.Exception(), context_);
      try_catch.ReThrow();
    }
    return return_value;
  }

  Member<ScriptFunction> typing_failure_callable_;
  ExceptionContext context_ =
      ExceptionContext(v8::ExceptionContext::kUnknown, nullptr, nullptr);
};

// ScriptPromise is the class for representing Promise values in C++
// world. ScriptPromise holds a Promise. Holding a `ScriptPromise`
// is rarely needed — typically you hold a `ScriptPromiseResolver` when creating
// a Promise and passing it *to* JavaScript — but is necessary when
// holding a promise received *from* JavaScript. If a promise is exposed as an
// attribute in IDL and you need to return the same promise on multiple
// invocations, use ScriptPromiseProperty.
//
// There are cases where promises cannot work (e.g., where the thread is being
// terminated). In such cases operations will silently fail, so you should not
// use promises for critical use such as releasing a resource.
template <typename IDLResolvedType>
class ScriptPromise {
  DISALLOW_NEW();

 public:
  ScriptPromise() = default;

  static ScriptPromise<IDLResolvedType> FromV8Promise(
      v8::Isolate* isolate,
      v8::Local<v8::Promise> promise) {
    return ScriptPromise<IDLResolvedType>(isolate, promise);
  }

  static ScriptPromise<IDLResolvedType> FromV8Value(
      ScriptState* script_state,
      v8::Local<v8::Value> value) {
    if (value.IsEmpty()) {
      return ScriptPromise<IDLResolvedType>();
    }

    v8::Isolate* isolate = script_state->GetIsolate();
    if (value->IsPromise()) {
      return ScriptPromise<IDLResolvedType>(isolate, value.As<v8::Promise>());
    }

    v8::Local<v8::Context> context = script_state->GetContext();
    v8::MicrotasksScope microtasks_scope(
        isolate, ToMicrotaskQueue(script_state),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    auto resolver = v8::Promise::Resolver::New(context).ToLocalChecked();
    std::ignore = resolver->Resolve(context, value);
    return ScriptPromise<IDLResolvedType>(isolate, resolver->GetPromise());
  }

  static ScriptPromise<IDLResolvedType> RejectWithDOMException(
      ScriptState* script_state,
      DOMException* exception) {
    return Reject(script_state, exception->ToV8(script_state));
  }

  static ScriptPromise<IDLResolvedType> Reject(ScriptState* script_state,
                                               const ScriptValue& value) {
    return Reject(script_state, value.V8Value());
  }

  static ScriptPromise<IDLResolvedType> Reject(ScriptState* script_state,
                                               v8::Local<v8::Value> value) {
    if (value.IsEmpty()) {
      return ScriptPromise<IDLResolvedType>();
    }
    v8::Isolate* isolate = script_state->GetIsolate();
    v8::Local<v8::Context> context = script_state->GetContext();
    v8::MicrotasksScope microtasks_scope(
        isolate, ToMicrotaskQueue(script_state),
        v8::MicrotasksScope::kDoNotRunMicrotasks);
    auto resolver = v8::Promise::Resolver::New(context).ToLocalChecked();
    std::ignore = resolver->Reject(context, value);
    return ScriptPromise<IDLResolvedType>(isolate, resolver->GetPromise());
  }

  v8::Local<v8::Promise> V8Promise() const {
    return IsEmpty() ? v8::Local<v8::Promise>()
                     : promise_.Get(ScriptState::ForCurrentRealm(isolate_));
  }

  bool IsEmpty() const { return promise_.IsEmpty(); }
  void Clear() { promise_.Reset(); }

  // Marks this promise as handled to avoid reporting unhandled rejections.
  void MarkAsHandled() {
    if (!IsEmpty()) {
      V8Promise()->MarkAsHandled();
    }
  }

  void MarkAsSilent() {
    if (!IsEmpty()) {
      V8Promise()->MarkAsSilent();
    }
  }

  bool operator==(const ScriptPromise<IDLResolvedType>& value) const {
    return promise_ == value.promise_;
  }

  bool operator!=(const ScriptPromise<IDLResolvedType>& value) const {
    return !operator==(value);
  }

  void Trace(Visitor* visitor) const { visitor->Trace(promise_); }

  template <typename ReturnPromiseResolveType = IDLResolvedType,
            typename ResolveClass>
  ScriptPromise<ReturnPromiseResolveType> Then(
      ScriptState* script_state,
      ThenCallable<IDLResolvedType, ResolveClass, ReturnPromiseResolveType>*
          on_fulfilled) const {
    if (IsEmpty()) {
      return ScriptPromise<ReturnPromiseResolveType>();
    }
    v8::Local<v8::Promise> v8_promise =
        V8Promise()
            ->Then(script_state->GetContext(),
                   on_fulfilled->ToV8Function(script_state))
            .FromMaybe(v8::Local<v8::Promise>());
    return ScriptPromise<ReturnPromiseResolveType>::FromV8Promise(
        script_state->GetIsolate(), v8_promise);
  }

  template <typename ReturnPromiseResolveType = IDLResolvedType,
            typename ReturnPromiseRejectType = IDLUndefined,
            typename ResolveClass,
            typename RejectClass>
  ScriptPromise<ReturnPromiseResolveType> Then(
      ScriptState* script_state,
      ThenCallable<IDLResolvedType, ResolveClass, ReturnPromiseResolveType>*
          on_fulfilled,
      ThenCallable<IDLAny, RejectClass, ReturnPromiseRejectType>* on_rejected)
      const {
    if (IsEmpty()) {
      return ScriptPromise<ReturnPromiseResolveType>();
    }
    on_fulfilled->SetTypingFailureCallable(on_rejected);
    v8::Local<v8::Promise> v8_promise =
        V8Promise()
            ->Then(script_state->GetContext(),
                   on_fulfilled->ToV8Function(script_state),
                   on_rejected->ToV8Function(script_state))
            .FromMaybe(v8::Local<v8::Promise>());
    return ScriptPromise<ReturnPromiseResolveType>::FromV8Promise(
        script_state->GetIsolate(), v8_promise);
  }

  // For chaining promises in ThenCallable<>::React().
  template <typename ReturnPromiseResolveType, typename ResolveClass>
  ScriptPromise<ReturnPromiseResolveType> Then(
      ScriptState* script_state,
      ThenCallable<IDLResolvedType,
                   ResolveClass,
                   IDLPromise<ReturnPromiseResolveType>>* on_fulfilled) const {
    if (IsEmpty()) {
      return ScriptPromise<ReturnPromiseResolveType>();
    }
    v8::Local<v8::Promise> v8_promise =
        V8Promise()
            ->Then(script_state->GetContext(),
                   on_fulfilled->ToV8Function(script_state))
            .FromMaybe(v8::Local<v8::Promise>());
    return ScriptPromise<ReturnPromiseResolveType>::FromV8Promise(
        script_state->GetIsolate(), v8_promise);
  }

  template <typename ResolveClass>
  void React(ScriptState* script_state,
             ThenCallable<IDLResolvedType, ResolveClass, IDLUndefined>*
                 on_fulfilled) const {
    if (IsEmpty()) {
      return;
    }
    std::ignore = V8Promise()->Then(script_state->GetContext(),
                                    on_fulfilled->ToV8Function(script_state));
  }

  template <typename ResolveClass, typename RejectClass>
  void React(
      ScriptState* script_state,
      ThenCallable<IDLResolvedType, ResolveClass, IDLUndefined>* on_fulfilled,
      ThenCallable<IDLAny, RejectClass, IDLUndefined>* on_rejected) const {
    if (IsEmpty()) {
      return;
    }
    on_fulfilled->SetTypingFailureCallable(on_rejected);
    std::ignore = V8Promise()->Then(script_state->GetContext(),
                                    on_fulfilled->ToV8Function(script_state),
                                    on_rejected->ToV8Function(script_state));
  }

  template <typename ResolveClass, typename RejectClass>
  void ReactNoTypeChecks(
      ScriptState* script_state,
      ThenCallable<IDLAny, ResolveClass, IDLUndefined>* on_fulfilled,
      ThenCallable<IDLAny, RejectClass, IDLUndefined>* on_rejected) const {
    if (IsEmpty()) {
      return;
    }
    std::ignore = V8Promise()->Then(script_state->GetContext(),
                                    on_fulfilled->ToV8Function(script_state),
                                    on_rejected->ToV8Function(script_state));
  }

  template <typename RejectClass>
  void Catch(
      ScriptState* script_state,
      ThenCallable<IDLAny, RejectClass, IDLUndefined>* on_rejected) const {
    if (IsEmpty()) {
      return;
    }
    std::ignore = V8Promise()->Catch(script_state->GetContext(),
                                     on_rejected->ToV8Function(script_state));
  }

 private:
  template <typename IDLType>
  friend class ScriptPromiseResolver;

  ScriptPromise(v8::Isolate* isolate, v8::Local<v8::Promise> promise)
      : isolate_(isolate), promise_(isolate, promise) {}

  v8::Isolate* isolate_ = nullptr;
  WorldSafeV8Reference<v8::Promise> promise_;
};

template <typename IDLType, typename BlinkType>
ScriptPromise<IDLType> ToResolvedPromise(ScriptState* script_state,
                                         BlinkType value) {
  return ScriptPromise<IDLType>::FromV8Value(
      script_state, ToV8Traits<IDLType>::ToV8(script_state, value));
}

inline ScriptPromise<IDLUndefined> ToResolvedUndefinedPromise(
    ScriptState* script_state) {
  return ToResolvedPromise<IDLUndefined>(script_state,
                                         ToV8UndefinedGenerator());
}

// EmptyPromise() is a value similar to std::nullopt that can be used to return
// an empty ScriptPromise of any type. It is intended to be used when throwing
// an exception using an ExceptionState object, since in that case the bindings
// ignore the contents of the returned promise.
//
// The usual patterns for usage are:
//
//   if (bad thing) {
//     exception_state.ThrowRangeError("bad thing");
//     return EmptyPromise();
//   }
//
// or
//
//   FunctionThatMightThrow(script_state, exception_state);
//   if (exception_state.HadException()) {
//     return EmptyPromise();
//   }
class EmptyPromise {
  STACK_ALLOCATED();

 public:
  template <typename IDLType>
  // Intentionally permit implicit conversion. NOLINTNEXTLINE.
  operator ScriptPromise<IDLType>() {
    return ScriptPromise<IDLType>();
  }
};

}  // namespace blink

namespace WTF {

template <typename T>
struct VectorTraits<blink::ScriptPromise<T>>
    : VectorTraitsBase<blink::ScriptPromise<T>> {
  STATIC_ONLY(VectorTraits);
  static constexpr bool kCanClearUnusedSlotsWithMemset = true;
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_H_

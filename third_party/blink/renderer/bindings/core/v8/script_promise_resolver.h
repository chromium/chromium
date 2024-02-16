// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_RESOLVER_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_RESOLVER_H_

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/execution_context/execution_context_lifecycle_observer.h"
#include "third_party/blink/renderer/platform/bindings/dictionary_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_context.h"
#include "third_party/blink/renderer/platform/bindings/scoped_persistent.h"
#include "third_party/blink/renderer/platform/bindings/script_forbidden_scope.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/union_base.h"
#include "third_party/blink/renderer/platform/heap/disallow_new_wrapper.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/prefinalizer.h"
#include "third_party/blink/renderer/platform/heap/self_keep_alive.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cancellable_task.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "v8/include/v8.h"

#if DCHECK_IS_ON()
#include "base/debug/stack_trace.h"
#endif

namespace blink {

template <typename IDLResolvedType>
class ScriptPromiseResolverTyped;

// This class wraps v8::Promise::Resolver and provides the following
// functionalities.
//  - A ScriptPromiseResolver retains a ScriptState. A caller
//    can call resolve or reject from outside of a V8 context.
//  - This class is an ExecutionContextLifecycleObserver and keeps track of the
//    associated ExecutionContext state. When it is stopped, resolve or reject
//    will be ignored.
//
// There are cases where promises cannot work (e.g., where the thread is being
// terminated). In such cases operations will silently fail.
class CORE_EXPORT ScriptPromiseResolver
    : public GarbageCollected<ScriptPromiseResolver>,
      public ExecutionContextLifecycleObserver {
  USING_PRE_FINALIZER(ScriptPromiseResolver, Dispose);

 public:
  explicit ScriptPromiseResolver(ScriptState*);
  // Use this constructor if resolver is intended to be used in a callback
  // function to reject with exception. ExceptionState will be used for
  // creating exceptions in functions like RejectWithDOMException method.
  explicit ScriptPromiseResolver(ScriptState*, const ExceptionContext&);

  ScriptPromiseResolver(const ScriptPromiseResolver&) = delete;
  ScriptPromiseResolver& operator=(const ScriptPromiseResolver&) = delete;

  ~ScriptPromiseResolver() override;

  void Dispose();

  // Anything that can be passed to ToV8Traits can be passed to this function.
  template <typename IDLType, typename BlinkType>
  void Reject(BlinkType value) {
    ResolveOrReject<IDLType, BlinkType>(value, kRejecting);
  }

  // These are shorthand helpers for rejecting the promise with a common type.
  // Use the templated Reject<IDLType>() variant for uncommon types.
  void Reject(DOMException*);
  void Reject(v8::Local<v8::Value>);
  void Reject(const ScriptValue&);
  void Reject(const char*);
  void Reject(bool);

  // Anything that can be passed to toV8 can be passed to this function.
  template <typename T>
  void Resolve(T value) {
    ResolveOrReject(value, kResolving);
  }

  void Resolve() { Resolve(ToV8UndefinedGenerator()); }
  void Reject() { Reject<IDLUndefined>(ToV8UndefinedGenerator()); }

  // Returns a callback that will run |callback| with the Entry realm
  // and the Current realm set to the resolver's ScriptState. Note |callback|
  // will only be run if the execution context and V8 context are capable
  // to run. This situation occurs when the resolver's execution context
  // or V8 context have started their destruction. See
  // `IsInParallelAlgorithmRunnable` for details.
  template <class ScriptPromiseResolver, typename... Args>
  base::OnceCallback<void(Args...)> WrapCallbackInScriptScope(
      base::OnceCallback<void(ScriptPromiseResolver*, Args...)> callback) {
    return WTF::BindOnce(
        [](ScriptPromiseResolver* resolver,
           base::OnceCallback<void(ScriptPromiseResolver*, Args...)> callback,
           Args... args) {
          ScriptState* script_state = resolver->GetScriptState();
          if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                             script_state)) {
            return;
          }
          ScriptState::Scope script_state_scope(script_state);
          std::move(callback).Run(resolver, std::move(args)...);
        },
        WrapPersistent(this), std::move(callback));
  }

  // Reject with a given exception.
  void Reject(ExceptionState&);

  // Following functions create exceptions using ExceptionState.
  // They require ScriptPromiseResolver to be created with ExceptionContext.

  // Reject with DOMException with given exception code.
  void RejectWithDOMException(DOMExceptionCode exception_code,
                              const String& message);
  // Reject with DOMException with SECURITY_ERR.
  void RejectWithSecurityError(const String& sanitized_message,
                               const String& unsanitized_message);
  // Reject with ECMAScript Error object.
  void RejectWithTypeError(const String& message);
  void RejectWithRangeError(const String& message);

  // Reject with WebAssembly Error object.
  void RejectWithWasmCompileError(const String& message);

  ScriptState* GetScriptState() const { return script_state_.Get(); }

  const ExceptionContext& GetExceptionContext() const {
    return exception_context_;
  }

  // Note that an empty ScriptPromise will be returned after resolve or
  // reject is called.
  ScriptPromise Promise() {
#if DCHECK_IS_ON()
    is_promise_called_ = true;
#endif
    return resolver_.Promise();
  }

  template <typename IDLResolvedType>
  ScriptPromiseResolverTyped<IDLResolvedType>* DowncastTo() {
#if DCHECK_IS_ON()
    DCHECK_EQ(runtime_type_id_,
              GetTypeId<ScriptPromiseResolverTyped<IDLResolvedType>>());
#endif
    return static_cast<ScriptPromiseResolverTyped<IDLResolvedType>*>(this);
  }

  // ExecutionContextLifecycleObserver implementation.
  void ContextDestroyed() override { Detach(); }

  // Calling this function makes the resolver release its internal resources.
  // That means the associated promise will never be resolved or rejected
  // unless it's already been resolved or rejected.
  // Do not call this function unless you truly need the behavior.
  void Detach();

  // Suppresses the check in Dispose. Do not use this function unless you truly
  // need the behavior. Also consider using Detach().
  void SuppressDetachCheck() {
#if DCHECK_IS_ON()
    suppress_detach_check_ = true;
#endif
  }

  // Once this function is called this resolver stays alive while the
  // promise is pending and the associated ExecutionContext isn't stopped.
  void KeepAliveWhilePending();

  void Trace(Visitor*) const override;

 protected:
  typedef ScriptPromise::InternalResolver Resolver;

  ScriptPromiseResolver(ScriptState*, const ExceptionContext&, Resolver);

  Resolver resolver_;

#if DCHECK_IS_ON()
  template <typename T>
  struct FastTypeTag {
    constexpr static char type = 0;
  };
  template <typename T>
  constexpr inline const void* GetTypeId() {
    return &FastTypeTag<T>::type;
  }

  // True if promise() is called.
  bool is_promise_called_ = false;
  const void* runtime_type_id_ = 0;
#endif

  class ExceptionStateScope;
  enum ResolutionState {
    kPending,
    kResolving,
    kRejecting,
    kDetached,
  };

  template <typename IDLType, typename BlinkType>
  void ResolveOrReject(BlinkType value, ResolutionState new_state) {
    if (state_ != kPending || !GetScriptState()->ContextIsValid() ||
        !GetExecutionContext() || GetExecutionContext()->IsContextDestroyed()) {
      return;
    }
    DCHECK(new_state == kResolving || new_state == kRejecting);
    state_ = new_state;

    ScriptState::Scope scope(script_state_.Get());

    // Calling ToV8 in a ScriptForbiddenScope will trigger a CHECK and
    // cause a crash. ToV8 just invokes a constructor for wrapper creation,
    // which is safe (no author script can be run). Adding AllowUserAgentScript
    // directly inside createWrapper could cause a perf impact (calling
    // isMainThread() every time a wrapper is created is expensive). Ideally,
    // resolveOrReject shouldn't be called inside a ScriptForbiddenScope.
    {
      ScriptForbiddenScope::AllowUserAgentScript allow_script;
      v8::Isolate* isolate = script_state_->GetIsolate();
      v8::MicrotasksScope microtasks_scope(
          isolate, ToMicrotaskQueue(script_state_.Get()),
          v8::MicrotasksScope::kDoNotRunMicrotasks);
      value_.Reset(isolate, ToV8Traits<IDLType>::ToV8(script_state_, value));
    }

    if (GetExecutionContext()->IsContextPaused()) {
      ScheduleResolveOrReject();
      return;
    }
    // TODO(esprehn): This is a hack, instead we should CHECK that
    // script is allowed, and v8 should be running the entry hooks below and
    // crashing if script is forbidden. We should then audit all users of
    // ScriptPromiseResolver and the related specs and switch to an async
    // resolve.
    // See: http://crbug.com/663476
    if (ScriptForbiddenScope::IsScriptForbidden()) {
      ScheduleResolveOrReject();
      return;
    }
    ResolveOrRejectImmediately();
  }

 private:
  template <typename T>
  void ResolveOrReject(T value, ResolutionState new_state) {
    if (state_ != kPending || !GetScriptState()->ContextIsValid() ||
        !GetExecutionContext() || GetExecutionContext()->IsContextDestroyed())
      return;
    DCHECK(new_state == kResolving || new_state == kRejecting);
    state_ = new_state;

    ScriptState::Scope scope(script_state_.Get());

    // Calling ToV8 in a ScriptForbiddenScope will trigger a CHECK and
    // cause a crash. ToV8 just invokes a constructor for wrapper creation,
    // which is safe (no author script can be run). Adding AllowUserAgentScript
    // directly inside createWrapper could cause a perf impact (calling
    // isMainThread() every time a wrapper is created is expensive). Ideally,
    // resolveOrReject shouldn't be called inside a ScriptForbiddenScope.
    {
      ScriptForbiddenScope::AllowUserAgentScript allow_script;
      v8::Isolate* isolate = script_state_->GetIsolate();
      v8::MicrotasksScope microtasks_scope(
          isolate, ToMicrotaskQueue(script_state_.Get()),
          v8::MicrotasksScope::kDoNotRunMicrotasks);
      value_.Reset(isolate, ToV8(value, script_state_->GetContext()->Global(),
                                 script_state_->GetIsolate()));
    }

    if (GetExecutionContext()->IsContextPaused()) {
      ScheduleResolveOrReject();
      return;
    }
    // TODO(esprehn): This is a hack, instead we should CHECK that
    // script is allowed, and v8 should be running the entry hooks below and
    // crashing if script is forbidden. We should then audit all users of
    // ScriptPromiseResolver and the related specs and switch to an async
    // resolve.
    // See: http://crbug.com/663476
    if (ScriptForbiddenScope::IsScriptForbidden()) {
      ScheduleResolveOrReject();
      return;
    }
    ResolveOrRejectImmediately();
  }

  void ResolveOrRejectImmediately();
  void ScheduleResolveOrReject();
  void ResolveOrRejectDeferred();

  // ScriptWrappable
  static v8::Local<v8::Value> ToV8(ScriptWrappable* impl,
                                   v8::Local<v8::Object> creation_context,
                                   v8::Isolate* isolate) {
    if (UNLIKELY(!impl)) {
      return v8::Null(isolate);
    }
    return impl->ToV8(isolate, creation_context);
  }

  // Dictionary
  static v8::Local<v8::Value> ToV8(const bindings::DictionaryBase* dictionary,
                                   v8::Local<v8::Object> creation_context,
                                   v8::Isolate* isolate) {
    if (UNLIKELY(!dictionary)) {
      return v8::Null(isolate);
    }
    ScriptState* script_state =
        ScriptState::From(creation_context->GetCreationContextChecked());
    return dictionary->ToV8(script_state);
  }

  // Union
  static v8::Local<v8::Value> ToV8(const bindings::UnionBase* union_value,
                                   v8::Local<v8::Object> creation_context,
                                   v8::Isolate* isolate) {
    return union_value->ToV8(
        ScriptState::From(creation_context->GetCreationContextChecked()));
  }

  // Primitives
  static v8::Local<v8::Value> ToV8(const String& value,
                                   v8::Local<v8::Object> creation_context,
                                   v8::Isolate* isolate) {
    return V8String(isolate, value);
  }

  static v8::Local<v8::Value> ToV8(const char* value,
                                   v8::Local<v8::Object> creation_context,
                                   v8::Isolate* isolate) {
    return V8String(isolate, value);
  }

  static v8::Local<v8::Value> ToV8(int32_t value,
                                   v8::Local<v8::Object> creation_context,
                                   v8::Isolate* isolate) {
    return v8::Integer::New(isolate, value);
  }

  static v8::Local<v8::Value> ToV8(int64_t value,
                                   v8::Local<v8::Object> creation_context,
                                   v8::Isolate* isolate) {
    int32_t value_in32_bit = static_cast<int32_t>(value);
    if (value_in32_bit == value) {
      return v8::Integer::New(isolate, value_in32_bit);
    }
    // V8 doesn't have a 64-bit integer implementation.
    return v8::Number::New(isolate, value);
  }

  static v8::Local<v8::Value> ToV8(uint32_t value,
                                   v8::Local<v8::Object> creation_context,
                                   v8::Isolate* isolate) {
    return v8::Integer::NewFromUnsigned(isolate, value);
  }

  static v8::Local<v8::Value> ToV8(uint64_t value,
                                   v8::Local<v8::Object> creation_context,
                                   v8::Isolate* isolate) {
    uint32_t value_in32_bit = static_cast<uint32_t>(value);
    if (value_in32_bit == value) {
      return v8::Integer::NewFromUnsigned(isolate, value_in32_bit);
    }
    // V8 doesn't have a 64-bit integer implementation.
    return v8::Number::New(isolate, value);
  }

  static v8::Local<v8::Value> ToV8(bool value,
                                   v8::Local<v8::Object> creation_context,
                                   v8::Isolate* isolate) {
    return v8::Boolean::New(isolate, value);
  }

  // Identity operator
  static v8::Local<v8::Value> ToV8(v8::Local<v8::Value> value,
                                   v8::Local<v8::Object> creation_context,
                                   v8::Isolate*) {
    return value;
  }

  // Undefined
  static v8::Local<v8::Value> ToV8(const ToV8UndefinedGenerator& value,
                                   v8::Local<v8::Object> creation_context,
                                   v8::Isolate* isolate) {
    return v8::Undefined(isolate);
  }

  ResolutionState state_;
  const Member<ScriptState> script_state_;
  TaskHandle deferred_resolve_task_;
  TraceWrapperV8Reference<v8::Value> value_;
  const ExceptionContext exception_context_;
  String script_url_;

  // To support keepAliveWhilePending(), this object needs to keep itself
  // alive while in that state.
  SelfKeepAlive<ScriptPromiseResolver> keep_alive_;

#if DCHECK_IS_ON()
  bool suppress_detach_check_ = false;

  base::debug::StackTrace create_stack_trace_{8};
#endif
};

template <typename IDLResolvedType>
class ScriptPromiseResolverTyped : public ScriptPromiseResolver {
 public:
  explicit ScriptPromiseResolverTyped(ScriptState* script_state)
      : ScriptPromiseResolverTyped(
            script_state,
            ExceptionContext(ExceptionContextType::kUnknown,
                             nullptr,
                             nullptr)) {}

  ScriptPromiseResolverTyped(ScriptState* script_state,
                             const ExceptionContext& context)
      : ScriptPromiseResolver(script_state,
                              context,
                              TypedResolver(script_state)) {
#if DCHECK_IS_ON()
    runtime_type_id_ = GetTypeId<ScriptPromiseResolverTyped<IDLResolvedType>>();
#endif
  }

  // Anything that can be passed to ToV8Traits<IDLResolvedType> can be passed to
  // this function.
  template <typename BlinkType>
  void Resolve(BlinkType value) {
    ResolveOrReject<IDLResolvedType, BlinkType>(value, kResolving);
  }

  void Resolve() { ScriptPromiseResolver::Resolve(); }

  ScriptPromiseTyped<IDLResolvedType> Promise() {
#if DCHECK_IS_ON()
    is_promise_called_ = true;
#endif
    return TypedResolver::GetTyped(resolver_).Promise();
  }

  // Returns a callback that will run |callback| with the Entry realm
  // and the Current realm set to the resolver's ScriptState. Note |callback|
  // will only be run if the execution context and V8 context are capable
  // to run. This situation occurs when the resolver's execution context
  // or V8 context have started their destruction. See
  // `IsInParallelAlgorithmRunnable` for details.
  template <typename... Args>
  base::OnceCallback<void(Args...)> WrapCallbackInScriptScope(
      base::OnceCallback<void(ScriptPromiseResolverTyped<IDLResolvedType>*,
                              Args...)> callback) {
    return WTF::BindOnce(
        [](ScriptPromiseResolverTyped<IDLResolvedType>* resolver,
           base::OnceCallback<void(ScriptPromiseResolverTyped<IDLResolvedType>*,
                                   Args...)> callback,
           Args... args) {
          ScriptState* script_state = resolver->GetScriptState();
          if (!IsInParallelAlgorithmRunnable(resolver->GetExecutionContext(),
                                             script_state)) {
            return;
          }
          ScriptState::Scope script_state_scope(script_state);
          std::move(callback).Run(resolver, std::move(args)...);
        },
        WrapPersistent(this), std::move(callback));
  }

 private:
  using TypedResolver =
      ScriptPromiseTyped<IDLResolvedType>::InternalResolverTyped;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_PROMISE_RESOLVER_H_

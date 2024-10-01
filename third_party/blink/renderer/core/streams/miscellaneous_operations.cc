// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of functions that are shared between ReadableStream and
// WritableStream.

#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"

#include <math.h>

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

// PromiseRejectInternal() implements Promise.reject(_r_) from the ECMASCRIPT
// standard, https://tc39.github.io/ecma262/#sec-promise.reject.
// The |recursion_depth| argument is used to prevent infinite recursion in the
// case that we can't create a promise.
v8::Local<v8::Promise> PromiseRejectInternal(ScriptState* script_state,
                                             v8::Local<v8::Value> value,
                                             int recursion_depth) {
  auto context = script_state->GetContext();
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::MicrotasksScope microtasks_scope(
      isolate, ToMicrotaskQueue(script_state),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::TryCatch trycatch(isolate);
  // TODO(ricea): Can this fail for reasons other than memory exhaustion? Can we
  // recover if it does?
  auto resolver = v8::Promise::Resolver::New(context).ToLocalChecked();
  if (resolver->Reject(context, value).IsNothing()) {
    // Assume that the exception can be successfully used to create a Promise.
    // TODO(ricea): Can the body of this if statement actually be reached?
    if (recursion_depth >= 2) {
      LOG(FATAL) << "Recursion depth exceeded in PromiseRejectInternal";
    }
    return PromiseRejectInternal(script_state, trycatch.Exception(),
                                 recursion_depth + 1);
  }
  return resolver->GetPromise();
}

class DefaultSizeAlgorithm final : public StrategySizeAlgorithm {
 public:
  std::optional<double> Run(ScriptState*, v8::Local<v8::Value>) override {
    return 1;
  }
};

class JavaScriptSizeAlgorithm final : public StrategySizeAlgorithm {
 public:
  JavaScriptSizeAlgorithm(v8::Isolate* isolate, v8::Local<v8::Function> size)
      : function_(isolate, size) {}

  std::optional<double> Run(ScriptState* script_state,
                            v8::Local<v8::Value> chunk) override {
    auto* isolate = script_state->GetIsolate();
    auto context = script_state->GetContext();
    v8::Local<v8::Value> argv[] = {chunk};

    // https://streams.spec.whatwg.org/#make-size-algorithm-from-size-function
    // 3.a. Return ? Call(size, undefined, « chunk »).
    v8::MaybeLocal<v8::Value> result_maybe =
        function_.Get(isolate)->Call(context, v8::Undefined(isolate), 1, argv);
    v8::Local<v8::Value> result;
    if (!result_maybe.ToLocal(&result)) {
      return std::nullopt;
    }

    // This conversion to double comes from the EnqueueValueWithSize
    // operation: https://streams.spec.whatwg.org/#enqueue-value-with-size
    // 2. Let size be ? ToNumber(size).
    v8::MaybeLocal<v8::Number> number_maybe = result->ToNumber(context);
    v8::Local<v8::Number> number;
    if (!number_maybe.ToLocal(&number)) {
      return std::nullopt;
    }
    return number->Value();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(function_);
    StrategySizeAlgorithm::Trace(visitor);
  }

 private:
  TraceWrapperV8Reference<v8::Function> function_;
};

class TrivialStreamAlgorithm final : public StreamAlgorithm {
 public:
  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    return PromiseResolveWithUndefined(script_state);
  }
};

class JavaScriptStreamAlgorithmWithoutExtraArg final : public StreamAlgorithm {
 public:
  JavaScriptStreamAlgorithmWithoutExtraArg(v8::Isolate* isolate,
                                           v8::Local<v8::Function> method,
                                           v8::Local<v8::Object> recv)
      : recv_(isolate, recv), method_(isolate, method) {}

  // |argc| is equivalent to the "algoArgCount" argument to
  // CreateAlgorithmFromUnderlyingMethod() in the standard, but it is
  // determined when the algorithm is called rather than when the algorithm is
  // created.
  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    // This method technically supports any number of arguments, but we only
    // call it with 0 or 1 in practice.
    DCHECK_GE(argc, 0);
    auto* isolate = script_state->GetIsolate();
    // https://streams.spec.whatwg.org/#create-algorithm-from-underlying-method
    // 6.b.i. Return ! PromiseCall(method, underlyingObject, extraArgs).
    // In this class extraArgs is always empty, but there may be other arguments
    // supplied to the method.
    return PromiseCall(script_state, method_.Get(isolate), recv_.Get(isolate),
                       argc, argv);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(recv_);
    visitor->Trace(method_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  TraceWrapperV8Reference<v8::Object> recv_;
  TraceWrapperV8Reference<v8::Function> method_;
};

class JavaScriptStreamAlgorithmWithExtraArg final : public StreamAlgorithm {
 public:
  JavaScriptStreamAlgorithmWithExtraArg(v8::Isolate* isolate,
                                        v8::Local<v8::Function> method,
                                        v8::Local<v8::Value> extra_arg,
                                        v8::Local<v8::Object> recv)
      : recv_(isolate, recv),
        method_(isolate, method),
        extra_arg_(isolate, extra_arg) {}

  // |argc| is equivalent to the "algoArgCount" argument to
  // CreateAlgorithmFromUnderlyingMethod() in the standard,
  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_GE(argc, 0);
    DCHECK_LE(argc, 1);
    auto* isolate = script_state->GetIsolate();
    // https://streams.spec.whatwg.org/#create-algorithm-from-underlying-method
    // 6.c.
    //      i. Let fullArgs be a List consisting of arg followed by the
    //         elements of extraArgs in order.
    std::array<v8::Local<v8::Value>, 2> full_argv;
    if (argc != 0) {
      full_argv[0] = argv[0];
    }
    full_argv[argc] = extra_arg_.Get(isolate);
    int full_argc = argc + 1;

    //     ii. Return ! PromiseCall(method, underlyingObject, fullArgs).
    return PromiseCall(script_state, method_.Get(isolate), recv_.Get(isolate),
                       full_argc, full_argv.data());
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(recv_);
    visitor->Trace(method_);
    visitor->Trace(extra_arg_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  TraceWrapperV8Reference<v8::Object> recv_;
  TraceWrapperV8Reference<v8::Function> method_;
  TraceWrapperV8Reference<v8::Value> extra_arg_;
};

class JavaScriptByteStreamStartAlgorithm : public StreamStartAlgorithm {
 public:
  JavaScriptByteStreamStartAlgorithm(v8::Isolate* isolate,
                                     v8::Local<v8::Function> method,
                                     v8::Local<v8::Object> recv,
                                     v8::Local<v8::Value> controller)
      : recv_(isolate, recv),
        method_(isolate, method),
        controller_(isolate, controller) {}

  v8::MaybeLocal<v8::Promise> Run(ScriptState* script_state,
                                  ExceptionState& exception_state) override {
    auto* isolate = script_state->GetIsolate();

    auto value_maybe =
        Call1(script_state, method_.Get(isolate), recv_.Get(isolate),
              controller_.Get(isolate), exception_state);
    if (exception_state.HadException()) {
      return v8::MaybeLocal<v8::Promise>();
    }

    v8::Local<v8::Value> value;
    if (!value_maybe.ToLocal(&value)) {
      exception_state.ThrowTypeError("internal error");
      return v8::MaybeLocal<v8::Promise>();
    }
    return PromiseResolve(script_state, value);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(recv_);
    visitor->Trace(method_);
    visitor->Trace(controller_);
    StreamStartAlgorithm::Trace(visitor);
  }

 private:
  TraceWrapperV8Reference<v8::Object> recv_;
  TraceWrapperV8Reference<v8::Function> method_;
  TraceWrapperV8Reference<v8::Value> controller_;
};

class JavaScriptStreamStartAlgorithm : public StreamStartAlgorithm {
 public:
  JavaScriptStreamStartAlgorithm(v8::Isolate* isolate,
                                 v8::Local<v8::Object> recv,
                                 const char* method_name_for_error,
                                 v8::Local<v8::Value> controller)
      : recv_(isolate, recv),
        method_name_for_error_(method_name_for_error),
        controller_(isolate, controller) {}

  v8::MaybeLocal<v8::Promise> Run(ScriptState* script_state,
                                  ExceptionState& exception_state) override {
    auto* isolate = script_state->GetIsolate();
    // https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller-from-underlying-sink
    // 3. Let startAlgorithm be the following steps:
    //    a. Return ? InvokeOrNoop(underlyingSink, "start", « controller »).
    auto value_maybe = CallOrNoop1(script_state, recv_.Get(isolate), "start",
                                   method_name_for_error_,
                                   controller_.Get(isolate), exception_state);
    if (exception_state.HadException()) {
      return v8::MaybeLocal<v8::Promise>();
    }
    v8::Local<v8::Value> value;
    if (!value_maybe.ToLocal(&value)) {
      exception_state.ThrowTypeError("internal error");
      return v8::MaybeLocal<v8::Promise>();
    }
    return PromiseResolve(script_state, value);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(recv_);
    visitor->Trace(controller_);
    StreamStartAlgorithm::Trace(visitor);
  }

 private:
  TraceWrapperV8Reference<v8::Object> recv_;
  const char* const method_name_for_error_;
  TraceWrapperV8Reference<v8::Value> controller_;
};

class TrivialStartAlgorithm : public StreamStartAlgorithm {
 public:
  v8::MaybeLocal<v8::Promise> Run(ScriptState* script_state,
                                  ExceptionState&) override {
    return PromiseResolveWithUndefined(script_state);
  }
};

}  // namespace

// TODO(ricea): For optimal performance, method_name should be cached as an
// atomic v8::String. It's not clear who should own the cache.
CORE_EXPORT StreamAlgorithm* CreateAlgorithmFromUnderlyingMethod(
    ScriptState* script_state,
    v8::Local<v8::Object> underlying_object,
    const char* method_name,
    const char* method_name_for_error,
    v8::MaybeLocal<v8::Value> extra_arg,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#create-algorithm-from-underlying-method
  // 5. Let method be ? GetV(underlyingObject, methodName).
  // 6. If method is not undefined,
  //    a. If ! IsCallable(method) is false, throw a TypeError exception.
  v8::MaybeLocal<v8::Value> method_maybe =
      ResolveMethod(script_state, underlying_object, method_name,
                    method_name_for_error, exception_state);
  v8::Local<v8::Value> method;
  if (!method_maybe.ToLocal(&method)) {
    DCHECK(exception_state.HadException());
    return nullptr;
  }

  if (method->IsUndefined()) {
    // 7. Return an algorithm which returns a promise resolved with undefined.
    return MakeGarbageCollected<TrivialStreamAlgorithm>();
  }

  return CreateAlgorithmFromResolvedMethod(script_state, underlying_object,
                                           method, extra_arg);
}

CORE_EXPORT v8::MaybeLocal<v8::Value> ResolveMethod(
    ScriptState* script_state,
    v8::Local<v8::Object> object,
    const char* method_name,
    const char* name_for_error,
    ExceptionState& exception_state) {
  auto* isolate = script_state->GetIsolate();
  TryRethrowScope rethrow_scope(isolate, exception_state);

  // Algorithm steps from CreateAlgorithmFromUnderlyingMethod in the standard.
  // https://streams.spec.whatwg.org/#create-algorithm-from-underlying-method
  // 5. Let method be ? GetV(underlyingObject, methodName).
  auto method_maybe = object->Get(script_state->GetContext(),
                                  V8AtomicString(isolate, method_name));
  v8::Local<v8::Value> method;
  if (!method_maybe.ToLocal(&method)) {
    return v8::MaybeLocal<v8::Value>();
  }

  // 6. If method is not undefined,
  //    a. If ! IsCallable(method) is false, throw a TypeError exception.
  if (!method->IsFunction() && !method->IsUndefined()) {
    exception_state.ThrowTypeError(String(name_for_error) +
                                   " must be a function or undefined");
    return v8::MaybeLocal<v8::Value>();
  }

  return method;
}

CORE_EXPORT StreamAlgorithm* CreateAlgorithmFromResolvedMethod(
    ScriptState* script_state,
    v8::Local<v8::Object> underlying_object,
    v8::Local<v8::Value> method,
    v8::MaybeLocal<v8::Value> extra_arg) {
  DCHECK(method->IsFunction());

  auto* isolate = script_state->GetIsolate();

  // The standard switches on the number of arguments to be passed to the
  // algorithm, but this implementation doesn't care about that. Instead we
  // switch on whether or not there is an extraArg, as that decides whether or
  // not we need to reconstruct the argument list at runtime.
  v8::Local<v8::Value> extra_arg_local;
  if (!extra_arg.ToLocal(&extra_arg_local)) {
    return MakeGarbageCollected<JavaScriptStreamAlgorithmWithoutExtraArg>(
        isolate, method.As<v8::Function>(), underlying_object);
  }

  return MakeGarbageCollected<JavaScriptStreamAlgorithmWithExtraArg>(
      isolate, method.As<v8::Function>(), extra_arg_local, underlying_object);
}

CORE_EXPORT StreamStartAlgorithm* CreateStartAlgorithm(
    ScriptState* script_state,
    v8::Local<v8::Object> underlying_object,
    const char* method_name_for_error,
    v8::Local<v8::Value> controller) {
  return MakeGarbageCollected<JavaScriptStreamStartAlgorithm>(
      script_state->GetIsolate(), underlying_object, method_name_for_error,
      controller);
}

CORE_EXPORT StreamStartAlgorithm* CreateByteStreamStartAlgorithm(
    ScriptState* script_state,
    v8::Local<v8::Object> underlying_object,
    v8::Local<v8::Value> method,
    v8::Local<v8::Value> controller) {
  return MakeGarbageCollected<JavaScriptByteStreamStartAlgorithm>(
      script_state->GetIsolate(), method.As<v8::Function>(), underlying_object,
      controller);
}

CORE_EXPORT StreamStartAlgorithm* CreateTrivialStartAlgorithm() {
  return MakeGarbageCollected<TrivialStartAlgorithm>();
}

CORE_EXPORT StreamAlgorithm* CreateTrivialStreamAlgorithm() {
  return MakeGarbageCollected<TrivialStreamAlgorithm>();
}

CORE_EXPORT ScriptValue CreateTrivialQueuingStrategy(v8::Isolate* isolate,
                                                     size_t high_water_mark) {
  v8::Local<v8::Name> high_water_mark_string =
      V8AtomicString(isolate, "highWaterMark");
  v8::Local<v8::Value> high_water_mark_value =
      v8::Number::New(isolate, high_water_mark);

  auto strategy =
      v8::Object::New(isolate, v8::Null(isolate), &high_water_mark_string,
                      &high_water_mark_value, 1);

  return ScriptValue(isolate, strategy);
}

CORE_EXPORT v8::MaybeLocal<v8::Value> CallOrNoop1(
    ScriptState* script_state,
    v8::Local<v8::Object> object,
    const char* method_name,
    const char* name_for_error,
    v8::Local<v8::Value> arg0,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#invoke-or-noop
  // 4. Let method be ? GetV(O, P).
  v8::MaybeLocal<v8::Value> method_maybe = ResolveMethod(
      script_state, object, method_name, name_for_error, exception_state);
  v8::Local<v8::Value> method;
  if (!method_maybe.ToLocal(&method)) {
    DCHECK(exception_state.HadException());
    return v8::MaybeLocal<v8::Value>();
  }

  // 5. If method is undefined, return undefined.
  if (method->IsUndefined()) {
    return v8::Undefined(script_state->GetIsolate());
  }
  DCHECK(method->IsFunction());

  // 6. Return ? Call(method, O, args).
  TryRethrowScope rethrow_scope(script_state->GetIsolate(), exception_state);
  return method.As<v8::Function>()->Call(script_state->GetContext(), object, 1,
                                         &arg0);
}

CORE_EXPORT v8::MaybeLocal<v8::Value> Call1(ScriptState* script_state,
                                            v8::Local<v8::Function> method,
                                            v8::Local<v8::Object> object,
                                            v8::Local<v8::Value> arg0,
                                            ExceptionState& exception_state) {
  TryRethrowScope rethrow_scope(script_state->GetIsolate(), exception_state);
  return method->Call(script_state->GetContext(), object, 1, &arg0);
}

CORE_EXPORT v8::Local<v8::Promise> PromiseCall(ScriptState* script_state,
                                               v8::Local<v8::Function> method,
                                               v8::Local<v8::Object> recv,
                                               int argc,
                                               v8::Local<v8::Value> argv[]) {
  DCHECK_GE(argc, 0);
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch trycatch(isolate);
  v8::MicrotasksScope microtasks_scope(
      isolate, ToMicrotaskQueue(script_state),
      v8::MicrotasksScope::kDoNotRunMicrotasks);

  // https://streams.spec.whatwg.org/#promise-call
  // 4. Let returnValue be Call(F, V, args).
  v8::MaybeLocal<v8::Value> result_maybe =
      method->Call(script_state->GetContext(), recv, argc, argv);

  v8::Local<v8::Value> result;
  // 5. If returnValue is an abrupt completion, return a promise rejected with
  //    returnValue.[[Value]].
  if (!result_maybe.ToLocal(&result)) {
    return PromiseReject(script_state, trycatch.Exception());
  }

  // 6. Otherwise, return a promise resolved with returnValue.[[Value]].
  return PromiseResolve(script_state, result);
}

CORE_EXPORT double ValidateAndNormalizeHighWaterMark(
    double high_water_mark,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#validate-and-normalize-high-water-mark
  // 2. If highWaterMark is NaN or highWaterMark < 0, throw a RangeError
  //    exception.
  if (isnan(high_water_mark) || high_water_mark < 0) {
    exception_state.ThrowRangeError(
        "A queuing strategy's highWaterMark property must be a nonnegative, "
        "non-NaN number");
    return 0;
  }

  // 3. Return highWaterMark.
  return high_water_mark;
}

CORE_EXPORT StrategySizeAlgorithm* MakeSizeAlgorithmFromSizeFunction(
    ScriptState* script_state,
    v8::Local<v8::Value> size,
    ExceptionState& exception_state) {
  // 1. If size is undefined, return an algorithm that returns 1.
  if (size->IsUndefined()) {
    return MakeGarbageCollected<DefaultSizeAlgorithm>();
  }

  // 2. If ! IsCallable(size) is false, throw a TypeError exception.
  if (!size->IsFunction()) {
    exception_state.ThrowTypeError(
        "A queuing strategy's size property must be a function");
    return nullptr;
  }

  // 3. Return an algorithm that performs the following steps, taking a chunk
  // argument:
  //    a. Return ? Call(size, undefined, « chunk »).
  return MakeGarbageCollected<JavaScriptSizeAlgorithm>(
      script_state->GetIsolate(), size.As<v8::Function>());
}

CORE_EXPORT StrategySizeAlgorithm* CreateDefaultSizeAlgorithm() {
  return MakeGarbageCollected<DefaultSizeAlgorithm>();
}

// PromiseResolve implements Promise.resolve(_x_) from the ECMASCRIPT standard,
// https://tc39.github.io/ecma262/#sec-promise.resolve, except that the
// Get(_x_, "constructor") step is skipped.
CORE_EXPORT v8::Local<v8::Promise> PromiseResolve(ScriptState* script_state,
                                                  v8::Local<v8::Value> value) {
  if (value->IsPromise()) {
    return value.As<v8::Promise>();
  }
  auto context = script_state->GetContext();
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::MicrotasksScope microtasks_scope(
      isolate, ToMicrotaskQueue(script_state),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::TryCatch trycatch(isolate);
  // TODO(ricea): Can this fail for reasons other than memory exhaustion? Can we
  // recover if it does?
  auto resolver = v8::Promise::Resolver::New(context).ToLocalChecked();
  if (resolver->Resolve(context, value).IsNothing()) {
    // TODO(ricea): Is this actually reachable?
    return PromiseReject(script_state, trycatch.Exception());
  }
  return resolver->GetPromise();
}

CORE_EXPORT v8::Local<v8::Promise> PromiseResolveWithUndefined(
    ScriptState* script_state) {
  return PromiseResolve(script_state,
                        v8::Undefined(script_state->GetIsolate()));
}

CORE_EXPORT v8::Local<v8::Promise> PromiseReject(ScriptState* script_state,
                                                 v8::Local<v8::Value> value) {
  return PromiseRejectInternal(script_state, value, 0);
}

void ScriptValueToObject(ScriptState* script_state,
                         ScriptValue value,
                         v8::Local<v8::Object>* object,
                         ExceptionState& exception_state) {
  auto* isolate = script_state->GetIsolate();
  DCHECK(!value.IsEmpty());
  auto v8_value = value.V8Value();
  // All the object parameters in the standard are default-initialised to an
  // empty object.
  if (v8_value->IsUndefined()) {
    *object = v8::Object::New(isolate);
    return;
  }
  TryRethrowScope rethrow_scope(isolate, exception_state);
  std::ignore = v8_value->ToObject(script_state->GetContext()).ToLocal(object);
}

StrategyUnpacker::StrategyUnpacker(ScriptState* script_state,
                                   ScriptValue strategy,
                                   ExceptionState& exception_state) {
  auto* isolate = script_state->GetIsolate();
  auto context = script_state->GetContext();
  v8::Local<v8::Object> strategy_object;
  ScriptValueToObject(script_state, strategy, &strategy_object,
                      exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // This is used in several places. The steps here are taken from
  // https://streams.spec.whatwg.org/#ws-constructor.
  // 2. Let size be ? GetV(strategy, "size").
  TryRethrowScope rethrow_scope(isolate, exception_state);
  if (!strategy_object->Get(context, V8AtomicString(isolate, "size"))
           .ToLocal(&size_)) {
    return;
  }

  // 3. Let highWaterMark be ? GetV(strategy, "highWaterMark").
  if (!strategy_object->Get(context, V8AtomicString(isolate, "highWaterMark"))
           .ToLocal(&high_water_mark_)) {
    return;
  }
}

StrategySizeAlgorithm* StrategyUnpacker::MakeSizeAlgorithm(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  DCHECK(!size_.IsEmpty());
  // 6. Let sizeAlgorithm be ? MakeSizeAlgorithmFromSizeFunction(size).
  return MakeSizeAlgorithmFromSizeFunction(script_state, size_,
                                           exception_state);
}

double StrategyUnpacker::GetHighWaterMark(
    ScriptState* script_state,
    int default_value,
    ExceptionState& exception_state) const {
  DCHECK(!high_water_mark_.IsEmpty());
  // 7. If highWaterMark is undefined, let highWaterMark be 1.
  if (high_water_mark_->IsUndefined()) {
    return default_value;
  }

  TryRethrowScope rethrow_scope(script_state->GetIsolate(), exception_state);
  v8::Local<v8::Number> high_water_mark_as_number;
  if (!high_water_mark_->ToNumber(script_state->GetContext())
           .ToLocal(&high_water_mark_as_number)) {
    return 0.0;
  }

  // 8. Set highWaterMark to ? ValidateAndNormalizeHighWaterMark(highWaterMark)
  return ValidateAndNormalizeHighWaterMark(high_water_mark_as_number->Value(),
                                           exception_state);
}

bool StrategyUnpacker::IsSizeUndefined() const {
  return size_->IsUndefined();
}

}  // namespace blink

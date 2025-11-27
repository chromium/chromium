// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/transform_stream_default_controller.h"

#include "base/containers/span.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/core/streams/transform_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

TransformStreamDefaultController::TransformStreamDefaultController() = default;
TransformStreamDefaultController::~TransformStreamDefaultController() = default;

ReadableStreamDefaultController*
TransformStreamDefaultController::GetDefaultController(
    TransformStream* stream) {
  // The TransformStreamDefaultController will always use a
  // ReadableStreamDefaultController. Hence, it is safe to down-cast here.
  return To<ReadableStreamDefaultController>(
      stream->readable_->GetController());
}

std::optional<double> TransformStreamDefaultController::desiredSize() const {
  // https://streams.spec.whatwg.org/#ts-default-controller-desired-size
  // 2. Let readableController be
  //    this.[[controlledTransformStream]].[[readable]].
  //    [[readableStreamController]].
  const auto* readable_controller =
      GetDefaultController(controlled_transform_stream_);

  // 3. Return !
  //    ReadableStreamDefaultControllerGetDesiredSize(readableController).
  // Use the accessor instead as it already has the semantics we need and can't
  // be interfered with from JavaScript.
  return readable_controller->desiredSize();
}

// The handling of undefined arguments is implicit in the standard, but needs to
// be done explicitly with IDL.
void TransformStreamDefaultController::enqueue(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#ts-default-controller-enqueue
  // 2. Perform ? TransformStreamDefaultControllerEnqueue(this, chunk).
  Enqueue(script_state, this, v8::Undefined(script_state->GetIsolate()),
          exception_state);
}

void TransformStreamDefaultController::enqueue(
    ScriptState* script_state,
    ScriptValue chunk,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#ts-default-controller-enqueue
  // 2. Perform ? TransformStreamDefaultControllerEnqueue(this, chunk).
  Enqueue(script_state, this, chunk.V8Value(), exception_state);
}

void TransformStreamDefaultController::error(ScriptState* script_state) {
  // https://streams.spec.whatwg.org/#ts-default-controller-error
  // 2. Perform ! TransformStreamDefaultControllerError(this, reason).
  Error(script_state, this, v8::Undefined(script_state->GetIsolate()));
}

void TransformStreamDefaultController::error(ScriptState* script_state,
                                             ScriptValue reason) {
  // https://streams.spec.whatwg.org/#ts-default-controller-error
  // 2. Perform ! TransformStreamDefaultControllerError(this, reason).
  Error(script_state, this, reason.V8Value());
}

void TransformStreamDefaultController::terminate(ScriptState* script_state) {
  // https://streams.spec.whatwg.org/#ts-default-controller-terminate
  // 2. Perform ! TransformStreamDefaultControllerTerminate(this).
  Terminate(script_state, this);
}

void TransformStreamDefaultController::Trace(Visitor* visitor) const {
  visitor->Trace(controlled_transform_stream_);
  visitor->Trace(flush_algorithm_);
  visitor->Trace(transform_algorithm_);
  visitor->Trace(reject_function_);
  ScriptWrappable::Trace(visitor);
}

// This algorithm is not explicitly named, but is described as part of the
// SetUpTransformStreamDefaultControllerFromTransformer abstract operation in
// the standard.
class TransformStreamDefaultController::DefaultTransformAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit DefaultTransformAlgorithm(
      TransformStreamDefaultController* controller)
      : controller_(controller) {}

  ScriptPromise<IDLUndefined> Run(
      ScriptState* script_state,
      base::span<v8::Local<v8::Value>> argv) override {
    DCHECK_EQ(argv.size(), 1u);
    v8::Isolate* isolate = script_state->GetIsolate();
    v8::TryCatch try_catch(isolate);

    // https://streams.spec.whatwg.org/#set-up-transform-stream-default-controller-from-transformer
    // 3. Let transformAlgorithm be the following steps, taking a chunk
    //    argument:
    //    a. Let result be TransformStreamDefaultControllerEnqueue(controller,
    //       chunk).
    Enqueue(script_state, controller_, argv[0], PassThroughException(isolate));

    //    b. If result is an abrupt completion, return a promise rejected with
    //       result.[[Value]].
    if (try_catch.HasCaught()) {
      return ScriptPromise<IDLUndefined>::Reject(script_state,
                                                 try_catch.Exception());
    }

    //    c. Otherwise, return a promise resolved with undefined.
    return ToResolvedUndefinedPromise(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(controller_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  Member<TransformStreamDefaultController> controller_;
};

class TransformStreamDefaultController::PerformTransformRejectFunction final
    : public ThenCallable<IDLAny, PerformTransformRejectFunction> {
 public:
  explicit PerformTransformRejectFunction(TransformStream* stream)
      : stream_(stream) {}

  void React(ScriptState* script_state, ScriptValue r) {
    // 2. Return the result of transforming transformPromise with a rejection
    //    handler that, when called with argument r, performs the following
    //    steps:
    //    a. Perform ! TransformStreamError(controller.
    //       [[controlledTransformStream]], r).
    TransformStream::Error(script_state, stream_, r.V8Value());

    //    b. Throw r.
    V8ThrowException::ThrowException(script_state->GetIsolate(), r.V8Value());
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(stream_);
    ThenCallable<IDLAny, PerformTransformRejectFunction>::Trace(visitor);
  }

 private:
  Member<TransformStream> stream_;
};

void TransformStreamDefaultController::SetUp(
    ScriptState* script_state,
    TransformStream* stream,
    TransformStreamDefaultController* controller,
    StreamAlgorithm* transform_algorithm,
    StreamAlgorithm* flush_algorithm) {
  // https://streams.spec.whatwg.org/#set-up-transform-stream-default-controller
  // 1. Assert: ! IsTransformStream(stream) is true.
  DCHECK(stream);

  // 2. Assert: stream.[[transformStreamController]] is undefined.
  DCHECK(!stream->transform_stream_controller_);

  // 3. Set controller.[[controlledTransformStream]] to stream.
  controller->controlled_transform_stream_ = stream;

  // 4. Set stream.[[transformStreamController]] to controller.
  stream->transform_stream_controller_ = controller;

  // 5. Set controller.[[transformAlgorithm]] to transformAlgorithm.
  controller->transform_algorithm_ = transform_algorithm;

  // 6. Set controller.[[flushAlgorithm]] to flushAlgorithm.
  controller->flush_algorithm_ = flush_algorithm;

  controller->reject_function_ =
      MakeGarbageCollected<PerformTransformRejectFunction>(
          controller->controlled_transform_stream_);
}

v8::Local<v8::Value> TransformStreamDefaultController::SetUpFromTransformer(
    ScriptState* script_state,
    TransformStream* stream,
    v8::Local<v8::Object> transformer,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#set-up-transform-stream-default-controller-from-transformer
  // 1. Assert: transformer is not undefined.
  DCHECK(!transformer->IsUndefined());

  // 2. Let controller be ObjectCreate(the original value of
  //    TransformStreamDefaultController's prototype property).
  auto* controller = MakeGarbageCollected<TransformStreamDefaultController>();

  // This method is only called when a TransformStream is being constructed by
  // JavaScript. So the execution context should be valid and this call should
  // not crash.
  auto controller_value = ToV8Traits<TransformStreamDefaultController>::ToV8(
      script_state, controller);

  // The following steps are reordered from the standard for efficiency, but the
  // effect is the same.
  StreamAlgorithm* transform_algorithm = nullptr;

  // 4. Let transformMethod be ? GetV(transformer, "transform").
  v8::MaybeLocal<v8::Value> method_maybe =
      ResolveMethod(script_state, transformer, "transform",
                    "transformer.transform", exception_state);
  v8::Local<v8::Value> transform_method;
  if (!method_maybe.ToLocal(&transform_method)) {
    CHECK(exception_state.HadException());
    return v8::Local<v8::Value>();
  }
  DCHECK(!exception_state.HadException());

  if (transform_method->IsUndefined()) {
    // 3. Let transformAlgorithm be the following steps, taking a chunk
    // argument:
    //    i. Let result be TransformStreamDefaultControllerEnqueue(controller,
    //       chunk).
    //   ii. If result is an abrupt completion, return a promise rejected with
    //       result.[[Value]].
    //  iii. Otherwise, return a promise resolved with undefined.
    transform_algorithm =
        MakeGarbageCollected<DefaultTransformAlgorithm>(controller);
  } else {
    // 5. If transformMethod is not undefined,
    //    a. If ! IsCallable(transformMethod) is false, throw a TypeError
    //       exception.
    // (The IsCallable() check has already been done by ResolveMethod).

    //    b. Set transformAlgorithm to the following steps, taking a chunk
    //       argument:
    //       i. Return ! PromiseCall(transformMethod, transformer, « chunk,
    //          controller »).
    transform_algorithm = CreateAlgorithmFromResolvedMethod(
        script_state, transformer, transform_method, controller_value);
  }

  // 6. Let flushAlgorithm be ? CreateAlgorithmFromUnderlyingMethod(transformer,
  //    "flush", 0, « controller »).
  auto* flush_algorithm = CreateAlgorithmFromUnderlyingMethod(
      script_state, transformer, "flush", "transformer.flush", controller_value,
      exception_state);

  // 7. Perform ! SetUpTransformStreamDefaultController(stream, controller,
  //    transformAlgorithm, flushAlgorithm).
  SetUp(script_state, stream, controller, transform_algorithm, flush_algorithm);

  // This operation doesn't have a return value in the standard, but it's useful
  // to return the JavaScript wrapper here so that it can be used when calling
  // transformer.start().
  return controller_value;
}

void TransformStreamDefaultController::ClearAlgorithms(
    TransformStreamDefaultController* controller) {
  // https://streams.spec.whatwg.org/#transform-stream-default-controller-clear-algorithms
  // 1. Set controller.[[transformAlgorithm]] to undefined.
  controller->transform_algorithm_ = nullptr;

  // 2. Set controller.[[flushAlgorithm]] to undefined.
  controller->flush_algorithm_ = nullptr;
}

void TransformStreamDefaultController::Enqueue(
    ScriptState* script_state,
    TransformStreamDefaultController* controller,
    v8::Local<v8::Value> chunk,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#transform-stream-default-controller-enqueue
  // 1. Let stream be controller.[[controlledTransformStream]].
  TransformStream* stream = controller->controlled_transform_stream_;

  // 2. Let readableController be
  //    stream.[[readable]].[[readableStreamController]].
  auto* readable_controller = GetDefaultController(stream);

  // 3. If !
  //    ReadableStreamDefaultControllerCanCloseOrEnqueue(readableController) is
  //    false, throw a TypeError exception.
  if (!ReadableStreamDefaultController::CanCloseOrEnqueue(
          readable_controller)) {
    exception_state.ThrowTypeError(
        ReadableStreamDefaultController::EnqueueExceptionMessage(
            readable_controller));
    return;
  }

  // 4. Let enqueueResult be ReadableStreamDefaultControllerEnqueue(
  //    readableController, chunk).
  v8::Isolate* isolate = script_state->GetIsolate();
  TryRethrowScope rethrow_scope(isolate, exception_state);
  ReadableStreamDefaultController::Enqueue(
      script_state, readable_controller, chunk, PassThroughException(isolate));

  // 5. If enqueueResult is an abrupt completion,
  if (rethrow_scope.HasCaught()) {
    // a. Perform ! TransformStreamErrorWritableAndUnblockWrite(stream,
    //    enqueueResult.[[Value]]).
    TransformStream::ErrorWritableAndUnblockWrite(
        script_state, stream, rethrow_scope.TakeException());

    // b. Throw stream.[[readable]].[[storedError]].
    V8ThrowException::ThrowException(
        isolate, stream->readable_->GetStoredError(isolate));
    return;
  }

  // 6. Let backpressure be ! ReadableStreamDefaultControllerHasBackpressure(
  //    readableController).
  bool backpressure =
      ReadableStreamDefaultController::HasBackpressure(readable_controller);

  // 7. If backpressure is not stream.[[backpressure]],
  if (backpressure != stream->had_backpressure_) {
    // a. Assert: backpressure is true.
    DCHECK(backpressure);

    // b. Perform ! TransformStreamSetBackpressure(stream, true).
    TransformStream::SetBackpressure(script_state, stream, true);
  }
}

void TransformStreamDefaultController::Error(
    ScriptState* script_state,
    TransformStreamDefaultController* controller,
    v8::Local<v8::Value> e) {
  // https://streams.spec.whatwg.org/#transform-stream-default-controller-error
  // 1. Perform ! TransformStreamError(controller.[[controlledTransformStream]],
  //    e).
  TransformStream::Error(script_state, controller->controlled_transform_stream_,
                         e);
}

ScriptPromise<IDLUndefined> TransformStreamDefaultController::PerformTransform(
    ScriptState* script_state,
    TransformStreamDefaultController* controller,
    v8::Local<v8::Value> chunk) {
  if (!script_state->ContextIsValid()) {
    v8::Local<v8::Value> error = V8ThrowException::CreateTypeError(
        script_state->GetIsolate(), "invalid realm");
    Error(script_state, controller, error);
    return EmptyPromise();
  }
  // https://streams.spec.whatwg.org/#transform-stream-default-controller-perform-transform
  // 1. Let transformPromise be the result of performing controller.
  //    [[transformAlgorithm]], passing chunk.
  // This is needed because the result of transforming the transform promise
  // needs to be returned to the outer scope.
  ScriptState::EscapableScope scope(script_state);
  ScriptPromise<IDLUndefined> transform_promise =
      controller->transform_algorithm_->Run(script_state,
                                            base::span_from_ref(chunk));
  DCHECK(!transform_promise.IsEmpty());

  // 2. Return the result of transforming transformPromise ...
  v8::Local<v8::Value> escapable_streamed_promise = scope.Escape(
      transform_promise.Catch(script_state, controller->reject_function_.Get())
          .V8Promise());
  return ScriptPromise<IDLUndefined>::FromV8Value(script_state,
                                                  escapable_streamed_promise);
}

void TransformStreamDefaultController::Terminate(
    ScriptState* script_state,
    TransformStreamDefaultController* controller) {
  // https://streams.spec.whatwg.org/#transform-stream-default-controller-terminate
  // 1. Let stream be controller.[[controlledTransformStream]].
  TransformStream* stream = controller->controlled_transform_stream_;

  // 2. Let readableController be
  //    stream.[[readable]].[[readableStreamController]].
  ReadableStreamDefaultController* readable_controller =
      GetDefaultController(stream);

  // 3. If !
  //    ReadableStreamDefaultControllerCanCloseOrEnqueue(readableController) is
  //    true, perform ! ReadableStreamDefaultControllerClose(
  //    readableController).
  if (ReadableStreamDefaultController::CanCloseOrEnqueue(readable_controller)) {
    ReadableStreamDefaultController::Close(script_state, readable_controller);
  }

  // 4. Let error be a TypeError exception indicating that the stream has been
  //    terminated.
  const auto error = v8::Exception::TypeError(V8String(
      script_state->GetIsolate(), "The transform stream has been terminated"));

  // 5. Perform ! TransformStreamErrorWritableAndUnblockWrite(stream, error).
  TransformStream::ErrorWritableAndUnblockWrite(script_state, stream, error);
}

}  // namespace blink

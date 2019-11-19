// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/transform_stream_default_controller.h"

#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/promise_handler.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/readable_stream_native.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/core/streams/transform_stream_native.h"
#include "third_party/blink/renderer/core/streams/writable_stream_native.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/to_v8.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

TransformStreamDefaultController::TransformStreamDefaultController() = default;
TransformStreamDefaultController::~TransformStreamDefaultController() = default;

double TransformStreamDefaultController::desiredSize(bool& is_null) const {
  // https://streams.spec.whatwg.org/#ts-default-controller-desired-size
  // 2. Let readableController be
  //    this.[[controlledTransformStream]].[[readable]].
  //    [[readableStreamController]].
  const auto* readable_controller =
      controlled_transform_stream_->readable_->GetController();

  // 3. Return !
  //    ReadableStreamDefaultControllerGetDesiredSize(readableController).
  // Use the accessor instead as it already has the semantics we need and can't
  // be interfered with from JavaScript.
  return readable_controller->desiredSize(is_null);
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

void TransformStreamDefaultController::Trace(Visitor* visitor) {
  visitor->Trace(controlled_transform_stream_);
  visitor->Trace(flush_algorithm_);
  visitor->Trace(transform_algorithm_);
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

  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 1);
    ExceptionState exception_state(script_state->GetIsolate(),
                                   ExceptionState::kUnknownContext, "", "");

    // https://streams.spec.whatwg.org/#set-up-transform-stream-default-controller-from-transformer
    // 3. Let transformAlgorithm be the following steps, taking a chunk
    //    argument:
    //    a. Let result be TransformStreamDefaultControllerEnqueue(controller,
    //       chunk).
    Enqueue(script_state, controller_, argv[0], exception_state);

    //    b. If result is an abrupt completion, return a promise rejected with
    //       result.[[Value]].
    if (exception_state.HadException()) {
      v8::Local<v8::Value> exception = exception_state.GetException();
      exception_state.ClearException();
      return PromiseReject(script_state, exception);
    }

    //    c. Otherwise, return a promise resolved with undefined.
    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) override {
    visitor->Trace(controller_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  Member<TransformStreamDefaultController> controller_;
};

void TransformStreamDefaultController::SetUp(
    TransformStreamNative* stream,
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
}

v8::Local<v8::Value> TransformStreamDefaultController::SetUpFromTransformer(
    ScriptState* script_state,
    TransformStreamNative* stream,
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
  auto controller_value = ToV8(controller, script_state);

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
  SetUp(stream, controller, transform_algorithm, flush_algorithm);

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
  TransformStreamNative* stream = controller->controlled_transform_stream_;

  // 2. Let readableController be
  //    stream.[[readable]].[[readableStreamController]].
  auto* readable_controller = stream->readable_->GetController();

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
  ReadableStreamDefaultController::Enqueue(script_state, readable_controller,
                                           chunk, exception_state);

  // 5. If enqueueResult is an abrupt completion,
  if (exception_state.HadException()) {
    // a. Perform ! TransformStreamErrorWritableAndUnblockWrite(stream,
    //    enqueueResult.[[Value]]).
    TransformStreamNative::ErrorWritableAndUnblockWrite(
        script_state, stream, exception_state.GetException());
    exception_state.ClearException();

    // b. Throw stream.[[readable]].[[storedError]].
    exception_state.RethrowV8Exception(
        stream->readable_->GetStoredError(script_state->GetIsolate()));
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
    TransformStreamNative::SetBackpressure(script_state, stream, true);
  }
}

void TransformStreamDefaultController::Error(
    ScriptState* script_state,
    TransformStreamDefaultController* controller,
    v8::Local<v8::Value> e) {
  // https://streams.spec.whatwg.org/#transform-stream-default-controller-error
  // 1. Perform ! TransformStreamError(controller.[[controlledTransformStream]],
  //    e).
  TransformStreamNative::Error(script_state,
                               controller->controlled_transform_stream_, e);
}

v8::Local<v8::Promise> TransformStreamDefaultController::PerformTransform(
    ScriptState* script_state,
    TransformStreamDefaultController* controller,
    v8::Local<v8::Value> chunk) {
  // https://streams.spec.whatwg.org/#transform-stream-default-controller-perform-transform
  // 1. Let transformPromise be the result of performing controller.
  //    [[transformAlgorithm]], passing chunk.
  auto transform_promise =
      controller->transform_algorithm_->Run(script_state, 1, &chunk);

  class RejectFunction final : public PromiseHandlerWithValue {
   public:
    RejectFunction(ScriptState* script_state, TransformStreamNative* stream)
        : PromiseHandlerWithValue(script_state), stream_(stream) {}

    v8::Local<v8::Value> CallWithLocal(v8::Local<v8::Value> r) override {
      // 2. Return the result of transforming transformPromise with a rejection
      //    handler that, when called with argument r, performs the following
      //    steps:
      //    a. Perform ! TransformStreamError(controller.
      //       [[controlledTransformStream]], r).
      TransformStreamNative::Error(GetScriptState(), stream_, r);

      //    b. Throw r.
      return PromiseReject(GetScriptState(), r);
    }

    void Trace(Visitor* visitor) override {
      visitor->Trace(stream_);
      PromiseHandlerWithValue::Trace(visitor);
    }

   private:
    Member<TransformStreamNative> stream_;
  };

  // 2. Return the result of transforming transformPromise ...
  return StreamThenPromise(
      script_state->GetContext(), transform_promise, nullptr,
      MakeGarbageCollected<RejectFunction>(
          script_state, controller->controlled_transform_stream_));
}

void TransformStreamDefaultController::Terminate(
    ScriptState* script_state,
    TransformStreamDefaultController* controller) {
  // https://streams.spec.whatwg.org/#transform-stream-default-controller-terminate
  // 1. Let stream be controller.[[controlledTransformStream]].
  TransformStreamNative* stream = controller->controlled_transform_stream_;

  // 2. Let readableController be
  //    stream.[[readable]].[[readableStreamController]].
  ReadableStreamDefaultController* readable_controller =
      stream->readable_->GetController();

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
  TransformStreamNative::ErrorWritableAndUnblockWrite(script_state, stream,
                                                      error);
}

}  // namespace blink

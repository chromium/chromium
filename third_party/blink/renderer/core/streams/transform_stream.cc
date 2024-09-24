// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/transform_stream.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/promise_handler.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/core/streams/transform_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/transform_stream_transformer.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Implements a flush algorithm that delegates to a TransformStreamTransformer.
// This is used when creating a TransformStream from C++.
class TransformStream::FlushAlgorithm final : public StreamAlgorithm {
 public:
  explicit FlushAlgorithm(TransformStreamTransformer* transformer)
      : transformer_(transformer) {}

  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 0);
    DCHECK(controller_);
    v8::Isolate* isolate = script_state->GetIsolate();
    auto* transformer_script_state = transformer_->GetScriptState();
    if (!transformer_script_state->ContextIsValid()) {
      return PromiseReject(script_state, V8ThrowException::CreateTypeError(
                                             isolate, "invalid realm"));
    }
    v8::TryCatch try_catch(isolate);
    ScriptPromiseUntyped promise;
    {
      // This is needed because the realm of the transformer can be different
      // from the realm of the transform stream.
      ScriptState::Scope scope(transformer_script_state);
      promise = transformer_->Flush(controller_, PassThroughException(isolate));
    }
    if (try_catch.HasCaught()) {
      return PromiseReject(script_state, try_catch.Exception());
    }

    return promise.V8Promise();
  }

  // SetController() must be called before Run() is.
  void SetController(TransformStreamDefaultController* controller) {
    controller_ = controller;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(transformer_);
    visitor->Trace(controller_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  Member<TransformStreamTransformer> transformer_;
  Member<TransformStreamDefaultController> controller_;
};

// Implements a transform algorithm that delegates to a
// TransformStreamTransformer.
class TransformStream::TransformAlgorithm final : public StreamAlgorithm {
 public:
  explicit TransformAlgorithm(TransformStreamTransformer* transformer)
      : transformer_(transformer) {}

  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 1);
    DCHECK(controller_);
    v8::Isolate* isolate = script_state->GetIsolate();
    auto* transformer_script_state = transformer_->GetScriptState();
    if (!transformer_script_state->ContextIsValid()) {
      return PromiseReject(script_state, V8ThrowException::CreateTypeError(
                                             isolate, "invalid realm"));
    }
    v8::TryCatch try_catch(isolate);
    auto promise = transformer_->Transform(argv[0], controller_,
                                           PassThroughException(isolate));
    if (try_catch.HasCaught()) {
      return PromiseReject(script_state, try_catch.Exception());
    }

    return promise.V8Promise();
  }

  // SetController() must be called before Run() is.
  void SetController(TransformStreamDefaultController* controller) {
    controller_ = controller;
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(transformer_);
    visitor->Trace(controller_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  Member<TransformStreamTransformer> transformer_;
  Member<TransformStreamDefaultController> controller_;
};

TransformStream* TransformStream::Create(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  ScriptValue undefined(script_state->GetIsolate(),
                        v8::Undefined(script_state->GetIsolate()));
  return Create(script_state, undefined, undefined, undefined, exception_state);
}

TransformStream* TransformStream::Create(
    ScriptState* script_state,
    ScriptValue transform_stream_transformer,
    ExceptionState& exception_state) {
  ScriptValue undefined(script_state->GetIsolate(),
                        v8::Undefined(script_state->GetIsolate()));
  return Create(script_state, transform_stream_transformer, undefined,
                undefined, exception_state);
}

TransformStream* TransformStream::Create(
    ScriptState* script_state,
    ScriptValue transform_stream_transformer,
    ScriptValue writable_strategy,
    ExceptionState& exception_state) {
  ScriptValue undefined(script_state->GetIsolate(),
                        v8::Undefined(script_state->GetIsolate()));
  return Create(script_state, transform_stream_transformer, writable_strategy,
                undefined, exception_state);
}

TransformStream* TransformStream::Create(ScriptState* script_state,
                                         ScriptValue transformer,
                                         ScriptValue writable_strategy,
                                         ScriptValue readable_strategy,
                                         ExceptionState& exception_state) {
  auto* ts = MakeGarbageCollected<TransformStream>();

  ts->InitInternal(script_state, transformer, writable_strategy,
                   readable_strategy, exception_state);

  if (exception_state.HadException()) {
    return nullptr;
  }

  return ts;
}

// static
TransformStream* TransformStream::Create(
    ScriptState* script_state,
    TransformStreamTransformer* transformer,
    ExceptionState& exception_state) {
  auto* transform_algorithm =
      MakeGarbageCollected<TransformAlgorithm>(transformer);
  auto* flush_algorithm = MakeGarbageCollected<FlushAlgorithm>(transformer);
  auto* size_algorithm = CreateDefaultSizeAlgorithm();
  auto* stream = Create(script_state, CreateTrivialStartAlgorithm(),
                        transform_algorithm, flush_algorithm, 1, size_algorithm,
                        0, size_algorithm, exception_state);
  DCHECK(stream);
  DCHECK(!exception_state.HadException());
  TransformStreamDefaultController* controller =
      stream->transform_stream_controller_;
  transform_algorithm->SetController(controller);
  flush_algorithm->SetController(controller);
  return stream;
}

TransformStream* TransformStream::Create(
    ScriptState* script_state,
    StreamStartAlgorithm* start_algorithm,
    StreamAlgorithm* transform_algorithm,
    StreamAlgorithm* flush_algorithm,
    double writable_high_water_mark,
    StrategySizeAlgorithm* writable_size_algorithm,
    double readable_high_water_mark,
    StrategySizeAlgorithm* readable_size_algorithm,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#create-transform-stream
  //
  // In this implementation, all the arguments are compulsory, so the following
  // steps aren't performed:
  // 1. If writableHighWaterMark was not passed, set it to 1.
  // 2. If writableSizeAlgorithm was not passed, set it to an algorithm that
  //    returns 1.
  // 3. If readableHighWaterMark was not passed, set it to 0.
  // 4. If readableSizeAlgorithm was not passed, set it to an algorithm that
  //    returns 1.

  // 5. Assert: ! IsNonNegativeNumber(writableHighWaterMark) is true.
  DCHECK_GE(writable_high_water_mark, 0);

  // 6. Assert: ! IsNonNegativeNumber(readableHighWaterMark) is true.
  DCHECK_GE(readable_high_water_mark, 0);

  // 7. Let stream be ObjectCreate(the original value of TransformStream's
  //    prototype property).
  auto* stream = MakeGarbageCollected<TransformStream>();

  // 8. Let startPromise be a new promise.
  auto* start_promise =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);

  // 9. Perform ! InitializeTransformStream(stream, startPromise,
  //    writableHighWaterMark, writableSizeAlgorithm, readableHighWaterMark,
  //    readableSizeAlgorithm).
  Initialize(script_state, stream, start_promise, writable_high_water_mark,
             writable_size_algorithm, readable_high_water_mark,
             readable_size_algorithm, exception_state);

  // 10. Let controller be ObjectCreate(the original value of
  //     TransformStreamDefaultController's prototype property).
  auto* controller = MakeGarbageCollected<TransformStreamDefaultController>();

  // 11. Perform ! SetUpTransformStreamDefaultController(stream, controller,
  //     transformAlgorithm, flushAlgorithm).
  TransformStreamDefaultController::SetUp(script_state, stream, controller,
                                          transform_algorithm, flush_algorithm);

  // 12. Let startResult be the result of performing startAlgorithm. (This may
  //     throw an exception.)
  v8::MaybeLocal<v8::Promise> start_result_maybe =
      start_algorithm->Run(script_state, exception_state);
  v8::Local<v8::Promise> start_result;
  if (!start_result_maybe.ToLocal(&start_result)) {
    CHECK(exception_state.HadException());
    return nullptr;
  }
  DCHECK(!exception_state.HadException());

  // 13. Resolve startPromise with startResult.
  start_promise->Resolve(start_result);

  // 14. Return stream.
  return stream;
}

// This constructor is only used internally.
TransformStream::TransformStream() = default;

TransformStream::TransformStream(ReadableStream* readable,
                                 WritableStream* writable)
    : readable_(readable), writable_(writable) {}

ReadableStreamDefaultController* TransformStream::GetReadableController() {
  // The type of source is not given when constructing the readable stream in
  // TranformStream, so it is guaranteed that the controller is a
  // ReadableStreamDefaultController.
  return To<ReadableStreamDefaultController>(readable_->GetController());
}

void TransformStream::Trace(Visitor* visitor) const {
  visitor->Trace(backpressure_change_promise_);
  visitor->Trace(readable_);
  visitor->Trace(transform_stream_controller_);
  visitor->Trace(writable_);
  ScriptWrappable::Trace(visitor);
}

// Implements the "an algorithm that returns startPromise" step from
// InitializeTransformStream():
// https://streams.spec.whatwg.org/#initialize-transform-stream.
class TransformStream::ReturnStartPromiseAlgorithm final
    : public StreamStartAlgorithm {
 public:
  explicit ReturnStartPromiseAlgorithm(
      ScriptPromiseResolver<IDLAny>* start_promise)
      : start_promise_(start_promise) {}

  v8::MaybeLocal<v8::Promise> Run(ScriptState*, ExceptionState&) override {
    return start_promise_->V8Promise();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(start_promise_);
    StreamStartAlgorithm::Trace(visitor);
  }

 private:
  Member<ScriptPromiseResolver<IDLAny>> start_promise_;
};

//
// The following algorithms are described as abstract operations in the
// standard, but need to be implemented as classes in C++ so that the data they
// store can be traced by the garbage collector correctly.
//
class TransformStream::DefaultSinkWriteAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit DefaultSinkWriteAlgorithm(TransformStream* stream)
      : stream_(stream) {}

  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 1);
    const auto chunk = argv[0];

    // https://streams.spec.whatwg.org/#transform-stream-default-sink-write-algorithm
    // 1. Assert: stream.[[writable]].[[state]] is "writable".
    DCHECK(stream_->writable_->IsWritable());

    // 2. Let controller be stream.[[transformStreamController]].
    TransformStreamDefaultController* controller =
        stream_->transform_stream_controller_;

    // 3. If stream.[[backpressure]] is true,
    if (stream_->had_backpressure_) {
      // a. Let backpressureChangePromise be
      //    stream.[[backpressureChangePromise]].
      auto* backpressure_change_promise =
          stream_->backpressure_change_promise_.Get();

      // b. Assert: backpressureChangePromise is not undefined.
      DCHECK(backpressure_change_promise);

      class ResponseFunction final : public PromiseHandlerWithValue {
       public:
        ResponseFunction(ScriptState* script_state,
                         TransformStream* stream,
                         v8::Local<v8::Value> chunk)
            : stream_(stream), chunk_(script_state->GetIsolate(), chunk) {}

        v8::Local<v8::Value> CallWithLocal(ScriptState* script_state,
                                           v8::Local<v8::Value>) override {
          auto* isolate = script_state->GetIsolate();

          // c. Return the result of transforming backpressureChangePromise with
          //    a fulfillment handler which performs the following steps:
          //    i. Let writable be stream.[[writable]].
          WritableStream* writable = stream_->writable_;

          //   ii. Let state be writable.[[state]].
          //  iii. If state is "erroring", throw writable.[[storedError]].
          if (writable->IsErroring()) {
            return PromiseReject(script_state,
                                 writable->GetStoredError(isolate));
          }

          // 4. Assert: state is "writable".
          CHECK(writable->IsWritable());

          // 5. Return ! TransformStreamDefaultControllerPerformTransform(
          //    controller, chunk).
          return TransformStreamDefaultController::PerformTransform(
              script_state, stream_->transform_stream_controller_,
              chunk_.Get(isolate));
        }

        void Trace(Visitor* visitor) const override {
          visitor->Trace(stream_);
          visitor->Trace(chunk_);
          PromiseHandlerWithValue::Trace(visitor);
        }

       private:
        Member<TransformStream> stream_;
        TraceWrapperV8Reference<v8::Value> chunk_;
      };

      // c. Return the result of transforming backpressureChangePromise ...
      return StreamThenPromise(
          script_state->GetContext(), backpressure_change_promise->V8Promise(),
          MakeGarbageCollected<ScriptFunction>(
              script_state, MakeGarbageCollected<ResponseFunction>(
                                script_state, stream_, chunk)));
    }

    //  4. Return ! TransformStreamDefaultControllerPerformTransform(controller,
    //     chunk).
    return TransformStreamDefaultController::PerformTransform(
        script_state, controller, chunk);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(stream_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  Member<TransformStream> stream_;
};

class TransformStream::DefaultSinkAbortAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit DefaultSinkAbortAlgorithm(TransformStream* stream)
      : stream_(stream) {}

  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 1);
    const auto reason = argv[0];

    // https://streams.spec.whatwg.org/#transform-stream-default-sink-abort-algorithm
    // 1. Perform ! TransformStreamError(stream, reason).
    Error(script_state, stream_, reason);

    // 2. Return a promise resolved with undefined.
    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(stream_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  Member<TransformStream> stream_;
};

class TransformStream::DefaultSinkCloseAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit DefaultSinkCloseAlgorithm(TransformStream* stream)
      : stream_(stream) {}

  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 0);
    // https://streams.spec.whatwg.org/#transform-stream-default-sink-close-algorithm
    // 1. Let readable be stream.[[readable]].

    // 2. Let controller be stream.[[transformStreamController]].
    TransformStreamDefaultController* controller =
        stream_->transform_stream_controller_;

    // 3. Let flushPromise be the result of performing
    //    controller.[[flushAlgorithm]].
    auto flush_promise =
        controller->flush_algorithm_->Run(script_state, 0, nullptr);

    // 4. Perform ! TransformStreamDefaultControllerClearAlgorithms(controller).
    TransformStreamDefaultController::ClearAlgorithms(controller);

    class ResolveFunction final : public PromiseHandlerWithValue {
     public:
      explicit ResolveFunction(TransformStream* stream) : stream_(stream) {}

      v8::Local<v8::Value> CallWithLocal(ScriptState* script_state,
                                         v8::Local<v8::Value>) override {
        // 5. Return the result of transforming flushPromise with:
        //    a. A fulfillment handler that performs the following steps:
        //       i. If readable.[[state]] is "errored", throw
        //          readable.[[storedError]].
        if (ReadableStream::IsErrored(stream_->readable_)) {
          // Returning a rejection is equivalent to throwing here.
          return PromiseReject(script_state, stream_->readable_->GetStoredError(
                                                 script_state->GetIsolate()));
        }

        //      ii. Let readableController be
        //          readable.[[readableStreamController]].
        auto* readable_controller = stream_->GetReadableController();

        //     iii. If ! ReadableStreamDefaultControllerCanCloseOrEnqueue(
        //          readableController) is true, perform !
        //          ReadableStreamDefaultControllerClose(readableController).
        if (ReadableStreamDefaultController::CanCloseOrEnqueue(
                readable_controller)) {
          ReadableStreamDefaultController::Close(script_state,
                                                 readable_controller);
        }

        return v8::Undefined(script_state->GetIsolate());
      }

      void Trace(Visitor* visitor) const override {
        visitor->Trace(stream_);
        PromiseHandlerWithValue::Trace(visitor);
      }

     private:
      Member<TransformStream> stream_;
    };

    class RejectFunction final : public PromiseHandlerWithValue {
     public:
      explicit RejectFunction(TransformStream* stream) : stream_(stream) {}

      v8::Local<v8::Value> CallWithLocal(ScriptState* script_state,
                                         v8::Local<v8::Value> r) override {
        // b. A rejection handler that, when called with argument r, performs
        //    the following steps:
        //    i. Perform ! TransformStreamError(stream, r).
        Error(script_state, stream_, r);

        //   ii. Throw readable.[[storedError]].
        return PromiseReject(script_state, stream_->readable_->GetStoredError(
                                               script_state->GetIsolate()));
      }

      void Trace(Visitor* visitor) const override {
        visitor->Trace(stream_);
        PromiseHandlerWithValue::Trace(visitor);
      }

     private:
      Member<TransformStream> stream_;
    };

    // 5. Return the result of transforming flushPromise ...
    return StreamThenPromise(
        script_state->GetContext(), flush_promise,
        MakeGarbageCollected<ScriptFunction>(
            script_state, MakeGarbageCollected<ResolveFunction>(stream_)),
        MakeGarbageCollected<ScriptFunction>(
            script_state, MakeGarbageCollected<RejectFunction>(stream_)));
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(stream_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  Member<TransformStream> stream_;
};

class TransformStream::DefaultSourcePullAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit DefaultSourcePullAlgorithm(TransformStream* stream)
      : stream_(stream) {}

  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 0);

    // https://streams.spec.whatwg.org/#transform-stream-default-source-pull
    // 1. Assert: stream.[[backpressure]] is true.
    DCHECK(stream_->had_backpressure_);

    // 2. Assert: stream.[[backpressureChangePromise]] is not undefined.
    DCHECK(stream_->backpressure_change_promise_);

    // 3. Perform ! TransformStreamSetBackpressure(stream, false).
    SetBackpressure(script_state, stream_, false);

    // 4. Return stream.[[backpressureChangePromise]].
    return stream_->backpressure_change_promise_->V8Promise();
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(stream_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  Member<TransformStream> stream_;
};

// This algorithm isn't explicitly named in the standard, but instead is
// described by a step in InitializeTransformStream.
class TransformStream::DefaultSourceCancelAlgorithm final
    : public StreamAlgorithm {
 public:
  explicit DefaultSourceCancelAlgorithm(TransformStream* stream)
      : stream_(stream) {}

  v8::Local<v8::Promise> Run(ScriptState* script_state,
                             int argc,
                             v8::Local<v8::Value> argv[]) override {
    DCHECK_EQ(argc, 1);

    // https://streams.spec.whatwg.org/#initialize-transform-stream
    // 7. Let cancelAlgorithm be the following steps, taking a reason argument:
    //    a. Perform ! TransformStreamErrorWritableAndUnblockWrite(stream,
    //       reason).
    ErrorWritableAndUnblockWrite(script_state, stream_, argv[0]);

    //    b. Return a promise resolved with undefined.
    return PromiseResolveWithUndefined(script_state);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(stream_);
    StreamAlgorithm::Trace(visitor);
  }

 private:
  Member<TransformStream> stream_;
};

// This is split out from the constructor in this implementation as calling
// JavaScript from inside a C++ constructor can cause GC problems.
void TransformStream::InitInternal(ScriptState* script_state,
                                   ScriptValue raw_transformer,
                                   ScriptValue raw_writable_strategy,
                                   ScriptValue raw_readable_strategy,
                                   ExceptionState& exception_state) {
  // TODO(ricea): Move this to IDL.
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kTransformStreamConstructor);

  DCHECK(!raw_transformer.IsEmpty());
  DCHECK(!raw_writable_strategy.IsEmpty());
  DCHECK(!raw_readable_strategy.IsEmpty());

  auto context = script_state->GetContext();
  auto* isolate = script_state->GetIsolate();

  // https://streams.spec.whatwg.org/#ts-constructor
  // Perform the "transformer = {}" step from the function signature.
  v8::Local<v8::Object> transformer;
  ScriptValueToObject(script_state, raw_transformer, &transformer,
                      exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // Perform the "writableStrategy = {}" step from the function signature, and
  // 1. Let writableSizeFunction be ? GetV(writableStrategy, "size").
  // 2. Let writableHighWaterMark be ? GetV(writableStrategy, "highWaterMark").
  StrategyUnpacker writable_strategy_unpacker(
      script_state, raw_writable_strategy, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // Perform the "readableStrategy = {}" step from the function signature, and
  // 3. Let readableSizeFunction be ? GetV(readableStrategy, "size").
  // 4. Let readableHighWaterMark be ? GetV(readableStrategy, "highWaterMark").
  StrategyUnpacker readable_strategy_unpacker(
      script_state, raw_readable_strategy, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  TryRethrowScope rethrow_scope(isolate, exception_state);

  // 5. Let writableType be ? GetV(transformer, "writableType").
  v8::Local<v8::Value> writable_type;
  if (!transformer->Get(context, V8AtomicString(isolate, "writableType"))
           .ToLocal(&writable_type)) {
    return;
  }

  // 6. If writableType is not undefined, throw a RangeError exception.
  if (!writable_type->IsUndefined()) {
    exception_state.ThrowRangeError("Invalid writableType was specified");
    return;
  }

  // 7. Let writableSizeAlgorithm be ? MakeSizeAlgorithmFromSizeFunction(
  //    writableSizeFunction).
  auto* writable_size_algorithm = writable_strategy_unpacker.MakeSizeAlgorithm(
      script_state, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 8. If writableHighWaterMark is undefined, set writableHighWaterMark to 1.
  // 9. Set writableHighWaterMark to ? ValidateAndNormalizeHighWaterMark(
  //    writableHighWaterMark).
  double writable_high_water_mark = writable_strategy_unpacker.GetHighWaterMark(
      script_state, 1, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 10. Let readableType be ? GetV(transformer, "readableType").
  v8::Local<v8::Value> readable_type;
  if (!transformer->Get(context, V8AtomicString(isolate, "readableType"))
           .ToLocal(&readable_type)) {
    return;
  }

  // 11. If readableType is not undefined, throw a RangeError exception.
  if (!readable_type->IsUndefined()) {
    exception_state.ThrowRangeError("Invalid readableType was specified");
    return;
  }

  // 12. Let readableSizeAlgorithm be ? MakeSizeAlgorithmFromSizeFunction(
  //     readableSizeFunction).
  auto* readable_size_algorithm = readable_strategy_unpacker.MakeSizeAlgorithm(
      script_state, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 13. If readableHighWaterMark is undefined, set readableHighWaterMark to 0.
  // 14. Set readableHighWaterMark be ? ValidateAndNormalizeHighWaterMark(
  //     readableHighWaterMark).
  double readable_high_water_mark = readable_strategy_unpacker.GetHighWaterMark(
      script_state, 0, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 15. Let startPromise be a new promise.
  auto* start_promise =
      MakeGarbageCollected<ScriptPromiseResolver<IDLAny>>(script_state);

  // 16. Perform ! InitializeTransformStream(this, startPromise,
  //     writableHighWaterMark, writableSizeAlgorithm, readableHighWaterMark,
  //     readableSizeAlgorithm).
  Initialize(script_state, this, start_promise, writable_high_water_mark,
             writable_size_algorithm, readable_high_water_mark,
             readable_size_algorithm, exception_state);

  // 17. Perform ? SetUpTransformStreamDefaultControllerFromTransformer(this,
  //     transformer).
  const auto controller_value =
      TransformStreamDefaultController::SetUpFromTransformer(
          script_state, this, transformer, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 18. Let startResult be ? InvokeOrNoop(transformer, "start", « this.
  //     [[transformStreamController]] »).
  v8::MaybeLocal<v8::Value> start_result_maybe =
      CallOrNoop1(script_state, transformer, "start", "transformer.start",
                  controller_value, exception_state);
  v8::Local<v8::Value> start_result;
  if (!start_result_maybe.ToLocal(&start_result)) {
    CHECK(exception_state.HadException());
    return;
  }
  DCHECK(!exception_state.HadException());

  // 19. Resolve startPromise with startResult.
  start_promise->Resolve(start_result);
}

void TransformStream::Initialize(ScriptState* script_state,
                                 TransformStream* stream,
                                 ScriptPromiseResolver<IDLAny>* start_promise,
                                 double writable_high_water_mark,
                                 StrategySizeAlgorithm* writable_size_algorithm,
                                 double readable_high_water_mark,
                                 StrategySizeAlgorithm* readable_size_algorithm,
                                 ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#initialize-transform-stream
  // 1. Let startAlgorithm be an algorithm that returns startPromise.
  auto* start_algorithm =
      MakeGarbageCollected<ReturnStartPromiseAlgorithm>(start_promise);

  // 2. Let writeAlgorithm be the following steps, taking a chunk argument:
  //    a. Return ! TransformStreamDefaultSinkWriteAlgorithm(stream, chunk).
  auto* write_algorithm =
      MakeGarbageCollected<DefaultSinkWriteAlgorithm>(stream);

  // 3. Let abortAlgorithm be the following steps, taking a reason argument:
  //    a. Return ! TransformStreamDefaultSinkAbortAlgorithm(stream, reason).
  auto* abort_algorithm =
      MakeGarbageCollected<DefaultSinkAbortAlgorithm>(stream);

  // 4. Let closeAlgorithm be the following steps:
  //    a. Return ! TransformStreamDefaultSinkCloseAlgorithm(stream).
  auto* close_algorithm =
      MakeGarbageCollected<DefaultSinkCloseAlgorithm>(stream);

  // 5. Set stream.[[writable]] to ! CreateWritableStream(startAlgorithm,
  //    writeAlgorithm, closeAlgorithm, abortAlgorithm, writableHighWaterMark,
  //    writableSizeAlgorithm).
  stream->writable_ = WritableStream::Create(
      script_state, start_algorithm, write_algorithm, close_algorithm,
      abort_algorithm, writable_high_water_mark, writable_size_algorithm,
      exception_state);
  DCHECK(!exception_state.HadException());

  // 6. Let pullAlgorithm be the following steps:
  //    a. Return ! TransformStreamDefaultSourcePullAlgorithm(stream).
  auto* pull_algorithm =
      MakeGarbageCollected<DefaultSourcePullAlgorithm>(stream);

  // 7. Let cancelAlgorithm be the following steps, taking a reason argument:
  //    a. Perform ! TransformStreamErrorWritableAndUnblockWrite(stream,
  //       reason).
  //    b. Return a promise resolved with undefined.
  auto* cancel_algorithm =
      MakeGarbageCollected<DefaultSourceCancelAlgorithm>(stream);

  // 8. Set stream.[[readable]] to ! CreateReadableStream(startAlgorithm,
  //    pullAlgorithm, cancelAlgorithm, readableHighWaterMark,
  //    readableSizeAlgorithm).
  stream->readable_ = ReadableStream::Create(
      script_state, start_algorithm, pull_algorithm, cancel_algorithm,
      readable_high_water_mark, readable_size_algorithm, exception_state);
  DCHECK(!exception_state.HadException());

  //  9. Set stream.[[backpressure]] and stream.[[backpressureChangePromise]] to
  //     undefined.
  // 10. Perform ! TransformStreamSetBackpressure(stream, true).
  // |had_backpressure_| is bool and so can't be set to undefined; instead we
  // take the equivalent steps to achieve the final result here.
  DCHECK(stream->had_backpressure_);
  DCHECK(!stream->backpressure_change_promise_);
  stream->backpressure_change_promise_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  stream->backpressure_change_promise_->SuppressDetachCheck();

  // 11. Set stream.[[transformStreamController]] to undefined.
  // (This is set by the constructor; just verify the value here).
  DCHECK_EQ(stream->transform_stream_controller_, nullptr);
}

void TransformStream::Error(ScriptState* script_state,
                            TransformStream* stream,
                            v8::Local<v8::Value> e) {
  // https://streams.spec.whatwg.org/#transform-stream-error
  // 1. Perform ! ReadableStreamDefaultControllerError(stream.[[readable]].
  //    [[readableStreamController]], e).
  ReadableStreamDefaultController::Error(script_state,
                                         stream->GetReadableController(), e);

  // 2. Perform ! TransformStreamErrorWritableAndUnblockWrite(stream, e).
  ErrorWritableAndUnblockWrite(script_state, stream, e);
}

void TransformStream::ErrorWritableAndUnblockWrite(ScriptState* script_state,
                                                   TransformStream* stream,
                                                   v8::Local<v8::Value> e) {
  // https://streams.spec.whatwg.org/#transform-stream-error-writable-and-unblock-write
  // 1. Perform ! TransformStreamDefaultControllerClearAlgorithms(stream.
  //    [[transformStreamController]]).
  TransformStreamDefaultController::ClearAlgorithms(
      stream->transform_stream_controller_);

  // 2. Perform !
  //    WritableStreamDefaultControllerErrorIfNeeded(stream.[[writable]].
  //    [[writableStreamController]], e).
  WritableStreamDefaultController::ErrorIfNeeded(
      script_state, stream->writable_->Controller(), e);

  // 3. If stream.[[backpressure]] is true, perform !
  //    TransformStreamSetBackpressure(stream, false).
  if (stream->had_backpressure_) {
    SetBackpressure(script_state, stream, false);
  }
}

void TransformStream::SetBackpressure(ScriptState* script_state,
                                      TransformStream* stream,
                                      bool backpressure) {
  // https://streams.spec.whatwg.org/#transform-stream-set-backpressure
  // 1. Assert: stream.[[backpressure]] is not backpressure.
  DCHECK(stream->had_backpressure_ != backpressure);

  // 2. If stream.[[backpressureChangePromise]] is not undefined, resolve
  //    stream.[[backpressureChangePromise]] with undefined.
  // In the standard, [[backpressureChangePromise]] is initialized by calling
  // this function. However, in this implementation it is initialized in
  // InitializeTransformStream() without calling this function. As a result,
  // the function is never called without |backpressure_change_promise_| set
  // and we don't need to test it.
  DCHECK(stream->backpressure_change_promise_);
  stream->backpressure_change_promise_->Resolve();

  // 3. Set stream.[[backpressureChangePromise]] to a new promise.
  stream->backpressure_change_promise_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  stream->backpressure_change_promise_->SuppressDetachCheck();

  // 4. Set stream.[[backpressure]] to backpressure.
  stream->had_backpressure_ = backpressure;
}

}  // namespace blink

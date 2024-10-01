// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_writable_stream_default_controller.h"
#include "third_party/blink/renderer/core/dom/abort_controller.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/promise_handler.h"
#include "third_party/blink/renderer/core/streams/queue_with_sizes.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

WritableStreamDefaultController* WritableStreamDefaultController::From(
    ScriptState* script_state,
    ScriptValue controller) {
  CHECK(controller.IsObject());
  auto* controller_impl = V8WritableStreamDefaultController::ToWrappable(
      script_state->GetIsolate(), controller.V8Value().As<v8::Object>());
  CHECK(controller_impl);
  return controller_impl;
}

// Only used internally. Not reachable from JavaScript.
WritableStreamDefaultController::WritableStreamDefaultController()
    : queue_(MakeGarbageCollected<QueueWithSizes>()) {}

void WritableStreamDefaultController::error(ScriptState* script_state) {
  error(script_state, ScriptValue(script_state->GetIsolate(),
                                  v8::Undefined(script_state->GetIsolate())));
}

void WritableStreamDefaultController::error(ScriptState* script_state,
                                            ScriptValue e) {
  // https://streams.spec.whatwg.org/#ws-default-controller-error
  //  2. Let state be this.[[controlledWritableStream]].[[state]].
  const auto state = controlled_writable_stream_->GetState();

  //  3. If state is not "writable", return.
  if (state != WritableStream::kWritable) {
    return;
  }
  //  4. Perform ! WritableStreamDefaultControllerError(this, e).
  Error(script_state, this, e.V8Value());
}

// Writable Stream Default Controller Internal Methods

v8::Local<v8::Promise> WritableStreamDefaultController::AbortSteps(
    ScriptState* script_state,
    v8::Local<v8::Value> reason) {
  // https://streams.spec.whatwg.org/#ws-default-controller-private-abort
  //  1. Let result be the result of performing this.[[abortAlgorithm]], passing
  //     reason.
  const auto result = abort_algorithm_->Run(script_state, 1, &reason);

  //  2. Perform ! WritableStreamDefaultControllerClearAlgorithms(this).
  ClearAlgorithms(this);

  //  3. Return result.
  return result;
}

void WritableStreamDefaultController::ErrorSteps() {
  // https://streams.spec.whatwg.org/#ws-default-controller-private-error
  //  1. Perform ! ResetQueue(this).
  queue_->ResetQueue();
}

// Writable Stream Default Controller Abstract Operations

// TODO(ricea): Should this be a constructor?
void WritableStreamDefaultController::SetUp(
    ScriptState* script_state,
    WritableStream* stream,
    WritableStreamDefaultController* controller,
    StreamStartAlgorithm* start_algorithm,
    StreamAlgorithm* write_algorithm,
    StreamAlgorithm* close_algorithm,
    StreamAlgorithm* abort_algorithm,
    double high_water_mark,
    StrategySizeAlgorithm* size_algorithm,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller
  //  2. Assert: stream.[[writableStreamController]] is undefined.
  DCHECK(!stream->Controller());

  //  3. Set controller.[[controlledWritableStream]] to stream.
  controller->controlled_writable_stream_ = stream;

  //  4. Set stream.[[writableStreamController]] to controller.
  stream->SetController(controller);

  // Step not needed because queue is initialised during construction.
  //  5. Perform ! ResetQueue(controller).

  //  6. Set controller.[[abortController]] to a new AbortController.
  controller->abort_controller_ = AbortController::Create(script_state);

  //  7. Set controller.[[started]] to false.
  controller->started_ = false;

  //  8. Set controller.[[strategySizeAlgorithm]] to sizeAlgorithm.
  controller->strategy_size_algorithm_ = size_algorithm;

  //  9. Set controller.[[strategyHWM]] to highWaterMark.
  controller->strategy_high_water_mark_ = high_water_mark;

  // 10. Set controller.[[writeAlgorithm]] to writeAlgorithm.
  controller->write_algorithm_ = write_algorithm;

  // 11. Set controller.[[closeAlgorithm]] to closeAlgorithm.
  controller->close_algorithm_ = close_algorithm;

  // 12. Set controller.[[abortAlgorithm]] to abortAlgorithm.
  controller->abort_algorithm_ = abort_algorithm;

  // 13. Let backpressure be !
  //     WritableStreamDefaultControllerGetBackpressure(controller).
  const bool backpressure = GetBackpressure(controller);

  // 14. Perform ! WritableStreamUpdateBackpressure(stream, backpressure).
  WritableStream::UpdateBackpressure(script_state, stream, backpressure);

  // 15. Let startResult be the result of performing startAlgorithm. (This may
  //     throw an exception.)
  // In this implementation, start_algorithm returns a Promise when it doesn't
  // throw.
  // 16. Let startPromise be a promise resolved with startResult.
  v8::Local<v8::Promise> start_promise;
  if (!start_algorithm->Run(script_state, exception_state)
           .ToLocal(&start_promise)) {
    if (!exception_state.HadException()) {
      // Is this block really needed? Can we make this a DCHECK?
      exception_state.ThrowException(
          static_cast<int>(DOMExceptionCode::kInvalidStateError),
          "start algorithm failed with no exception thrown");
    }
    return;
  }
  DCHECK(!exception_state.HadException());

  class ResolvePromiseFunction final : public PromiseHandler {
   public:
    explicit ResolvePromiseFunction(WritableStream* stream) : stream_(stream) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value>) override {
      // 17. Upon fulfillment of startPromise
      //      a. Assert: stream.[[state]] is "writable" or "erroring".
      const auto state = stream_->GetState();
      CHECK(state == WritableStream::kWritable ||
            state == WritableStream::kErroring);

      //      b. Set controller.[[started]] to true.
      WritableStreamDefaultController* controller = stream_->Controller();
      controller->started_ = true;

      //      c. Perform ! WritableStreamDefaultControllerAdvanceQueueIfNeeded(
      //         controller).
      WritableStreamDefaultController::AdvanceQueueIfNeeded(script_state,
                                                            controller);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(stream_);
      PromiseHandler::Trace(visitor);
    }

   private:
    Member<WritableStream> stream_;
  };

  class RejectPromiseFunction final : public PromiseHandler {
   public:
    explicit RejectPromiseFunction(WritableStream* stream) : stream_(stream) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value> r) override {
      // 18. Upon rejection of startPromise with reason r,
      //      a. Assert: stream.[[state]] is "writable" or "erroring".
      const auto state = stream_->GetState();
      CHECK(state == WritableStream::kWritable ||
            state == WritableStream::kErroring);

      //      b. Set controller.[[started]] to true.
      WritableStreamDefaultController* controller = stream_->Controller();
      controller->started_ = true;

      //      c. Perform ! WritableStreamDealWithRejection(stream, r).
      WritableStream::DealWithRejection(script_state, stream_, r);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(stream_);
      PromiseHandler::Trace(visitor);
    }

   private:
    Member<WritableStream> stream_;
  };

  StreamThenPromise(
      script_state->GetContext(), start_promise,
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<ResolvePromiseFunction>(stream)),
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<RejectPromiseFunction>(stream)));

  class ProcessWriteResolveFunction final : public PromiseHandler {
   public:
    ProcessWriteResolveFunction(WritableStream* stream,
                                WritableStreamDefaultController* controller)
        : stream_(stream), controller_(controller) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value>) override {
      // https://streams.spec.whatwg.org/#writable-stream-default-controller-process-write
      //  4. Upon fulfillment of sinkWritePromise,
      //      a. Perform ! WritableStreamFinishInFlightWrite(stream).
      WritableStream::FinishInFlightWrite(script_state, stream_);

      //      b. Let state be stream.[[state]].
      const auto state = stream_->GetState();

      //      c. Assert: state is "writable" or "erroring".
      CHECK(state == WritableStream::kWritable ||
            state == WritableStream::kErroring);

      //      d. Perform ! DequeueValue(controller).
      controller_->queue_->DequeueValue(script_state->GetIsolate());

      //      e. If ! WritableStreamCloseQueuedOrInFlight(stream) is false and
      //         state is "writable",
      if (!WritableStream::CloseQueuedOrInFlight(stream_) &&
          state == WritableStream::kWritable) {
        //          i. Let backpressure be !
        //             WritableStreamDefaultControllerGetBackpressure(
        //             controller).
        const bool backpressure =
            WritableStreamDefaultController::GetBackpressure(controller_);

        //         ii. Perform ! WritableStreamUpdateBackpressure(stream,
        //             backpressure).
        WritableStream::UpdateBackpressure(script_state, stream_, backpressure);
      }
      //      f. Perform ! WritableStreamDefaultControllerAdvanceQueueIfNeeded(
      //         controller).
      WritableStreamDefaultController::AdvanceQueueIfNeeded(script_state,
                                                            controller_);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(stream_);
      visitor->Trace(controller_);
      PromiseHandler::Trace(visitor);
    }

   private:
    Member<WritableStream> stream_;
    Member<WritableStreamDefaultController> controller_;
  };

  class ProcessWriteRejectFunction final : public PromiseHandler {
   public:
    ProcessWriteRejectFunction(WritableStream* stream,
                               WritableStreamDefaultController* controller)
        : stream_(stream), controller_(controller) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value> reason) override {
      const auto state = stream_->GetState();
      //  5. Upon rejection of sinkWritePromise with reason,
      //      a. If stream.[[state]] is "writable", perform !
      //         WritableStreamDefaultControllerClearAlgorithms(controller).
      if (state == WritableStream::kWritable) {
        WritableStreamDefaultController::ClearAlgorithms(controller_);
      }

      //      b. Perform ! WritableStreamFinishInFlightWriteWithError(stream,
      //         reason).
      WritableStream::FinishInFlightWriteWithError(script_state, stream_,
                                                   reason);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(stream_);
      visitor->Trace(controller_);
      PromiseHandler::Trace(visitor);
    }

   private:
    Member<WritableStream> stream_;
    Member<WritableStreamDefaultController> controller_;
  };

  controller->resolve_function_ = MakeGarbageCollected<ScriptFunction>(
      script_state, MakeGarbageCollected<ProcessWriteResolveFunction>(
                        controller->controlled_writable_stream_, controller));
  controller->reject_function_ = MakeGarbageCollected<ScriptFunction>(
      script_state, MakeGarbageCollected<ProcessWriteRejectFunction>(
                        controller->controlled_writable_stream_, controller));
}

// TODO(ricea): Should this be a constructor?
void WritableStreamDefaultController::SetUpFromUnderlyingSink(
    ScriptState* script_state,
    WritableStream* stream,
    v8::Local<v8::Object> underlying_sink,
    double high_water_mark,
    StrategySizeAlgorithm* size_algorithm,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#set-up-writable-stream-default-controller-from-underlying-sink
  //  1. Assert: underlyingSink is not undefined.
  DCHECK(!underlying_sink.IsEmpty());

  //  2. Let controller be ObjectCreate(the original value of
  //     WritableStreamDefaultController's prototype property).
  auto* controller = MakeGarbageCollected<WritableStreamDefaultController>();

  // This method is only called when a WritableStream is being constructed by
  // JavaScript. So the execution context should be valid and this call should
  // not crash.
  auto controller_value = ToV8Traits<WritableStreamDefaultController>::ToV8(
      script_state, controller);

  //  3. Let startAlgorithm be the following steps:
  //      a. Return ? InvokeOrNoop(underlyingSink, "start", « controller »).
  auto* start_algorithm = CreateStartAlgorithm(
      script_state, underlying_sink, "underlyingSink.start", controller_value);

  //  4. Let writeAlgorithm be ? CreateAlgorithmFromUnderlyingMethod(
  //     underlyingSink, "write", 1, « controller »).
  auto* write_algorithm = CreateAlgorithmFromUnderlyingMethod(
      script_state, underlying_sink, "write", "underlyingSink.write",
      controller_value, exception_state);
  if (exception_state.HadException()) {
    return;
  }
  DCHECK(write_algorithm);

  //  5. Let closeAlgorithm be ? CreateAlgorithmFromUnderlyingMethod(
  //     underlyingSink, "close", 0, « »).
  auto* close_algorithm = CreateAlgorithmFromUnderlyingMethod(
      script_state, underlying_sink, "close", "underlyingSink.close",
      v8::MaybeLocal<v8::Value>(), exception_state);
  if (exception_state.HadException()) {
    return;
  }
  DCHECK(close_algorithm);

  //  6. Let abortAlgorithm be ? CreateAlgorithmFromUnderlyingMethod(
  //     underlyingSink, "abort", 1, « »).
  auto* abort_algorithm = CreateAlgorithmFromUnderlyingMethod(
      script_state, underlying_sink, "abort", "underlyingSink.abort",
      v8::MaybeLocal<v8::Value>(), exception_state);
  if (exception_state.HadException()) {
    return;
  }
  DCHECK(abort_algorithm);

  //  7. Perform ? SetUpWritableStreamDefaultController(stream, controller,
  //     startAlgorithm, writeAlgorithm, closeAlgorithm, abortAlgorithm,
  //     highWaterMark, sizeAlgorithm).
  SetUp(script_state, stream, controller, start_algorithm, write_algorithm,
        close_algorithm, abort_algorithm, high_water_mark, size_algorithm,
        exception_state);
}

void WritableStreamDefaultController::Close(
    ScriptState* script_state,
    WritableStreamDefaultController* controller) {
  // https://streams.spec.whatwg.org/#writable-stream-default-controller-close
  //  1. Perform ! EnqueueValueWithSize(controller, "close", 0).
  // The |close_queued_| flag represents the presence of the `"close"` marker
  // in the queue.
  controller->close_queued_ = true;

  //  2. Perform ! WritableStreamDefaultControllerAdvanceQueueIfNeeded(
  //     controller).
  AdvanceQueueIfNeeded(script_state, controller);
}

double WritableStreamDefaultController::GetChunkSize(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    v8::Local<v8::Value> chunk) {
  if (!controller->strategy_size_algorithm_) {
    DCHECK_NE(controller->controlled_writable_stream_->GetState(),
              WritableStream::kWritable);
    // No need to error since the stream is already stopped or stopping.
    return 1;
  }

  // https://streams.spec.whatwg.org/#writable-stream-default-controller-get-chunk-size
  //  1. Let returnValue be the result of performing
  //     controller.[[strategySizeAlgorithm]], passing in chunk, and
  //     interpreting the result as an ECMAScript completion value.
  v8::TryCatch try_catch(script_state->GetIsolate());
  auto return_value =
      controller->strategy_size_algorithm_->Run(script_state, chunk);

  //  2. If returnValue is an abrupt completion,
  if (!return_value.has_value()) {
    //      a. Perform ! WritableStreamDefaultControllerErrorIfNeeded(
    //         controller, returnValue.[[Value]]).
    ErrorIfNeeded(script_state, controller, try_catch.Exception());

    //      b. Return 1.
    return 1;
  }
  //  3. Return returnValue.[[Value]].
  return return_value.value();
}

double WritableStreamDefaultController::GetDesiredSize(
    const WritableStreamDefaultController* controller) {
  // https://streams.spec.whatwg.org/#writable-stream-default-controller-get-desired-size
  //  1. Return controller.[[strategyHWM]] − controller.[[queueTotalSize]].
  return controller->strategy_high_water_mark_ -
         controller->queue_->TotalSize();
}

void WritableStreamDefaultController::Write(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    v8::Local<v8::Value> chunk,
    double chunk_size,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#writable-stream-default-controller-write
  // The chunk is represented literally in the queue, rather than being embedded
  // in an object, so the following step is not performed:
  //  1. Let writeRecord be Record {[[chunk]]: chunk}.
  {
    //  2. Let enqueueResult be EnqueueValueWithSize(controller, writeRecord,
    //     chunkSize).
    v8::Isolate* isolate = script_state->GetIsolate();
    v8::TryCatch try_catch(isolate);
    controller->queue_->EnqueueValueWithSize(isolate, chunk, chunk_size,
                                             PassThroughException(isolate));

    //  3. If enqueueResult is an abrupt completion,
    if (try_catch.HasCaught()) {
      //      a. Perform ! WritableStreamDefaultControllerErrorIfNeeded(
      //         controller, enqueueResult.[[Value]]).

      ErrorIfNeeded(script_state, controller, try_catch.Exception());

      //      b. Return.
      return;
    }
  }
  //  4. Let stream be controller.[[controlledWritableStream]].
  WritableStream* stream = controller->controlled_writable_stream_;

  //  5. If ! WritableStreamCloseQueuedOrInFlight(stream) is false and
  //     stream.[[state]] is "writable",
  if (!WritableStream::CloseQueuedOrInFlight(stream) &&
      stream->GetState() == WritableStream::kWritable) {
    //      a. Let backpressure be !
    //         WritableStreamDefaultControllerGetBackpressure(controller).
    const bool backpressure = GetBackpressure(controller);

    //      b. Perform ! WritableStreamUpdateBackpressure(stream, backpressure).
    WritableStream::UpdateBackpressure(script_state, stream, backpressure);
  }

  //  6. Perform ! WritableStreamDefaultControllerAdvanceQueueIfNeeded(
  //     controller).
  AdvanceQueueIfNeeded(script_state, controller);
}

void WritableStreamDefaultController::ErrorIfNeeded(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    v8::Local<v8::Value> error) {
  // https://streams.spec.whatwg.org/#writable-stream-default-controller-error-if-needed
  //  1. If controller.[[controlledWritableStream]].[[state]] is "writable",
  //     perform ! WritableStreamDefaultControllerError(controller, error).
  const auto state = controller->controlled_writable_stream_->GetState();
  if (state == WritableStream::kWritable) {
    Error(script_state, controller, error);
  }
}

void WritableStreamDefaultController::Trace(Visitor* visitor) const {
  visitor->Trace(abort_algorithm_);
  visitor->Trace(close_algorithm_);
  visitor->Trace(controlled_writable_stream_);
  visitor->Trace(queue_);
  visitor->Trace(abort_controller_);
  visitor->Trace(strategy_size_algorithm_);
  visitor->Trace(write_algorithm_);
  visitor->Trace(resolve_function_);
  visitor->Trace(reject_function_);
  ScriptWrappable::Trace(visitor);
}

void WritableStreamDefaultController::ClearAlgorithms(
    WritableStreamDefaultController* controller) {
  // https://streams.spec.whatwg.org/#writable-stream-default-controller-clear-algorithms
  //  1. Set controller.[[writeAlgorithm]] to undefined.
  controller->write_algorithm_ = nullptr;

  //  2. Set controller.[[closeAlgorithm]] to undefined.
  controller->close_algorithm_ = nullptr;

  //  3. Set controller.[[abortAlgorithm]] to undefined.
  controller->abort_algorithm_ = nullptr;

  //  4. Set controller.[[strategySizeAlgorithm]] to undefined.
  controller->strategy_size_algorithm_ = nullptr;
}

void WritableStreamDefaultController::AdvanceQueueIfNeeded(
    ScriptState* script_state,
    WritableStreamDefaultController* controller) {
  // https://streams.spec.whatwg.org/#writable-stream-default-controller-advance-queue-if-needed
  //  1. Let stream be controller.[[controlledWritableStream]].
  WritableStream* stream = controller->controlled_writable_stream_;

  //  2. If controller.[[started]] is false, return
  if (!controller->started_) {
    return;
  }

  //  3. If stream.[[inFlightWriteRequest]] is not undefined, return.
  if (stream->HasInFlightWriteRequest()) {
    return;
  }

  //  4. Let state be stream.[[state]].
  const auto state = stream->GetState();

  //  5. If state is "closed" or "errored", return.
  if (state == WritableStream::kClosed || state == WritableStream::kErrored) {
    return;
  }

  //  6. If state is "erroring",
  if (state == WritableStream::kErroring) {
    //      a. Perform ! WritableStreamFinishErroring(stream).
    WritableStream::FinishErroring(script_state, stream);

    //      b. Return.
    return;
  }

  //  7. If controller.[[queue]] is empty, return.
  if (controller->queue_->IsEmpty()) {
    // Empty queue + |close_queued_| true implies `"close"` marker in queue.
    //  9. If writeRecord is "close", perform !
    //     WritableStreamDefaultControllerProcessClose(controller).
    if (controller->close_queued_) {
      ProcessClose(script_state, controller);
    }
    return;
  }

  //  8. Let writeRecord be ! PeekQueueValue(controller).
  const auto chunk =
      controller->queue_->PeekQueueValue(script_state->GetIsolate());

  // 10. Otherwise, perform ! WritableStreamDefaultControllerProcessWrite(
  //     controller, writeRecord.[[chunk]]).
  // ("Otherwise" here means if the chunk is not a `"close"` marker).
  WritableStreamDefaultController::ProcessWrite(script_state, controller,
                                                chunk);
}

void WritableStreamDefaultController::ProcessClose(
    ScriptState* script_state,
    WritableStreamDefaultController* controller) {
  // https://streams.spec.whatwg.org/#writable-stream-default-controller-process-close
  //  1. Let stream be controller.[[controlledWritableStream]].
  WritableStream* stream = controller->controlled_writable_stream_;

  //  2. Perform ! WritableStreamMarkCloseRequestInFlight(stream).
  WritableStream::MarkCloseRequestInFlight(stream);

  //  3. Perform ! DequeueValue(controller).
  // Here we "dequeue" the `"close"` marker, which is implied by the
  // |close_queued_| flag, by unsetting the flag.
  //  4. Assert: controller.[[queue]] is empty.
  DCHECK(controller->queue_->IsEmpty());
  DCHECK(controller->close_queued_);
  controller->close_queued_ = false;

  //  5. Let sinkClosePromise be the result of performing
  //     controller.[[closeAlgorithm]].
  const auto sinkClosePromise =
      controller->close_algorithm_->Run(script_state, 0, nullptr);

  //  6. Perform ! WritableStreamDefaultControllerClearAlgorithms(controller).
  ClearAlgorithms(controller);

  class ResolveFunction final : public PromiseHandler {
   public:
    explicit ResolveFunction(WritableStream* stream) : stream_(stream) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value>) override {
      //  7. Upon fulfillment of sinkClosePromise,
      //      a. Perform ! WritableStreamFinishInFlightClose(stream).
      WritableStream::FinishInFlightClose(script_state, stream_);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(stream_);
      PromiseHandler::Trace(visitor);
    }

   private:
    Member<WritableStream> stream_;
  };

  class RejectFunction final : public PromiseHandler {
   public:
    explicit RejectFunction(WritableStream* stream) : stream_(stream) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value> reason) override {
      //  8. Upon rejection of sinkClosePromise with reason reason,
      //      a. Perform ! WritableStreamFinishInFlightCloseWithError(stream,
      //         reason).
      WritableStream::FinishInFlightCloseWithError(script_state, stream_,
                                                   reason);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(stream_);
      PromiseHandler::Trace(visitor);
    }

   private:
    Member<WritableStream> stream_;
  };

  StreamThenPromise(
      script_state->GetContext(), sinkClosePromise,
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<ResolveFunction>(stream)),
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<RejectFunction>(stream)));
}

void WritableStreamDefaultController::ProcessWrite(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    v8::Local<v8::Value> chunk) {
  // https://streams.spec.whatwg.org/#writable-stream-default-controller-process-write
  //  1. Let stream be controller.[[controlledWritableStream]].
  WritableStream* stream = controller->controlled_writable_stream_;

  //  2. Perform ! WritableStreamMarkFirstWriteRequestInFlight(stream).
  WritableStream::MarkFirstWriteRequestInFlight(stream);

  //  3. Let sinkWritePromise be the result of performing
  //     controller.[[writeAlgorithm]], passing in chunk.
  const auto sinkWritePromise =
      controller->write_algorithm_->Run(script_state, 1, &chunk);

  StreamThenPromise(script_state->GetContext(), sinkWritePromise,
                    controller->resolve_function_,
                    controller->reject_function_);
}

bool WritableStreamDefaultController::GetBackpressure(
    const WritableStreamDefaultController* controller) {
  // https://streams.spec.whatwg.org/#writable-stream-default-controller-get-backpressure
  //  1. Let desiredSize be ! WritableStreamDefaultControllerGetDesiredSize(
  //     controller).
  const double desired_size = GetDesiredSize(controller);

  //  2. Return desiredSize ≤ 0.
  return desired_size <= 0;
}

void WritableStreamDefaultController::Error(
    ScriptState* script_state,
    WritableStreamDefaultController* controller,
    v8::Local<v8::Value> error) {
  // https://streams.spec.whatwg.org/#writable-stream-default-controller-error
  //  1. Let stream be controller.[[controlledWritableStream]].
  WritableStream* stream = controller->controlled_writable_stream_;

  //  2. Assert: stream.[[state]] is "writable".
  DCHECK_EQ(stream->GetState(), WritableStream::kWritable);

  //  3. Perform ! WritableStreamDefaultControllerClearAlgorithms(controller).
  ClearAlgorithms(controller);

  //  4. Perform ! WritableStreamStartErroring(stream, error).
  WritableStream::StartErroring(script_state, stream, error);
}

AbortSignal* WritableStreamDefaultController::signal() const {
  return abort_controller_->signal();
}

void WritableStreamDefaultController::Abort(ScriptState* script_state,
                                            ScriptValue reason) {
  abort_controller_->abort(script_state, reason);
}

}  // namespace blink

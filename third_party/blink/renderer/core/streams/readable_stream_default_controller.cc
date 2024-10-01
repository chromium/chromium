// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/promise_handler.h"
#include "third_party/blink/renderer/core/streams/queue_with_sizes.h"
#include "third_party/blink/renderer/core/streams/read_request.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

// This constructor is used internally; it is not reachable from JavaScript.
ReadableStreamDefaultController::ReadableStreamDefaultController(
    ScriptState* script_state)
    : queue_(MakeGarbageCollected<QueueWithSizes>()) {
  class CallPullIfNeededResolveFunction final : public PromiseHandler {
   public:
    explicit CallPullIfNeededResolveFunction(
        ReadableStreamDefaultController* controller)
        : controller_(controller) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value>) override {
      // https://streams.spec.whatwg.org/#readable-stream-default-controller-call-pull-if-needed
      // 7. Upon fulfillment of pullPromise,
      //   a. Set controller.[[pulling]] to false.
      controller_->is_pulling_ = false;

      //   b. If controller.[[pullAgain]] is true,
      if (controller_->will_pull_again_) {
        //  i. Set controller.[[pullAgain]] to false.
        controller_->will_pull_again_ = false;

        //  ii. Perform ! ReadableStreamDefaultControllerCallPullIfNeeded(
        //      controller).
        CallPullIfNeeded(script_state, controller_);
      }
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(controller_);
      PromiseHandler::Trace(visitor);
    }

   private:
    const Member<ReadableStreamDefaultController> controller_;
  };

  class CallPullIfNeededRejectFunction final : public PromiseHandler {
   public:
    explicit CallPullIfNeededRejectFunction(
        ReadableStreamDefaultController* controller)
        : controller_(controller) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value> e) override {
      // 8. Upon rejection of pullPromise with reason e,
      //   a. Perform ! ReadableStreamDefaultControllerError(controller, e).
      Error(script_state, controller_, e);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(controller_);
      PromiseHandler::Trace(visitor);
    }

   private:
    const Member<ReadableStreamDefaultController> controller_;
  };

  resolve_function_ = MakeGarbageCollected<ScriptFunction>(
      script_state,
      MakeGarbageCollected<CallPullIfNeededResolveFunction>(this));
  reject_function_ = MakeGarbageCollected<ScriptFunction>(
      script_state, MakeGarbageCollected<CallPullIfNeededRejectFunction>(this));
}

void ReadableStreamDefaultController::close(ScriptState* script_state,
                                            ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-default-controller-close
  // 2. If ! ReadableStreamDefaultControllerCanCloseOrEnqueue(this) is false,
  //    throw a TypeError exception.
  if (!CanCloseOrEnqueue(this)) {
    // The following code is just to provide a nice exception message.
    const char* errorDescription = nullptr;
    if (is_close_requested_) {
      errorDescription =
          "Cannot close a readable stream that has already been requested to "
          "be closed";
    } else {
      const ReadableStream* stream = controlled_readable_stream_;
      switch (stream->state_) {
        case ReadableStream::kErrored:
          errorDescription = "Cannot close an errored readable stream";
          break;

        case ReadableStream::kClosed:
          errorDescription = "Cannot close an errored readable stream";
          break;

        default:
          NOTREACHED_IN_MIGRATION();
          break;
      }
    }
    exception_state.ThrowTypeError(errorDescription);
    return;
  }

  // 3. Perform ! ReadableStreamDefaultControllerClose(this).
  return Close(script_state, this);
}

void ReadableStreamDefaultController::enqueue(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  enqueue(script_state,
          ScriptValue(script_state->GetIsolate(),
                      v8::Undefined(script_state->GetIsolate())),
          exception_state);
}

void ReadableStreamDefaultController::enqueue(ScriptState* script_state,
                                              ScriptValue chunk,
                                              ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-default-controller-enqueue
  // 2. If ! ReadableStreamDefaultControllerCanCloseOrEnqueue(this) is false,
  //    throw a TypeError exception.
  if (!CanCloseOrEnqueue(this)) {
    exception_state.ThrowTypeError(EnqueueExceptionMessage(this));
    return;
  }

  // 3. Return ? ReadableStreamDefaultControllerEnqueue(this, chunk).
  return Enqueue(script_state, this, chunk.V8Value(), exception_state);
}

void ReadableStreamDefaultController::error(ScriptState* script_state) {
  error(script_state, ScriptValue(script_state->GetIsolate(),
                                  v8::Undefined(script_state->GetIsolate())));
}

void ReadableStreamDefaultController::error(ScriptState* script_state,
                                            ScriptValue e) {
  // https://streams.spec.whatwg.org/#rs-default-controller-error
  // 2. Perform ! ReadableStreamDefaultControllerError(this, e).
  Error(script_state, this, e.V8Value());
}

void ReadableStreamDefaultController::Close(
    ScriptState* script_state,
    ReadableStreamDefaultController* controller) {
  // https://streams.spec.whatwg.org/#readable-stream-default-controller-close
  // 1. Let stream be controller.[[controlledReadableStream]].
  ReadableStream* stream = controller->controlled_readable_stream_;

  // 2. Assert: ! ReadableStreamDefaultControllerCanCloseOrEnqueue(controller)
  //    is true.
  CHECK(CanCloseOrEnqueue(controller));

  // 3. Set controller.[[closeRequested]] to true.
  controller->is_close_requested_ = true;

  // 4. If controller.[[queue]] is empty,
  if (controller->queue_->IsEmpty()) {
    // a. Perform ! ReadableStreamDefaultControllerClearAlgorithms(controller).
    ClearAlgorithms(controller);

    // b. Perform ! ReadableStreamClose(stream).
    ReadableStream::Close(script_state, stream);
  }
}

void ReadableStreamDefaultController::Enqueue(
    ScriptState* script_state,
    ReadableStreamDefaultController* controller,
    v8::Local<v8::Value> chunk,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-stream-default-controller-enqueue
  // 1. If ! ReadableStreamDefaultControllerCanCloseOrEnqueue(controller) is
  //    false, return.
  if (!CanCloseOrEnqueue(controller)) {
    return;
  }

  // 2. Let stream be controller.[[stream]].
  const auto stream = controller->controlled_readable_stream_;

  // 3. If ! IsReadableStreamLocked(stream) is true and !
  //    ReadableStreamGetNumReadRequests(stream) > 0, perform !
  //    ReadableStreamFulfillReadRequest(stream, chunk, false).
  if (ReadableStream::IsLocked(stream) &&
      ReadableStream::GetNumReadRequests(stream) > 0) {
    ReadableStream::FulfillReadRequest(script_state, stream, chunk, false,
                                       exception_state);
  } else {
    // 4. Otherwise,
    //   a. Let result be the result of performing controller.
    //      [[strategySizeAlgorithm]], passing in chunk, and interpreting the
    //      result as an ECMAScript completion value.
    TryRethrowScope rethrow_scope(script_state->GetIsolate(), exception_state);
    std::optional<double> chunk_size =
        controller->strategy_size_algorithm_->Run(script_state, chunk);

    //   b. If result is an abrupt completion,
    if (rethrow_scope.HasCaught()) {
      //    i. Perform ! ReadableStreamDefaultControllerError(controller,
      //       result.[[Value]]).
      Error(script_state, controller, rethrow_scope.GetException());
      //    ii. Return result.
      return;
    }
    DCHECK(chunk_size.has_value());

    //  c. Let chunkSize be result.[[Value]].
    //  d. Let enqueueResult be EnqueueValueWithSize(controller, chunk,
    //     chunkSize).
    controller->queue_->EnqueueValueWithSize(
        script_state->GetIsolate(), chunk, chunk_size.value(),
        PassThroughException(script_state->GetIsolate()));

    //   e. If enqueueResult is an abrupt completion,
    if (rethrow_scope.HasCaught()) {
      //    i. Perform ! ReadableStreamDefaultControllerError(controller,
      //       enqueueResult.[[Value]]).
      Error(script_state, controller, rethrow_scope.GetException());
      //    ii. Return enqueueResult.
      return;
    }
  }

  // 5. Perform ! ReadableStreamDefaultControllerCallPullIfNeeded(controller).
  CallPullIfNeeded(script_state, controller);
}

void ReadableStreamDefaultController::Error(
    ScriptState* script_state,
    ReadableStreamDefaultController* controller,
    v8::Local<v8::Value> e) {
  // https://streams.spec.whatwg.org/#readable-stream-default-controller-error
  // 1. Let stream be controller.[[controlledReadableStream]].
  ReadableStream* stream = controller->controlled_readable_stream_;

  // 2. If stream.[[state]] is not "readable", return.
  if (stream->state_ != ReadableStream::kReadable) {
    return;
  }

  // 3. Perform ! ResetQueue(controller).
  controller->queue_->ResetQueue();

  // 4. Perform ! ReadableStreamDefaultControllerClearAlgorithms(controller).
  ClearAlgorithms(controller);

  // 5. Perform ! ReadableStreamError(stream, e).
  ReadableStream::Error(script_state, stream, e);
}

// This is an instance method rather than the static function in the standard,
// so |this| is |controller|.
std::optional<double> ReadableStreamDefaultController::GetDesiredSize() const {
  // https://streams.spec.whatwg.org/#readable-stream-default-controller-get-desired-size
  switch (controlled_readable_stream_->state_) {
    // 3. If state is "errored", return null.
    case ReadableStream::kErrored:
      return std::nullopt;

    // 4. If state is "closed", return 0.
    case ReadableStream::kClosed:
      return 0.0;

    case ReadableStream::kReadable:
      // 5. Return controller.[[strategyHWM]] − controller.[[queueTotalSize]].
      return strategy_high_water_mark_ - queue_->TotalSize();
  }
}

bool ReadableStreamDefaultController::CanCloseOrEnqueue(
    const ReadableStreamDefaultController* controller) {
  // https://streams.spec.whatwg.org/#readable-stream-default-controller-can-close-or-enqueue
  // 1. Let state be controller.[[controlledReadableStream]].[[state]].
  const auto state = controller->controlled_readable_stream_->state_;

  // 2. If controller.[[closeRequested]] is false and state is "readable",
  //    return true.
  // 3. Otherwise, return false.
  return !controller->is_close_requested_ && state == ReadableStream::kReadable;
}

bool ReadableStreamDefaultController::HasBackpressure(
    const ReadableStreamDefaultController* controller) {
  // https://streams.spec.whatwg.org/#rs-default-controller-has-backpressure
  // 1. If ! ReadableStreamDefaultControllerShouldCallPull(controller) is true,
  //    return false.
  // 2. Otherwise, return true.
  return !ShouldCallPull(controller);
}

// Used internally by enqueue() and also by TransformStream.
const char* ReadableStreamDefaultController::EnqueueExceptionMessage(
    const ReadableStreamDefaultController* controller) {
  if (controller->is_close_requested_) {
    return "Cannot enqueue a chunk into a readable stream that is closed or "
           "has been requested to be closed";
  }

  const ReadableStream* stream = controller->controlled_readable_stream_;
  const auto state = stream->state_;
  if (state == ReadableStream::kErrored) {
    return "Cannot enqueue a chunk into an errored readable stream";
  }
  CHECK(state == ReadableStream::kClosed);
  return "Cannot enqueue a chunk into a closed readable stream";
}

void ReadableStreamDefaultController::Trace(Visitor* visitor) const {
  visitor->Trace(cancel_algorithm_);
  visitor->Trace(controlled_readable_stream_);
  visitor->Trace(pull_algorithm_);
  visitor->Trace(queue_);
  visitor->Trace(strategy_size_algorithm_);
  visitor->Trace(resolve_function_);
  visitor->Trace(reject_function_);
  ScriptWrappable::Trace(visitor);
}

//
// Readable stream default controller internal methods
//

v8::Local<v8::Promise> ReadableStreamDefaultController::CancelSteps(
    ScriptState* script_state,
    v8::Local<v8::Value> reason) {
  // https://streams.spec.whatwg.org/#rs-default-controller-private-cancel
  // 1. Perform ! ResetQueue(this).
  queue_->ResetQueue();

  // 2. Let result be the result of performing this.[[cancelAlgorithm]], passing
  //    reason.
  auto result = cancel_algorithm_->Run(script_state, 1, &reason);

  // 3. Perform ! ReadableStreamDefaultControllerClearAlgorithms(this).
  ClearAlgorithms(this);

  // 4. Return result.
  return result;
}

void ReadableStreamDefaultController::PullSteps(
    ScriptState* script_state,
    ReadRequest* read_request,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-default-controller-private-pull
  // 1. Let stream be this.[[stream]].
  ReadableStream* stream = controlled_readable_stream_;

  // 2. If this.[[queue]] is not empty,
  if (!queue_->IsEmpty()) {
    // a. Let chunk be ! DequeueValue(this).
    const auto chunk = queue_->DequeueValue(script_state->GetIsolate());

    // b. If this.[[closeRequested]] is true and this.[[queue]] is empty,
    if (is_close_requested_ && queue_->IsEmpty()) {
      //   i. Perform ! ReadableStreamDefaultControllerClearAlgorithms(this).
      ClearAlgorithms(this);

      //   ii. Perform ! ReadableStreamClose(stream).
      ReadableStream::Close(script_state, stream);
    } else {
      // c. Otherwise, perform !
      //    ReadableStreamDefaultControllerCallPullIfNeeded(this).
      CallPullIfNeeded(script_state, this);
    }

    // d. Perform readRequest’s chunk steps, given chunk.
    read_request->ChunkSteps(script_state, chunk, exception_state);
    // 3. Otherwise,
  } else {
    // a. Perform ! ReadableStreamAddReadRequest(stream, readRequest).
    ReadableStream::AddReadRequest(script_state, stream, read_request);

    // b. Perform ! ReadableStreamDefaultControllerCallPullIfNeeded(this).
    CallPullIfNeeded(script_state, this);
  }
}

void ReadableStreamDefaultController::ReleaseSteps() {
  // https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaultcontroller-releasesteps
  // 1. Return.
  return;
}

//
// Readable Stream Default Controller Abstract Operations
//

void ReadableStreamDefaultController::CallPullIfNeeded(
    ScriptState* script_state,
    ReadableStreamDefaultController* controller) {
  // https://streams.spec.whatwg.org/#readable-stream-default-controller-call-pull-if-needed
  // 1. Let shouldPull be ! ReadableStreamDefaultControllerShouldCallPull(
  //    controller).
  const bool should_pull = ShouldCallPull(controller);

  // 2. If shouldPull is false, return.
  if (!should_pull) {
    return;
  }

  // 3. If controller.[[pulling]] is true,
  if (controller->is_pulling_) {
    // a. Set controller.[[pullAgain]] to true.
    controller->will_pull_again_ = true;

    // b. Return.
    return;
  }

  // 4. Assert: controller.[[pullAgain]] is false.
  DCHECK(!controller->will_pull_again_);

  // 5. Set controller.[[pulling]] to true.
  controller->is_pulling_ = true;

  // 6. Let pullPromise be the result of performing
  //    controller.[[pullAlgorithm]].
  auto pull_promise =
      controller->pull_algorithm_->Run(script_state, 0, nullptr);

  StreamThenPromise(script_state->GetContext(), pull_promise,
                    controller->resolve_function_,
                    controller->reject_function_);
}

bool ReadableStreamDefaultController::ShouldCallPull(
    const ReadableStreamDefaultController* controller) {
  // https://streams.spec.whatwg.org/#readable-stream-default-controller-should-call-pull
  // 1. Let stream be controller.[[controlledReadableStream]].
  const ReadableStream* stream = controller->controlled_readable_stream_;

  // 2. If ! ReadableStreamDefaultControllerCanCloseOrEnqueue(controller) is
  //    false, return false.
  if (!CanCloseOrEnqueue(controller)) {
    return false;
  }

  // 3. If controller.[[started]] is false, return false.
  if (!controller->is_started_) {
    return false;
  }

  // 4. If ! IsReadableStreamLocked(stream) is true and !
  //    ReadableStreamGetNumReadRequests(stream) > 0, return true.
  if (ReadableStream::IsLocked(stream) &&
      ReadableStream::GetNumReadRequests(stream) > 0) {
    return true;
  }

  // 5. Let desiredSize be ! ReadableStreamDefaultControllerGetDesiredSize
  //    (controller).
  std::optional<double> desired_size = controller->GetDesiredSize();

  // 6. Assert: desiredSize is not null.
  DCHECK(desired_size.has_value());

  // 7. If desiredSize > 0, return true.
  // 8. Return false.
  return desired_size.value() > 0;
}

void ReadableStreamDefaultController::ClearAlgorithms(
    ReadableStreamDefaultController* controller) {
  // https://streams.spec.whatwg.org/#readable-stream-default-controller-clear-algorithms
  // 1. Set controller.[[pullAlgorithm]] to undefined.
  controller->pull_algorithm_ = nullptr;

  // 2. Set controller.[[cancelAlgorithm]] to undefined.
  controller->cancel_algorithm_ = nullptr;

  // 3. Set controller.[[strategySizeAlgorithm]] to undefined.
  controller->strategy_size_algorithm_ = nullptr;
}

void ReadableStreamDefaultController::SetUp(
    ScriptState* script_state,
    ReadableStream* stream,
    ReadableStreamDefaultController* controller,
    StreamStartAlgorithm* start_algorithm,
    StreamAlgorithm* pull_algorithm,
    StreamAlgorithm* cancel_algorithm,
    double high_water_mark,
    StrategySizeAlgorithm* size_algorithm,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller
  // 1. Assert: stream.[[readableStreamController]] is undefined.
  DCHECK(!stream->readable_stream_controller_);

  // 2. Set controller.[[controlledReadableStream]] to stream.
  controller->controlled_readable_stream_ = stream;

  // 3. Set controller.[[queue]] and controller.[[queueTotalSize]] to undefined,
  //    then perform ! ResetQueue(controller).
  // These steps are performed by the constructor, so just check that nothing
  // interfered.
  DCHECK(controller->queue_->IsEmpty());
  DCHECK_EQ(controller->queue_->TotalSize(), 0);

  // 5. Set controller.[[strategySizeAlgorithm]] to sizeAlgorithm and
  //    controller.[[strategyHWM]] to highWaterMark.
  controller->strategy_size_algorithm_ = size_algorithm;
  controller->strategy_high_water_mark_ = high_water_mark;

  // 6. Set controller.[[pullAlgorithm]] to pullAlgorithm.
  controller->pull_algorithm_ = pull_algorithm;

  // 7. Set controller.[[cancelAlgorithm]] to cancelAlgorithm.
  controller->cancel_algorithm_ = cancel_algorithm;

  // 8. Set stream.[[readableStreamController]] to controller.
  stream->readable_stream_controller_ = controller;

  // 9. Let startResult be the result of performing startAlgorithm. (This may
  //    throw an exception.)
  // 10. Let startPromise be a promise resolved with startResult.
  // The conversion of startResult to a promise happens inside start_algorithm
  // in this implementation.
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

  class ResolveFunction final : public PromiseHandler {
   public:
    explicit ResolveFunction(ReadableStreamDefaultController* controller)
        : controller_(controller) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value>) override {
      //  11. Upon fulfillment of startPromise,
      //    a. Set controller.[[started]] to true.
      controller_->is_started_ = true;

      //    b. Assert: controller.[[pulling]] is false.
      DCHECK(!controller_->is_pulling_);

      //    c. Assert: controller.[[pullAgain]] is false.
      DCHECK(!controller_->will_pull_again_);

      //    d. Perform ! ReadableStreamDefaultControllerCallPullIfNeeded(
      //       controller).
      CallPullIfNeeded(script_state, controller_);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(controller_);
      PromiseHandler::Trace(visitor);
    }

   private:
    const Member<ReadableStreamDefaultController> controller_;
  };

  class RejectFunction final : public PromiseHandler {
   public:
    explicit RejectFunction(ReadableStreamDefaultController* controller)
        : controller_(controller) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value> r) override {
      //  12. Upon rejection of startPromise with reason r,
      //    a. Perform ! ReadableStreamDefaultControllerError(controller, r).
      Error(script_state, controller_, r);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(controller_);
      PromiseHandler::Trace(visitor);
    }

   private:
    const Member<ReadableStreamDefaultController> controller_;
  };

  StreamThenPromise(
      script_state->GetContext(), start_promise,
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<ResolveFunction>(controller)),

      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<RejectFunction>(controller)));
}

void ReadableStreamDefaultController::SetUpFromUnderlyingSource(
    ScriptState* script_state,
    ReadableStream* stream,
    v8::Local<v8::Object> underlying_source,
    double high_water_mark,
    StrategySizeAlgorithm* size_algorithm,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#set-up-readable-stream-default-controller-from-underlying-source
  // 2. Let controller be ObjectCreate(the original value of
  //    ReadableStreamDefaultController's prototype property).
  auto* controller =
      MakeGarbageCollected<ReadableStreamDefaultController>(script_state);

  // This method is only called when a WritableStream is being constructed by
  // JavaScript. So the execution context should be valid and this call should
  // not crash.
  auto controller_value = ToV8Traits<ReadableStreamDefaultController>::ToV8(
      script_state, controller);

  // 3. Let startAlgorithm be the following steps:
  //   a. Return ? InvokeOrNoop(underlyingSource, "start", « controller »).
  auto* start_algorithm =
      CreateStartAlgorithm(script_state, underlying_source,
                           "underlyingSource.start", controller_value);

  // 4. Let pullAlgorithm be ? CreateAlgorithmFromUnderlyingMethod
  //    (underlyingSource, "pull", 0, « controller »).
  auto* pull_algorithm = CreateAlgorithmFromUnderlyingMethod(
      script_state, underlying_source, "pull", "underlyingSource.pull",
      controller_value, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 5. Let cancelAlgorithm be ? CreateAlgorithmFromUnderlyingMethod
  //    (underlyingSource, "cancel", 1, « »).
  auto* cancel_algorithm = CreateAlgorithmFromUnderlyingMethod(
      script_state, underlying_source, "cancel", "underlyingSource.cancel",
      controller_value, exception_state);
  if (exception_state.HadException()) {
    return;
  }

  // 6. Perform ? SetUpReadableStreamDefaultController(stream, controller,
  //    startAlgorithm, pullAlgorithm, cancelAlgorithm, highWaterMark,
  //    sizeAlgorithm).
  SetUp(script_state, stream, controller, start_algorithm, pull_algorithm,
        cancel_algorithm, high_water_mark, size_algorithm, exception_state);
}

}  // namespace blink

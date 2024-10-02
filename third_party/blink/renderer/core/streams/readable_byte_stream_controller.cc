// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_byte_stream_controller.h"

#include "base/numerics/checked_math.h"
#include "base/numerics/clamped_math.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_underlying_source.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_underlying_source_cancel_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_underlying_source_pull_callback.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_underlying_source_start_callback.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/promise_handler.h"
#include "third_party/blink/renderer/core/streams/read_into_request.h"
#include "third_party/blink/renderer/core/streams/read_request.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_byob_request.h"
#include "third_party/blink/renderer/core/streams/stream_algorithms.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_data_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

template <typename DOMType>
DOMArrayBufferView* CreateAsArrayBufferView(DOMArrayBuffer* buffer,
                                            size_t byte_offset,
                                            size_t length) {
  return DOMType::Create(buffer, byte_offset, length);
}

}  // namespace

ReadableByteStreamController::QueueEntry::QueueEntry(DOMArrayBuffer* buffer,
                                                     size_t byte_offset,
                                                     size_t byte_length)
    : buffer(buffer), byte_offset(byte_offset), byte_length(byte_length) {}

void ReadableByteStreamController::QueueEntry::Trace(Visitor* visitor) const {
  visitor->Trace(buffer);
}

ReadableByteStreamController::PullIntoDescriptor::PullIntoDescriptor(
    DOMArrayBuffer* buffer,
    size_t buffer_byte_length,
    size_t byte_offset,
    size_t byte_length,
    size_t bytes_filled,
    size_t element_size,
    ViewConstructorType view_constructor,
    ReaderType reader_type)
    : buffer(buffer),
      buffer_byte_length(buffer_byte_length),
      byte_offset(byte_offset),
      byte_length(byte_length),
      bytes_filled(bytes_filled),
      element_size(element_size),
      view_constructor(view_constructor),
      reader_type(reader_type) {}

void ReadableByteStreamController::PullIntoDescriptor::Trace(
    Visitor* visitor) const {
  visitor->Trace(buffer);
}

// This constructor is used internally; it is not reachable from Javascript.
ReadableByteStreamController::ReadableByteStreamController()
    : queue_total_size_(queue_.size()) {}

ReadableStreamBYOBRequest* ReadableByteStreamController::byobRequest() {
  // https://streams.spec.whatwg.org/#rbs-controller-byob-request
  // 1. Return ReadableByteStreamControllerGetBYOBRequest(this).
  return GetBYOBRequest(this);
}

ReadableStreamBYOBRequest* ReadableByteStreamController::GetBYOBRequest(
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontrollergetbyobrequest
  // 1. If controller.[[byobRequest]] is null and
  // controller.[[pendingPullIntos]] is not empty,
  if (!controller->byob_request_ && !controller->pending_pull_intos_.empty()) {
    //   a. Let firstDescriptor be controller.[[pendingPullIntos]][0].
    const PullIntoDescriptor* first_descriptor =
        controller->pending_pull_intos_[0];

    //   b. Let view be ! Construct(%Uint8Array%, « firstDescriptor’s buffer,
    //   firstDescriptor’s byte offset + firstDescriptor’s bytes filled,
    //   firstDescriptor’s byte length − firstDescriptor’s bytes filled »).
    DOMUint8Array* const view = DOMUint8Array::Create(
        first_descriptor->buffer,
        first_descriptor->byte_offset + first_descriptor->bytes_filled,
        first_descriptor->byte_length - first_descriptor->bytes_filled);

    //   c. Let byobRequest be a new ReadableStreamBYOBRequest.
    //   d. Set byobRequest.[[controller]] to controller.
    //   e. Set byobRequest.[[view]] to view.
    //   f. Set controller.[[byobRequest]] to byobRequest.
    controller->byob_request_ = MakeGarbageCollected<ReadableStreamBYOBRequest>(
        controller, NotShared<DOMUint8Array>(view));
  }

  // 2. Return controller.[[byobRequest]].
  return controller->byob_request_.Get();
}

std::optional<double> ReadableByteStreamController::desiredSize() {
  // https://streams.spec.whatwg.org/#rbs-controller-desired-size
  // 1. Return ! ReadableByteStreamControllerGetDesiredSize(this).
  return GetDesiredSize(this);
}

std::optional<double> ReadableByteStreamController::GetDesiredSize(
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-get-desired-size
  // 1. Let state be controller.[[stream]].[[state]].
  switch (controller->controlled_readable_stream_->state_) {
      // 2. If state is "errored", return null.
    case ReadableStream::kErrored:
      return std::nullopt;

      // 3. If state is "closed", return 0.
    case ReadableStream::kClosed:
      return 0.0;

    case ReadableStream::kReadable:
      // 4. Return controller.[[strategyHWM]]] - controller.[[queueTotalSize]].
      return controller->strategy_high_water_mark_ -
             controller->queue_total_size_;
  }
}

void ReadableByteStreamController::close(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rbs-controller-close
  // 1. If this.[[closeRequested]] is true, throw a TypeError exception.
  if (close_requested_) {
    exception_state.ThrowTypeError(
        "Cannot close a readable stream that has already been requested "
        "to be closed");
    return;
  }

  // 2. If this.[[stream]].[[state]] is not "readable", throw a TypeError
  // exception.
  if (controlled_readable_stream_->state_ != ReadableStream::kReadable) {
    exception_state.ThrowTypeError(
        "Cannot close a readable stream that is not readable");
    return;
  }

  // 3. Perform ? ReadableByteStreamControllerClose(this).
  Close(script_state, this);
}

void ReadableByteStreamController::enqueue(ScriptState* script_state,
                                           NotShared<DOMArrayBufferView> chunk,
                                           ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rbs-controller-enqueue
  // 1. If chunk.[[ByteLength]] is 0, throw a TypeError exception.
  if (chunk->byteLength() == 0) {
    exception_state.ThrowTypeError("chunk is empty");
    return;
  }

  // 2. If chunk.[[ViewedArrayBuffer]].[[ArrayBufferByteLength]] is 0, throw a
  // TypeError exception.
  if (chunk->buffer()->ByteLength() == 0) {
    exception_state.ThrowTypeError("chunk's buffer is empty");
    return;
  }

  // 3. If this.[[closeRequested]] is true, throw a TypeError exception.
  if (close_requested_) {
    exception_state.ThrowTypeError("close requested already");
    return;
  }

  // 4. If this.[[stream]].[[state]] is not "readable", throw a TypeError
  // exception.
  if (controlled_readable_stream_->state_ != ReadableStream::kReadable) {
    exception_state.ThrowTypeError("stream is not readable");
    return;
  }

  // 5. Return ! ReadableByteStreamControllerEnqueue(this, chunk).
  Enqueue(script_state, this, chunk, exception_state);
}

void ReadableByteStreamController::error(ScriptState* script_state) {
  error(script_state, ScriptValue(script_state->GetIsolate(),
                                  v8::Undefined(script_state->GetIsolate())));
}

void ReadableByteStreamController::error(ScriptState* script_state,
                                         const ScriptValue& e) {
  // https://streams.spec.whatwg.org/#rbs-controller-error
  // 1. Perform ! ReadableByteStreamControllerError(this, e).
  Error(script_state, this, e.V8Value());
}

void ReadableByteStreamController::Close(
    ScriptState* script_state,
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-close
  // 1. Let stream be controller.[[stream]].
  ReadableStream* const stream = controller->controlled_readable_stream_;

  // 2. If controller.[[closeRequested]] is true or stream.[[state]] is not
  // "readable", return.
  if (controller->close_requested_ ||
      stream->state_ != ReadableStream::kReadable) {
    return;
  }

  // 3. If controller.[[queueTotalSize]] > 0,
  if (controller->queue_total_size_ > 0) {
    //   a. Set controller.[[closeRequested]] to true.
    controller->close_requested_ = true;
    //   b. Return.
    return;
  }

  // 4. If controller.[[pendingPullIntos]] is not empty,
  if (!controller->pending_pull_intos_.empty()) {
    //   a. Let firstPendingPullInto be controller.[[pendingPullIntos]][0].
    const PullIntoDescriptor* first_pending_pull_into =
        controller->pending_pull_intos_[0];
    //   b. If firstPendingPullInto’s bytes filled > 0,
    if (first_pending_pull_into->bytes_filled > 0) {
      //     i. Let e be a new TypeError exception.
      v8::Local<v8::Value> e = V8ThrowException::CreateTypeError(
          script_state->GetIsolate(), "Cannot close while responding");
      //     ii. Perform ! ReadableByteStreamControllerError(controller, e).
      Error(script_state, controller, e);
      //     iii. Throw e.
      V8ThrowException::ThrowException(script_state->GetIsolate(), e);
      return;
    }
  }

  // 5. Perform ! ReadableByteStreamControllerClearAlgorithms(controller).
  ClearAlgorithms(controller);

  // 6. Perform ! ReadableStreamClose(stream).
  ReadableStream::Close(script_state, stream);
}

void ReadableByteStreamController::Error(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    v8::Local<v8::Value> e) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-error
  // 1. Let stream by controller.[[stream]].
  ReadableStream* const stream = controller->controlled_readable_stream_;

  // 2. If stream.[[state]] is not "readable", return.
  if (stream->state_ != ReadableStream::kReadable) {
    return;
  }

  // 3. Perform ! ReadableByteStreamControllerClearPendingPullIntos(controller).
  ClearPendingPullIntos(controller);

  // 4. Perform ! ResetQueue(controller).
  ResetQueue(controller);

  // 5. Perform ! ReadableByteStreamControllerClearAlgorithms(controller).
  ClearAlgorithms(controller);

  // 6. Perform ! ReadableStreamError(stream, e).
  ReadableStream::Error(script_state, stream, e);
}

void ReadableByteStreamController::Enqueue(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    NotShared<DOMArrayBufferView> chunk,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-enqueue
  // 1. Let stream be controller.[[stream]].
  ReadableStream* const stream = controller->controlled_readable_stream_;

  // 2. If controller.[[closeRequested]] is true or stream.[[state]] is not
  // "readable", return.
  if (controller->close_requested_ ||
      stream->state_ != ReadableStream::kReadable) {
    return;
  }

  // 3. Let buffer be chunk.[[ViewedArrayBuffer]].
  DOMArrayBuffer* const buffer = chunk->buffer();

  // 4. Let byteOffset be chunk.[[ByteOffset]].
  const size_t byte_offset = chunk->byteOffset();

  // 5. Let byteLength be chunk.[[ByteLength]].
  const size_t byte_length = chunk->byteLength();

  // 6. If ! IsDetachedBuffer(buffer) is true, throw a TypeError exception.
  if (buffer->IsDetached()) {
    exception_state.ThrowTypeError("buffer is detached");
    return;
  }

  // 7. Let transferredBuffer be ? TransferArrayBuffer(buffer).
  DOMArrayBuffer* const transferred_buffer =
      TransferArrayBuffer(script_state, buffer, exception_state);
  if (!transferred_buffer) {
    return;
  }

  // 8. If controller.[[pendingPullIntos]] is not empty,
  if (!controller->pending_pull_intos_.empty()) {
    //     a. Let firstPendingPullInto be controller.[[pendingPullIntos]][0].
    PullIntoDescriptor* first_pending_pull_into =
        controller->pending_pull_intos_[0];
    //     b. If ! IsDetachedBuffer(firstPendingPullInto's buffer) is true,
    //     throw a TypeError exception.
    if (first_pending_pull_into->buffer->IsDetached()) {
      exception_state.ThrowTypeError("first pending read's buffer is detached");
      return;
    }
    //     c. Perform !
    //     ReadableByteStreamControllerInvalidateBYOBRequest(controller).
    InvalidateBYOBRequest(controller);
    //     d. Set firstPendingPullInto's buffer to ! TransferArrayBuffer(
    //     firstPendingPullInto's buffer).
    first_pending_pull_into->buffer = TransferArrayBuffer(
        script_state, first_pending_pull_into->buffer, exception_state);
    //     e. If firstPendingPullInto’s reader type is "none", perform ?
    //     ReadableByteStreamControllerEnqueueDetachedPullIntoToQueue(controller,
    //     firstPendingPullInto).
    if (first_pending_pull_into->reader_type == ReaderType::kNone) {
      EnqueueDetachedPullIntoToQueue(controller, first_pending_pull_into);
    }
  }

  // 9. If ! ReadableStreamHasDefaultReader(stream) is true
  if (ReadableStream::HasDefaultReader(stream)) {
    //   a. Perform !
    //   ReadableByteStreamControllerProcessReadRequestsUsingQueue(controller).
    ProcessReadRequestsUsingQueue(script_state, controller, exception_state);
    //   b. If ! ReadableStreamGetNumReadRequests(stream) is 0,
    if (ReadableStream::GetNumReadRequests(stream) == 0) {
      //     i. Assert: controller.[[pendingPullIntos]] is empty.
      DCHECK(controller->pending_pull_intos_.empty());

      //     ii. Perform !
      //     ReadableByteStreamControllerEnqueueChunkToQueue(controller,
      //     transferredBuffer, byteOffset, byteLength).
      EnqueueChunkToQueue(controller, transferred_buffer, byte_offset,
                          byte_length);
    } else {
      // c. Otherwise,
      //     i. Assert: controller.[[queue]] is empty.
      DCHECK(controller->queue_.empty());

      //     ii. If controller.[[pendingPullIntos]] is not empty,
      if (!controller->pending_pull_intos_.empty()) {
        //        1. Assert: controller.[[pendingPullIntos]][0]'s reader type is
        //        "default".
        DCHECK_EQ(controller->pending_pull_intos_[0]->reader_type,
                  ReaderType::kDefault);

        //        2. Perform !
        //        ReadableByteStreamControllerShiftPendingPullInto(controller).
        ShiftPendingPullInto(controller);
      }

      //     iii. Let transferredView be ! Construct(%Uint8Array%, «
      //     transferredBuffer, byteOffset, byteLength »).
      v8::Local<v8::Value> const transferred_view = v8::Uint8Array::New(
          ToV8Traits<DOMArrayBuffer>::ToV8(script_state, transferred_buffer)
              .As<v8::ArrayBuffer>(),
          byte_offset, byte_length);
      //     iv. Perform ! ReadableStreamFulfillReadRequest(stream,
      //     transferredView, false).
      ReadableStream::FulfillReadRequest(script_state, stream, transferred_view,
                                         false, exception_state);
    }
  }

  // 10. Otherwise, if ! ReadableStreamHasBYOBReader(stream) is true,
  else if (ReadableStream::HasBYOBReader(stream)) {
    //   a. Perform !
    //   ReadableByteStreamControllerEnqueueChunkToQueue(controller,
    //   transferredBuffer, byteOffset, byteLength).
    EnqueueChunkToQueue(controller, transferred_buffer, byte_offset,
                        byte_length);
    //   b. Perform !
    //   ReadableByteStreamControllerProcessPullIntoDescriptorsUsing
    //   Queue(controller).
    ProcessPullIntoDescriptorsUsingQueue(script_state, controller);
    DCHECK(!exception_state.HadException());
  } else {
    // 11. Otherwise,
    //   a. Assert: ! IsReadableStreamLocked(stream) is false.
    DCHECK(!ReadableStream::IsLocked(stream));
    //   b. Perform !
    //   ReadableByteStreamControllerEnqueueChunkToQueue(controller,
    //   transferredBuffer, byteOffset, byteLength).
    EnqueueChunkToQueue(controller, transferred_buffer, byte_offset,
                        byte_length);
  }

  // 12. Perform ! ReadableByteStreamControllerCallPullIfNeeded(controller).
  CallPullIfNeeded(script_state, controller);
}

void ReadableByteStreamController::EnqueueChunkToQueue(
    ReadableByteStreamController* controller,
    DOMArrayBuffer* buffer,
    size_t byte_offset,
    size_t byte_length) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-enqueue-chunk-to-queue
  // 1. Append a new readable byte stream queue entry with buffer buffer, byte
  // offset byteOffset, and byte length byteLength to controller.[[queue]].
  QueueEntry* const entry =
      MakeGarbageCollected<QueueEntry>(buffer, byte_offset, byte_length);
  controller->queue_.push_back(entry);
  // 2. Set controller.[[queueTotalSize]] to controller.[[queueTotalSize]] +
  // byteLength.
  controller->queue_total_size_ += byte_length;
}

void ReadableByteStreamController::EnqueueClonedChunkToQueue(
    ReadableByteStreamController* controller,
    DOMArrayBuffer* buffer,
    size_t byte_offset,
    size_t byte_length) {
  // https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontrollerenqueueclonedchunktoqueue
  // 1. Let cloneResult be CloneArrayBuffer(buffer, byteOffset, byteLength,
  // %ArrayBuffer%).
  DOMArrayBuffer* const clone_result = DOMArrayBuffer::Create(
    buffer->ByteSpan().subspan(byte_offset, byte_length));
  // 2. If cloneResult is an abrupt completion,
  //   a. Perform ! ReadableByteStreamControllerError(controller,
  //   cloneResult.[[Value]]). b. Return cloneResult.
  // This is not needed as DOMArrayBuffer::Create() is designed to crash if it
  // cannot allocate the memory.

  // 3. Perform ! ReadableByteStreamControllerEnqueueChunkToQueue(controller,
  // cloneResult.[[Value]], 0, byteLength).
  EnqueueChunkToQueue(controller, clone_result, 0, byte_length);
}

void ReadableByteStreamController::EnqueueDetachedPullIntoToQueue(
    ReadableByteStreamController* controller,
    PullIntoDescriptor* pull_into_descriptor) {
  // https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontrollerenqueuedetachedpullintotoqueue
  // Note: EnqueueDetachedPullIntoToQueue cannot throw in this implementation.
  // 1. Assert: pullIntoDescriptor’s reader type is "none".
  DCHECK_EQ(pull_into_descriptor->reader_type, ReaderType::kNone);

  // 2. If pullIntoDescriptor’s bytes filled > 0, perform ?
  // ReadableByteStreamControllerEnqueueClonedChunkToQueue(controller,
  // pullIntoDescriptor’s buffer, pullIntoDescriptor’s byte offset,
  // pullIntoDescriptor’s bytes filled).
  if (pull_into_descriptor->bytes_filled > 0) {
    EnqueueClonedChunkToQueue(controller, pull_into_descriptor->buffer,
                              pull_into_descriptor->byte_offset,
                              pull_into_descriptor->bytes_filled);
  }

  // 3. Perform ! ReadableByteStreamControllerShiftPendingPullInto(controller).
  ShiftPendingPullInto(controller);
}

void ReadableByteStreamController::ProcessPullIntoDescriptorsUsingQueue(
    ScriptState* script_state,
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-process-pull-into-descriptors-using-queue
  // 1. Assert: controller.[[closeRequested]] is false.
  DCHECK(!controller->close_requested_);
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::TryCatch try_catch(isolate);
  // 2. While controller.[[pendingPullIntos]] is not empty,
  while (!controller->pending_pull_intos_.empty()) {
    //   a. If controller.[[queueTotalSize]] is 0, return.
    if (controller->queue_total_size_ == 0) {
      return;
    }
    //   b. Let pullIntoDescriptor be controller.[[pendingPullIntos]][0].
    PullIntoDescriptor* const pull_into_descriptor =
        controller->pending_pull_intos_[0];
    //   c. If ! ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(
    //   controller, pullIntoDescriptor) is true,
    if (FillPullIntoDescriptorFromQueue(controller, pull_into_descriptor,
                                        PassThroughException(isolate))) {
      //     i. Perform !
      //     ReadableByteStreamControllerShiftPendingPullInto(controller).
      ShiftPendingPullInto(controller);
      //     ii. Perform ! ReadableByteStreamControllerCommitPullIntoDescriptor(
      //     controller.[[stream]], pullIntoDescriptor).
      CommitPullIntoDescriptor(
          script_state, controller->controlled_readable_stream_,
          pull_into_descriptor, PassThroughException(isolate));
      DCHECK(!try_catch.HasCaught());
    }
    if (try_catch.HasCaught()) {
      // Instead of returning a rejection, which is inconvenient here,
      // call ControllerError(). The only difference this makes is that it
      // happens synchronously, but that should not be observable.
      ReadableByteStreamController::Error(script_state, controller,
                                          try_catch.Exception());
      return;
    }
  }
}

void ReadableByteStreamController::ProcessReadRequestsUsingQueue(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontrollerprocessreadrequestsusingqueue
  // 1. Let reader be controller.[[stream]].[[reader]].
  ReadableStreamGenericReader* reader =
      controller->controlled_readable_stream_->reader_;
  // 2. Assert: reader implements ReadableStreamDefaultReader.
  DCHECK(reader->IsDefaultReader());
  ReadableStreamDefaultReader* default_reader =
      To<ReadableStreamDefaultReader>(reader);
  // 3. While reader.[[readRequests]] is not empty,
  while (!default_reader->read_requests_.empty()) {
    //   a. If controller.[[queueTotalSize]] is 0, return.
    if (controller->queue_total_size_ == 0) {
      return;
    }
    //   b. Let readRequest be reader.[[readRequests]][0].
    ReadRequest* read_request = default_reader->read_requests_[0];
    //   c. Remove readRequest from reader.[[readRequests]].
    default_reader->read_requests_.pop_front();
    //   d. Perform !
    //   ReadableByteStreamControllerFillReadRequestFromQueue(controller,
    //   readRequest).
    FillReadRequestFromQueue(script_state, controller, read_request,
                             exception_state);
  }
}

void ReadableByteStreamController::CallPullIfNeeded(
    ScriptState* script_state,
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-call-pull-if-needed
  // 1. Let shouldPull be !
  // ReadableByteStreamControllerShouldCallPull(controller).
  const bool should_pull = ShouldCallPull(controller);
  // 2. If shouldPull is false, return.
  if (!should_pull) {
    return;
  }
  // 3. If controller.[[pulling]] is true,
  if (controller->pulling_) {
    //   a. Set controller.[[pullAgain]] to true.
    controller->pull_again_ = true;
    //   b. Return.
    return;
  }
  // 4. Assert: controller.[[pullAgain]] is false.
  DCHECK(!controller->pull_again_);
  // 5. Set controller.[[pulling]] to true.
  controller->pulling_ = true;
  // 6. Let pullPromise be the result of performing
  // controller.[[pullAlgorithm]].
  auto pull_promise =
      controller->pull_algorithm_->Run(script_state, 0, nullptr);

  class ResolveFunction final : public PromiseHandler {
   public:
    explicit ResolveFunction(ReadableByteStreamController* controller)
        : controller_(controller) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value>) override {
      // 7. Upon fulfillment of pullPromise,
      //   a. Set controller.[[pulling]] to false.
      controller_->pulling_ = false;
      //   b. If controller.[[pullAgain]] is true,
      if (controller_->pull_again_) {
        //     i. Set controller.[[pullAgain]] to false.
        controller_->pull_again_ = false;
        //     ii. Perform !
        //     ReadableByteStreamControllerCallPullIfNeeded(controller).
        CallPullIfNeeded(script_state, controller_);
      }
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(controller_);
      PromiseHandler::Trace(visitor);
    }

   private:
    const Member<ReadableByteStreamController> controller_;
  };

  class RejectFunction final : public PromiseHandler {
   public:
    explicit RejectFunction(ReadableByteStreamController* controller)
        : controller_(controller) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value> e) override {
      // 8. Upon rejection of pullPromise with reason e,
      //   a. Perform ! ReadableByteStreamControllerError(controller, e).
      Error(script_state, controller_, e);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(controller_);
      PromiseHandler::Trace(visitor);
    }

   private:
    const Member<ReadableByteStreamController> controller_;
  };

  StreamThenPromise(
      script_state->GetContext(), pull_promise,
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<ResolveFunction>(controller)),
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<RejectFunction>(controller)));
}

ReadableByteStreamController::PullIntoDescriptor*
ReadableByteStreamController::ShiftPendingPullInto(
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-shift-pending-pull-into
  // 1. Assert: controller.[[byobRequest]] is null.
  DCHECK(!controller->byob_request_);
  // 2. Let descriptor be controller.[[pendingPullIntos]][0].
  PullIntoDescriptor* const descriptor = controller->pending_pull_intos_[0];
  // 3. Remove descriptor from controller.[[pendingPullIntos]].
  controller->pending_pull_intos_.pop_front();
  // 4. Return descriptor.
  return descriptor;
}

bool ReadableByteStreamController::ShouldCallPull(
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-should-call-pull
  // 1. Let stream be controller.[[stream]].
  ReadableStream* const stream = controller->controlled_readable_stream_;
  // 2. If stream.[[state]] is not "readable", return false.
  if (stream->state_ != ReadableStream::kReadable) {
    return false;
  }
  // 3. If controller.[[closeRequested]] is true, return false.
  if (controller->close_requested_) {
    return false;
  }
  // 4. If controller.[[started]] is false, return false.
  if (!controller->started_) {
    return false;
  }
  // 5. If ! ReadableStreamHasDefaultReader(stream) is true and !
  // ReadableStreamGetNumReadRequests(stream) > 0, return true.
  if (ReadableStream::HasDefaultReader(stream) &&
      ReadableStream::GetNumReadRequests(stream) > 0) {
    return true;
  }
  // 6. If ! ReadableStreamHasBYOBReader(stream) is true and !
  // ReadableStreamGetNumReadIntoRequests(stream) > 0, return true.
  if (ReadableStream::HasBYOBReader(stream) &&
      ReadableStream::GetNumReadIntoRequests(stream) > 0) {
    return true;
  }
  // 7. Let desiredSize be !
  // ReadableByteStreamControllerGetDesiredSize(controller).
  const std::optional<double> desired_size = GetDesiredSize(controller);
  // 8. Assert: desiredSize is not null.
  DCHECK(desired_size);
  // 9. If desiredSize > 0, return true.
  if (*desired_size > 0) {
    return true;
  }
  // 10. Return false.
  return false;
}

void ReadableByteStreamController::CommitPullIntoDescriptor(
    ScriptState* script_state,
    ReadableStream* stream,
    PullIntoDescriptor* pull_into_descriptor,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-commit-pull-into-descriptor
  // 1. Assert: stream.[[state]] is not "errored".
  DCHECK_NE(stream->state_, ReadableStream::kErrored);
  // 2. Assert: pullIntoDescriptor.reader type is not "none".
  DCHECK_NE(pull_into_descriptor->reader_type, ReaderType::kNone);
  // 3. Let done be false.
  bool done = false;
  // 4. If stream.[[state]] is "closed",
  if (stream->state_ == ReadableStream::kClosed) {
    //   a. Assert: pullIntoDescriptor’s bytes filled is 0.
    DCHECK_EQ(pull_into_descriptor->bytes_filled, 0u);
    //   b. Set done to true.
    done = true;
  }
  // 5. Let filledView be !
  // ReadableByteStreamControllerConvertPullIntoDescriptor(pullIntoDescriptor).
  auto* filled_view = ConvertPullIntoDescriptor(
      script_state, pull_into_descriptor, exception_state);
  DCHECK(!exception_state.HadException());
  // 6. If pullIntoDescriptor’s reader type is "default",
  if (pull_into_descriptor->reader_type == ReaderType::kDefault) {
    //   a. Perform ! ReadableStreamFulfillReadRequest(stream, filledView,
    //   done).
    ReadableStream::FulfillReadRequest(
        script_state, stream,
        ToV8Traits<DOMArrayBufferView>::ToV8(script_state, filled_view), done,
        exception_state);
  } else {
    // 7. Otherwise,
    //   a. Assert: pullIntoDescriptor’s reader type is "byob".
    DCHECK_EQ(pull_into_descriptor->reader_type, ReaderType::kBYOB);
    //   b. Perform ! ReadableStreamFulfillReadIntoRequest(stream, filledView,
    //   done).
    ReadableStream::FulfillReadIntoRequest(script_state, stream, filled_view,
                                           done, exception_state);
  }
}

DOMArrayBufferView* ReadableByteStreamController::ConvertPullIntoDescriptor(
    ScriptState* script_state,
    PullIntoDescriptor* pull_into_descriptor,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-convert-pull-into-descriptor
  // 1. Let bytesFilled be pullIntoDescriptor’s bytes filled.
  const size_t bytes_filled = pull_into_descriptor->bytes_filled;
  // 2. Let elementSize be pullIntoDescriptor’s element size.
  const size_t element_size = pull_into_descriptor->element_size;
  // 3. Assert: bytesFilled ≤ pullIntoDescriptor’s byte length.
  DCHECK_LE(bytes_filled, pull_into_descriptor->byte_length);
  // 4. Assert: bytesFilled mod elementSize is 0.
  DCHECK_EQ(bytes_filled % element_size, 0u);
  // 5. Let buffer be ! TransferArrayBuffer(pullIntoDescriptor's buffer).
  DOMArrayBuffer* const buffer = TransferArrayBuffer(
      script_state, pull_into_descriptor->buffer, exception_state);
  // 6. Return ! Construct(pullIntoDescriptor’s view constructor, « buffer,
  // pullIntoDescriptor’s byte offset, bytesFilled ÷ elementSize »).
  return pull_into_descriptor->view_constructor(
      buffer, pull_into_descriptor->byte_offset, (bytes_filled / element_size));
}

void ReadableByteStreamController::ClearPendingPullIntos(
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-clear-pending-pull-intos
  // 1. Perform ! ReadableByteStreamControllerInvalidateBYOBRequest(controller).
  InvalidateBYOBRequest(controller);
  // 2. Set controller.[[pendingPullIntos]] to a new empty list.
  controller->pending_pull_intos_.clear();
}

void ReadableByteStreamController::ClearAlgorithms(
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-clear-algorithms
  // 1. Set controller.[[pullAlgorithm]] to undefined.
  controller->pull_algorithm_ = nullptr;

  // 2. Set controller.[[cancelAlgorithm]] to undefined.
  controller->cancel_algorithm_ = nullptr;
}

void ReadableByteStreamController::InvalidateBYOBRequest(
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-invalidate-byob-request
  // 1. If controller.[[byobRequest]] is null, return.
  if (!controller->byob_request_) {
    return;
  }
  // 2. Set controller.[[byobRequest]].[[controller]] to undefined.
  controller->byob_request_->controller_ = nullptr;
  // 3. Set controller.[[byobRequest]].[[view]] to null.
  controller->byob_request_->view_ = NotShared<DOMArrayBufferView>(nullptr);
  // 4. Set controller.[[byobRequest]] to null.
  controller->byob_request_ = nullptr;
}

void ReadableByteStreamController::SetUp(
    ScriptState* script_state,
    ReadableStream* stream,
    ReadableByteStreamController* controller,
    StreamStartAlgorithm* start_algorithm,
    StreamAlgorithm* pull_algorithm,
    StreamAlgorithm* cancel_algorithm,
    double high_water_mark,
    size_t auto_allocate_chunk_size,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#set-up-readable-byte-stream-controller
  // 1. Assert: stream.[[controller]] is undefined.
  DCHECK(!stream->readable_stream_controller_);
  // 2. If autoAllocateChunkSize is not undefined,
  if (auto_allocate_chunk_size) {
    //   a. Assert: ! IsInteger(autoAllocateChunkSize) is true.
    //   b. Assert: autoAllocateChunkSize is positive.
    //   Due to autoAllocateChunkSize having the [EnforceRange] attribute, it
    //   can never be negative.
    DCHECK_GT(auto_allocate_chunk_size, 0u);
  }
  // 3. Set controller.[[stream]] to stream.
  controller->controlled_readable_stream_ = stream;
  // 4. Set controller.[[pullAgain]] and controller.[[pulling]] to false.
  DCHECK(!controller->pull_again_);
  DCHECK(!controller->pulling_);
  // 5. Set controller.[[byobRequest]] to null.
  DCHECK(!controller->byob_request_);
  // 6. Perform ! ResetQueue(controller).
  ResetQueue(controller);
  // 7. Set controller.[[closeRequested]] and controller.[[started]] to false.
  DCHECK(!controller->close_requested_);
  DCHECK(!controller->started_);
  // 8. Set controller.[[strategyHWM]] to highWaterMark.
  controller->strategy_high_water_mark_ = high_water_mark;
  // 9. Set controller.[[pullAlgorithm]] to pullAlgorithm.
  controller->pull_algorithm_ = pull_algorithm;
  // 10. Set controller.[[cancelAlgorithm]] to cancelAlgorithm.
  controller->cancel_algorithm_ = cancel_algorithm;
  // 11. Set controller.[[autoAllocateChunkSize]] to autoAllocateChunkSize.
  controller->auto_allocate_chunk_size_ = auto_allocate_chunk_size;
  // 12. Set controller.[[pendingPullIntos]] to a new empty list.
  DCHECK(controller->pending_pull_intos_.empty());
  // 13. Set stream.[[controller]] to controller.
  stream->readable_stream_controller_ = controller;
  // 14. Let startResult be the result of performing startAlgorithm.
  // 15. Let startPromise be a promise resolved with startResult.
  // The conversion of startResult to a promise happens inside start_algorithm
  // in this implementation.
  v8::Local<v8::Promise> start_promise;
  if (!start_algorithm->Run(script_state, exception_state)
           .ToLocal(&start_promise)) {
    if (!exception_state.HadException()) {
      exception_state.ThrowException(
          static_cast<int>(DOMExceptionCode::kInvalidStateError),
          "start algorithm failed with no exception thrown");
    }
    return;
  }
  DCHECK(!exception_state.HadException());

  class ResolveFunction final : public PromiseHandler {
   public:
    explicit ResolveFunction(ReadableByteStreamController* controller)
        : controller_(controller) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value>) override {
      // 16. Upon fulfillment of startPromise,
      //   a. Set controller.[[started]] to true.
      controller_->started_ = true;
      //   b. Assert: controller.[[pulling]] is false.
      DCHECK(!controller_->pulling_);
      //   c. Assert: controller.[[pullAgain]] is false.
      DCHECK(!controller_->pull_again_);
      //   d. Perform !
      //   ReadableByteStreamControllerCallPullIfNeeded(controller).
      CallPullIfNeeded(script_state, controller_);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(controller_);
      PromiseHandler::Trace(visitor);
    }

   private:
    const Member<ReadableByteStreamController> controller_;
  };

  class RejectFunction final : public PromiseHandler {
   public:
    explicit RejectFunction(ReadableByteStreamController* controller)
        : controller_(controller) {}

    void CallWithLocal(ScriptState* script_state,
                       v8::Local<v8::Value> r) override {
      // 17. Upon rejection of startPromise with reason r,
      //   a. Perform ! ReadableByteStreamControllerError(controller, r).
      Error(script_state, controller_, r);
    }

    void Trace(Visitor* visitor) const override {
      visitor->Trace(controller_);
      PromiseHandler::Trace(visitor);
    }

   private:
    const Member<ReadableByteStreamController> controller_;
  };

  StreamThenPromise(
      script_state->GetContext(), start_promise,
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<ResolveFunction>(controller)),
      MakeGarbageCollected<ScriptFunction>(
          script_state, MakeGarbageCollected<RejectFunction>(controller)));
}

void ReadableByteStreamController::SetUpFromUnderlyingSource(
    ScriptState* script_state,
    ReadableStream* stream,
    v8::Local<v8::Object> underlying_source,
    UnderlyingSource* underlying_source_dict,
    double high_water_mark,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#set-up-readable-byte-stream-controller-from-underlying-source
  // 1. Let controller be a new ReadableByteStreamController.
  ReadableByteStreamController* controller =
      MakeGarbageCollected<ReadableByteStreamController>();
  // 2. Let startAlgorithm be an algorithm that returns undefined.
  StreamStartAlgorithm* start_algorithm = CreateTrivialStartAlgorithm();
  // 3. Let pullAlgorithm be an algorithm that returns a promise resolved with
  // undefined.
  StreamAlgorithm* pull_algorithm = CreateTrivialStreamAlgorithm();
  // 4. Let cancelAlgorithm be an algorithm that returns a promise resolved with
  // undefined.
  StreamAlgorithm* cancel_algorithm = CreateTrivialStreamAlgorithm();

  const auto controller_value =
      ToV8Traits<ReadableByteStreamController>::ToV8(script_state, controller);
  // 5. If underlyingSourceDict["start"] exists, then set startAlgorithm to an
  // algorithm which returns the result of invoking
  // underlyingSourceDict["start"] with argument list « controller » and
  // callback this value underlyingSource.
  if (underlying_source_dict->hasStart()) {
    start_algorithm = CreateByteStreamStartAlgorithm(
        script_state, underlying_source,
        ToV8Traits<V8UnderlyingSourceStartCallback>::ToV8(
            script_state, underlying_source_dict->start()),
        controller_value);
  }
  // 6. If underlyingSourceDict["pull"] exists, then set pullAlgorithm to an
  // algorithm which returns the result of invoking underlyingSourceDict["pull"]
  // with argument list « controller » and callback this value underlyingSource.
  if (underlying_source_dict->hasPull()) {
    pull_algorithm = CreateAlgorithmFromResolvedMethod(
        script_state, underlying_source,
        ToV8Traits<V8UnderlyingSourcePullCallback>::ToV8(
            script_state, underlying_source_dict->pull()),
        controller_value);
  }
  // 7. If underlyingSourceDict["cancel"] exists, then set cancelAlgorithm to an
  // algorithm which takes an argument reason and returns the result of invoking
  // underlyingSourceDict["cancel"] with argument list « reason » and callback
  // this value underlyingSource.
  if (underlying_source_dict->hasCancel()) {
    cancel_algorithm = CreateAlgorithmFromResolvedMethod(
        script_state, underlying_source,
        ToV8Traits<V8UnderlyingSourceCancelCallback>::ToV8(
            script_state, underlying_source_dict->cancel()),
        controller_value);
  }
  // 8. Let autoAllocateChunkSize be
  // underlyingSourceDict["autoAllocateChunkSize"], if it exists, or undefined
  // otherwise.
  size_t auto_allocate_chunk_size =
      underlying_source_dict->hasAutoAllocateChunkSize()
          ? static_cast<size_t>(underlying_source_dict->autoAllocateChunkSize())
          : 0u;
  // 9. If autoAllocateChunkSize is 0, then throw a TypeError exception.
  if (underlying_source_dict->hasAutoAllocateChunkSize() &&
      auto_allocate_chunk_size == 0) {
    exception_state.ThrowTypeError("autoAllocateChunkSize cannot be 0");
    return;
  }
  // 10. Perform ? SetUpReadableByteStreamController(stream, controller,
  // startAlgorithm, pullAlgorithm, cancelAlgorithm, highWaterMark,
  // autoAllocateChunkSize).
  SetUp(script_state, stream, controller, start_algorithm, pull_algorithm,
        cancel_algorithm, high_water_mark, auto_allocate_chunk_size,
        exception_state);
}

void ReadableByteStreamController::FillHeadPullIntoDescriptor(
    ReadableByteStreamController* controller,
    size_t size,
    PullIntoDescriptor* pull_into_descriptor) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-fill-head-pull-into-descriptor
  // 1. Assert: either controller.[[pendingPullIntos]] is empty, or
  // controller.[[pendingPullIntos]][0] is pullIntoDescriptor.
  DCHECK(controller->pending_pull_intos_.empty() ||
         controller->pending_pull_intos_[0] == pull_into_descriptor);
  // 2. Assert: controller.[[byobRequest]] is null.
  DCHECK(!controller->byob_request_);
  // 3. Set pullIntoDescriptor’s bytes filled to bytes filled + size.
  pull_into_descriptor->bytes_filled =
      base::CheckAdd(pull_into_descriptor->bytes_filled, size).ValueOrDie();
}

bool ReadableByteStreamController::FillPullIntoDescriptorFromQueue(
    ReadableByteStreamController* controller,
    PullIntoDescriptor* pull_into_descriptor,
    ExceptionState& exception_state) {
  if (pull_into_descriptor->buffer->IsDetached()) {
    exception_state.ThrowTypeError("buffer is detached");
    return false;
  }
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-fill-pull-into-descriptor-from-queue
  // 1. Let elementSize be pullIntoDescriptor.[[elementSize]].
  const size_t element_size = pull_into_descriptor->element_size;
  // 2. Let currentAlignedBytes be pullIntoDescriptor's bytes filled −
  // (pullIntoDescriptor's bytes filled mod elementSize).
  const size_t current_aligned_bytes =
      pull_into_descriptor->bytes_filled -
      (pull_into_descriptor->bytes_filled % element_size);
  // 3. Let maxBytesToCopy be min(controller.[[queueTotalSize]],
  // pullIntoDescriptor’s byte length − pullIntoDescriptor’s bytes filled).
  // The subtraction will not underflow because bytes length will always be more
  // than or equal to bytes filled.
  const size_t max_bytes_to_copy = std::min(
      static_cast<size_t>(controller->queue_total_size_),
      pull_into_descriptor->byte_length - pull_into_descriptor->bytes_filled);
  // 4. Let maxBytesFilled be pullIntoDescriptor’s bytes filled +
  // maxBytesToCopy.
  // This addition will not overflow because maxBytesToCopy can be at most
  // queue_total_size_. Both bytes_filled and queue_total_size_ refer to
  // actually allocated memory, so together they cannot exceed size_t.
  const size_t max_bytes_filled =
      pull_into_descriptor->bytes_filled + max_bytes_to_copy;
  // 5. Let maxAlignedBytes be maxBytesFilled − (maxBytesFilled mod
  // elementSize).
  // This subtraction will not underflow because the modulus operator is
  // guaranteed to return a value less than or equal to the first argument.
  const size_t max_aligned_bytes =
      max_bytes_filled - (max_bytes_filled % element_size);
  // 6. Let totalBytesToCopyRemaining be maxBytesToCopy.
  size_t total_bytes_to_copy_remaining = max_bytes_to_copy;
  // 7. Let ready be false;
  bool ready = false;
  // 8. If maxAlignedBytes > currentAlignedBytes,
  if (max_aligned_bytes > current_aligned_bytes) {
    // a. Set totalBytesToCopyRemaining to maxAlignedBytes −
    // pullIntoDescriptor’s bytes filled.
    total_bytes_to_copy_remaining =
        base::CheckSub(max_aligned_bytes, pull_into_descriptor->bytes_filled)
            .ValueOrDie();
    // b. Set ready to true.
    ready = true;
  }
  // 9. Let queue be controller.[[queue]].
  HeapDeque<Member<QueueEntry>>& queue = controller->queue_;
  // 10. While totalBytesToCopyRemaining > 0,
  while (total_bytes_to_copy_remaining > 0) {
    // a. Let headOfQueue be queue[0].
    QueueEntry* head_of_queue = queue[0];
    // b. Let bytesToCopy be min(totalBytesToCopyRemaining,
    // headOfQueue’s byte length).
    size_t bytes_to_copy =
        std::min(total_bytes_to_copy_remaining, head_of_queue->byte_length);
    // c. Let destStart be pullIntoDescriptor’s byte offset +
    // pullIntoDescriptor’s bytes filled.
    // This addition will not overflow because byte offset and bytes filled
    // refer to actually allocated memory, so together they cannot exceed
    // size_t.
    size_t dest_start =
        pull_into_descriptor->byte_offset + pull_into_descriptor->bytes_filled;
    // d. Perform ! CopyDataBlockBytes(pullIntoDescriptor’s
    // buffer.[[ArrayBufferData]], destStart, headOfQueue’s
    // buffer.[[ArrayBufferData]], headOfQueue’s byte offset, bytesToCopy).
    auto copy_destination = pull_into_descriptor->buffer->ByteSpan().subspan(
        dest_start, bytes_to_copy);
    auto copy_source = head_of_queue->buffer->ByteSpan().subspan(
        head_of_queue->byte_offset, bytes_to_copy);
    copy_destination.copy_from(copy_source);
    // e. If headOfQueue’s byte length is bytesToCopy,
    if (head_of_queue->byte_length == bytes_to_copy) {
      //   i. Remove queue[0].
      queue.pop_front();
    } else {
      // f. Otherwise,
      //   i. Set headOfQueue’s byte offset to headOfQueue’s byte offset +
      //   bytesToCopy.
      head_of_queue->byte_offset =
          base::CheckAdd(head_of_queue->byte_offset, bytes_to_copy)
              .ValueOrDie();
      //   ii. Set headOfQueue’s byte length to headOfQueue’s byte
      //   length − bytesToCopy.
      head_of_queue->byte_length =
          base::CheckSub(head_of_queue->byte_length, bytes_to_copy)
              .ValueOrDie();
    }
    // g. Set controller.[[queueTotalSize]] to controller.[[queueTotalSize]] −
    // bytesToCopy.
    controller->queue_total_size_ =
        base::CheckSub(controller->queue_total_size_, bytes_to_copy)
            .ValueOrDie();
    // h. Perform !
    // ReadableByteStreamControllerFillHeadPullIntoDescriptor(controller,
    // bytesToCopy, pullIntoDescriptor).
    FillHeadPullIntoDescriptor(controller, bytes_to_copy, pull_into_descriptor);
    // i. Set totalBytesToCopyRemaining to totalBytesToCopyRemaining −
    // bytesToCopy.
    // This subtraction will not underflow because bytes_to_copy will always be
    // greater than or equal to total_bytes_to_copy_remaining.
    total_bytes_to_copy_remaining -= bytes_to_copy;
  }
  // 11. If ready is false,
  if (!ready) {
    // a. Assert: controller.[[queueTotalSize]] is 0.
    DCHECK_EQ(controller->queue_total_size_, 0u);
    // b. Assert: pullIntoDescriptor’s bytes filled > 0.
    DCHECK_GT(pull_into_descriptor->bytes_filled, 0.0);
    // c. Assert: pullIntoDescriptor’s bytes filled < pullIntoDescriptor’s
    // element size.
    DCHECK_LT(pull_into_descriptor->bytes_filled,
              pull_into_descriptor->element_size);
  }
  // 12. Return ready.
  return ready;
}

void ReadableByteStreamController::FillReadRequestFromQueue(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    ReadRequest* read_request,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontrollerfillreadrequestfromqueue
  // 1. Assert: controller.[[queueTotalSize]] > 0.
  DCHECK_GT(controller->queue_total_size_, 0);
  // 2. Let entry be controller.[[queue]][0].
  QueueEntry* entry = controller->queue_[0];
  // 3. Remove entry from controller.[[queue]].
  controller->queue_.pop_front();
  // 4. Set controller.[[queueTotalSize]] to controller.[[queueTotalSize]] −
  // entry’s byte length.
  controller->queue_total_size_ -= entry->byte_length;
  // 5. Perform ! ReadableByteStreamControllerHandleQueueDrain(controller).
  HandleQueueDrain(script_state, controller);
  // 6. Let view be ! Construct(%Uint8Array%, « entry’s buffer, entry’s byte
  // offset, entry’s byte length »).
  DOMUint8Array* view = DOMUint8Array::Create(entry->buffer, entry->byte_offset,
                                              entry->byte_length);
  // 7. Perform readRequest’s chunk steps, given view.
  read_request->ChunkSteps(script_state,
                           ToV8Traits<DOMUint8Array>::ToV8(script_state, view),
                           exception_state);
}

void ReadableByteStreamController::PullInto(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    NotShared<DOMArrayBufferView> view,
    ReadIntoRequest* read_into_request,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-pull-into
  // 1. Let stream be controller.[[stream]].
  ReadableStream* const stream = controller->controlled_readable_stream_;
  // 2. Let elementSize be 1.
  size_t element_size = 1;
  // 3. Let ctor be %DataView%.
  auto* ctor = &CreateAsArrayBufferView<DOMDataView>;
  // 4. If view has a [[TypedArrayName]] internal slot (i.e., it is not a
  // DataView),
  if (view->GetType() != DOMArrayBufferView::kTypeDataView) {
    //   a. Set elementSize to be the element size specified in the typed array
    //   constructors table for view.[[TypedArrayName]].
    element_size = view->TypeSize();
    //   b. Set ctor to the constructor specified in the typed array
    //   constructors table for view.[[TypedArrayName]].
    switch (view->GetType()) {
      case DOMArrayBufferView::kTypeInt8:
        ctor = &CreateAsArrayBufferView<DOMInt8Array>;
        break;
      case DOMArrayBufferView::kTypeUint8:
        ctor = &CreateAsArrayBufferView<DOMUint8Array>;
        break;
      case DOMArrayBufferView::kTypeUint8Clamped:
        ctor = &CreateAsArrayBufferView<DOMUint8ClampedArray>;
        break;
      case DOMArrayBufferView::kTypeInt16:
        ctor = &CreateAsArrayBufferView<DOMInt16Array>;
        break;
      case DOMArrayBufferView::kTypeUint16:
        ctor = &CreateAsArrayBufferView<DOMUint16Array>;
        break;
      case DOMArrayBufferView::kTypeInt32:
        ctor = &CreateAsArrayBufferView<DOMInt32Array>;
        break;
      case DOMArrayBufferView::kTypeUint32:
        ctor = &CreateAsArrayBufferView<DOMUint32Array>;
        break;
      case DOMArrayBufferView::kTypeFloat16:
        ctor = &CreateAsArrayBufferView<DOMFloat16Array>;
        break;
      case DOMArrayBufferView::kTypeFloat32:
        ctor = &CreateAsArrayBufferView<DOMFloat32Array>;
        break;
      case DOMArrayBufferView::kTypeFloat64:
        ctor = &CreateAsArrayBufferView<DOMFloat64Array>;
        break;
      case DOMArrayBufferView::kTypeBigInt64:
        ctor = &CreateAsArrayBufferView<DOMBigInt64Array>;
        break;
      case DOMArrayBufferView::kTypeBigUint64:
        ctor = &CreateAsArrayBufferView<DOMBigUint64Array>;
        break;
      case DOMArrayBufferView::kTypeDataView:
        NOTREACHED_IN_MIGRATION();
    }
  }
  // 5. Let byteOffset be view.[[ByteOffset]].
  const size_t byte_offset = view->byteOffset();
  // 6. Let byteLength be view.[[ByteLength]].
  const size_t byte_length = view->byteLength();
  // 7. Let bufferResult be TransferArrayBuffer(view.[[ViewedArrayBuffer]]).
  DOMArrayBuffer* buffer = nullptr;
  {
    v8::TryCatch try_catch(script_state->GetIsolate());
    buffer =
        TransferArrayBuffer(script_state, view->buffer(),
                            PassThroughException(script_state->GetIsolate()));
    // 8. If bufferResult is an abrupt completion,
    if (try_catch.HasCaught()) {
      //  a. Perform readIntoRequest's error steps, given
      //     bufferResult.[[Value]].
      read_into_request->ErrorSteps(script_state, try_catch.Exception());
      //  b. Return.
      return;
    }
  }
  // 9. Let buffer be bufferResult.[[Value]].

  // 10. Let pullIntoDescriptor be a new pull-into descriptor with buffer
  // buffer, buffer byte length buffer.[[ArrayBufferByteLength]], byte offset
  // byteOffset, byte length byteLength, bytes filled 0, element size
  // elementSize, view constructor ctor, and reader type "byob".
  PullIntoDescriptor* pull_into_descriptor =
      MakeGarbageCollected<PullIntoDescriptor>(
          buffer, buffer->ByteLength(), byte_offset, byte_length, 0,
          element_size, ctor, ReaderType::kBYOB);
  // 11. If controller.[[pendingPullIntos]] is not empty,
  if (!controller->pending_pull_intos_.empty()) {
    //   a. Append pullIntoDescriptor to controller.[[pendingPullIntos]].
    controller->pending_pull_intos_.push_back(pull_into_descriptor);
    //   b. Perform ! ReadableStreamAddReadIntoRequest(stream, readIntoRequest).
    ReadableStream::AddReadIntoRequest(script_state, stream, read_into_request);
    //   c. Return.
    return;
  }
  // 12. If stream.[[state]] is "closed",
  if (stream->state_ == ReadableStream::kClosed) {
    //   a. Let emptyView be ! Construct(ctor, « pullIntoDescriptor’s buffer,
    //   pullIntoDescriptor’s byte offset, 0 »).
    DOMArrayBufferView* emptyView = ctor(pull_into_descriptor->buffer,
                                         pull_into_descriptor->byte_offset, 0);
    //   b. Perform readIntoRequest’s close steps, given emptyView.
    read_into_request->CloseSteps(script_state, emptyView);
    //   c. Return.
    return;
  }
  // 13. If controller.[[queueTotalSize]] > 0,
  if (controller->queue_total_size_ > 0) {
    //   a. If !
    //   ReadableByteStreamControllerFillPullIntoDescriptorFromQueue(controller,
    //   pullIntoDescriptor) is true,
    v8::TryCatch try_catch(script_state->GetIsolate());
    if (FillPullIntoDescriptorFromQueue(
            controller, pull_into_descriptor,
            PassThroughException(script_state->GetIsolate()))) {
      //     i. Let filledView be !
      //     ReadableByteStreamControllerConvertPullIntoDescriptor(pullIntoDescriptor).
      DOMArrayBufferView* filled_view = ConvertPullIntoDescriptor(
          script_state, pull_into_descriptor,
          PassThroughException(script_state->GetIsolate()));
      DCHECK(!try_catch.HasCaught());
      //     ii. Perform !
      //     ReadableByteStreamControllerHandleQueueDrain(controller).
      HandleQueueDrain(script_state, controller);
      //     iii. Perform readIntoRequest’s chunk steps, given filledView.
      read_into_request->ChunkSteps(script_state, filled_view, exception_state);
      //     iv. Return.
      return;
    }
    if (try_catch.HasCaught()) {
      // Instead of returning a rejection, which is inconvenient here,
      // call ControllerError(). The only difference this makes is that it
      // happens synchronously, but that should not be observable.
      ReadableByteStreamController::Error(script_state, controller,
                                          try_catch.Exception());
      return;
    }
    //   b. If controller.[[closeRequested]] is true,
    if (controller->close_requested_) {
      //     i. Let e be a TypeError exception.
      v8::Local<v8::Value> e = V8ThrowException::CreateTypeError(
          script_state->GetIsolate(), "close requested");
      //     ii. Perform ! ReadableByteStreamControllerError(controller, e).
      controller->Error(script_state, controller, e);
      //     iii. Perform readIntoRequest’s error steps, given e.
      read_into_request->ErrorSteps(script_state, e);
      //     iv. Return.
      return;
    }
  }
  // 14. Append pullIntoDescriptor to controller.[[pendingPullIntos]].
  controller->pending_pull_intos_.push_back(pull_into_descriptor);
  // 15. Perform ! ReadableStreamAddReadIntoRequest(stream, readIntoRequest).
  ReadableStream::AddReadIntoRequest(script_state, stream, read_into_request);
  // 16. Perform ! ReadableByteStreamControllerCallPullIfNeeded(controller).
  CallPullIfNeeded(script_state, controller);
}

void ReadableByteStreamController::HandleQueueDrain(
    ScriptState* script_state,
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-handle-queue-drain
  // 1. Assert: controller.[[stream]].[[state]] is "readable".
  DCHECK_EQ(controller->controlled_readable_stream_->state_,
            ReadableStream::kReadable);
  // 2. If controller.[[queueTotalSize]] is 0 and controller.[[closeRequested]]
  // is true,
  if (!controller->queue_total_size_ && controller->close_requested_) {
    //   a. Perform ! ReadableByteStreamControllerClearAlgorithms(controller).
    ClearAlgorithms(controller);
    //   b. Perform ! ReadableStreamClose(controller.[[stream]]).
    ReadableStream::Close(script_state,
                          controller->controlled_readable_stream_);
  } else {
    // 3. Otherwise,
    //   a. Perform ! ReadableByteStreamControllerCallPullIfNeeded(controller).
    CallPullIfNeeded(script_state, controller);
  }
}

void ReadableByteStreamController::ResetQueue(
    ReadableByteStreamController* controller) {
  // https://streams.spec.whatwg.org/#reset-queue
  // 1. Assert: container has [[queue]] and [[queueTotalSize]] internal slots.
  // 2. Set container.[[queue]] to a new empty list.
  controller->queue_.clear();
  // 3. Set container.[[queueTotalSize]] to 0.
  controller->queue_total_size_ = 0;
}

void ReadableByteStreamController::Respond(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    size_t bytes_written,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond
  // 1. Assert: controller.[[pendingPullIntos]] is not empty.
  DCHECK(!controller->pending_pull_intos_.empty());
  // 2. Let firstDescriptor be controller.[[pendingPullIntos]][0].
  PullIntoDescriptor* first_descriptor = controller->pending_pull_intos_[0];
  // 3. Let state be controller.[[stream]].[[state]].
  const ReadableStream::State state =
      controller->controlled_readable_stream_->state_;
  // 4. If state is "closed",
  if (state == ReadableStream::kClosed) {
    //   a. If bytesWritten is not 0, throw a TypeError exception.
    if (bytes_written != 0) {
      exception_state.ThrowTypeError("bytes written is not 0");
      return;
    }
    // 5. Otherwise,
  } else {
    //   a. Assert: state is "readable".
    DCHECK_EQ(state, ReadableStream::kReadable);
    //   b. If bytesWritten is 0, throw a TypeError exception.
    if (bytes_written == 0) {
      exception_state.ThrowTypeError("bytes written is 0");
      return;
    }
    //   c. If firstDescriptor's bytes filled + bytesWritten > firstDescriptor's
    //   byte length, throw a RangeError exception.
    if (base::ClampAdd(first_descriptor->bytes_filled, bytes_written) >
        first_descriptor->byte_length) {
      exception_state.ThrowRangeError(
          "available read buffer is too small for specified number of bytes");
      return;
    }
  }
  // 6. Set firstDescriptor's buffer to ! TransferArrayBuffer(firstDescriptor's
  // buffer).
  first_descriptor->buffer = TransferArrayBuffer(
      script_state, first_descriptor->buffer, exception_state);
  // 7. Perform ? ReadableByteStreamControllerRespondInternal(controller,
  // bytesWritten).
  RespondInternal(script_state, controller, bytes_written, exception_state);
}

void ReadableByteStreamController::RespondInClosedState(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    PullIntoDescriptor* first_descriptor,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-in-closed-state
  // 1. Assert: firstDescriptor’s bytes filled is 0.
  DCHECK_EQ(first_descriptor->bytes_filled, 0u);
  // 2. If firstDescriptor’s reader type is "none", perform !
  // ReadableByteStreamControllerShiftPendingPullInto(controller).
  if (first_descriptor->reader_type == ReaderType::kNone) {
    ShiftPendingPullInto(controller);
  }
  // 3. Let stream be controller.[[stream]].
  ReadableStream* const stream = controller->controlled_readable_stream_;
  // 4. If ! ReadableStreamHasBYOBReader(stream) is true,
  if (ReadableStream::HasBYOBReader(stream)) {
    //   a. While ! ReadableStreamGetNumReadIntoRequests(stream) > 0,
    while (ReadableStream::GetNumReadIntoRequests(stream) > 0) {
      //     i. Let pullIntoDescriptor be !
      //     ReadableByteStreamControllerShiftPendingPullInto(controller).
      PullIntoDescriptor* pull_into_descriptor =
          ShiftPendingPullInto(controller);
      //     ii. Perform !
      //     ReadableByteStreamControllerCommitPullIntoDescriptor(stream,
      //     pullIntoDescriptor).
      CommitPullIntoDescriptor(script_state, stream, pull_into_descriptor,
                               exception_state);
      DCHECK(!exception_state.HadException());
    }
  }
}

void ReadableByteStreamController::RespondInReadableState(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    size_t bytes_written,
    PullIntoDescriptor* pull_into_descriptor,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-in-readable-state
  // 1. Assert: pullIntoDescriptor's bytes filled + bytesWritten ≤
  // pullIntoDescriptor's byte length.
  DCHECK_LE(pull_into_descriptor->bytes_filled + bytes_written,
            pull_into_descriptor->byte_length);
  // 2. Perform !
  // ReadableByteStreamControllerFillHeadPullIntoDescriptor(controller,
  // bytesWritten, pullIntoDescriptor).
  FillHeadPullIntoDescriptor(controller, bytes_written, pull_into_descriptor);
  // 3. If pullIntoDescriptor’s reader type is "none",
  if (pull_into_descriptor->reader_type == ReaderType::kNone) {
    //   a. Perform ?
    //   ReadableByteStreamControllerEnqueueDetachedPullIntoToQueue(controller,
    //   pullIntoDescriptor).
    EnqueueDetachedPullIntoToQueue(controller, pull_into_descriptor);
    //   b. Perform !
    //   ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(controller).
    ProcessPullIntoDescriptorsUsingQueue(script_state, controller);
    //   c. Return.
    return;
  }
  // 4. If pullIntoDescriptor’s bytes filled < pullIntoDescriptor’s element
  // size, return.
  if (pull_into_descriptor->bytes_filled < pull_into_descriptor->element_size) {
    return;
  }
  // 5. Perform ! ReadableByteStreamControllerShiftPendingPullInto(controller).
  ShiftPendingPullInto(controller);
  // 6. Let remainderSize be pullIntoDescriptor’s bytes filled mod
  // pullIntoDescriptor’s element size.
  const size_t remainder_size =
      pull_into_descriptor->bytes_filled % pull_into_descriptor->element_size;
  // 7. If remainderSize > 0,
  if (remainder_size > 0) {
    //   a. Let end be pullIntoDescriptor’s byte offset + pullIntoDescriptor’s
    //   bytes filled.
    //   This addition will not overflow because byte offset and bytes filled
    //   refer to actually allocated memory, so together they cannot exceed
    //   size_t.
    size_t end =
        pull_into_descriptor->byte_offset + pull_into_descriptor->bytes_filled;
    //   b. Perform ?
    //   ReadableByteStreamControllerEnqueueClonedChunkToQueue(controller,
    //   pullIntoDescriptor’s buffer, end − remainderSize, remainderSize).
    EnqueueClonedChunkToQueue(controller, pull_into_descriptor->buffer,
                              end - remainder_size, remainder_size);
  }
  // 8. Set pullIntoDescriptor’s bytes filled to pullIntoDescriptor’s bytes
  // filled − remainderSize.
  pull_into_descriptor->bytes_filled =
      pull_into_descriptor->bytes_filled - remainder_size;
  // 9. Perform !
  // ReadableByteStreamControllerCommitPullIntoDescriptor(controller.[[stream]],
  // pullIntoDescriptor).
  CommitPullIntoDescriptor(script_state,
                           controller->controlled_readable_stream_,
                           pull_into_descriptor, exception_state);
  DCHECK(!exception_state.HadException());
  // 10. Perform !
  // ReadableByteStreamControllerProcessPullIntoDescriptorsUsingQueue(controller).
  ProcessPullIntoDescriptorsUsingQueue(script_state, controller);
  DCHECK(!exception_state.HadException());
}

void ReadableByteStreamController::RespondInternal(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    size_t bytes_written,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-internal
  // 1. Let firstDescriptor be controller.[[pendingPullIntos]][0].
  PullIntoDescriptor* const first_descriptor =
      controller->pending_pull_intos_[0];
  // 2. Assert: ! CanTransferArrayBuffer(firstDescriptor's buffer) is true.
  DCHECK(CanTransferArrayBuffer(first_descriptor->buffer));
  // 3. Perform ! ReadableByteStreamControllerInvalidateBYOBRequest(controller).
  InvalidateBYOBRequest(controller);
  // 4. Let state be controller.[[stream]].[[state]].
  const ReadableStream::State state =
      controller->controlled_readable_stream_->state_;
  // 5. If state is "closed",
  if (state == ReadableStream::kClosed) {
    //   a. Assert: bytesWritten is 0
    DCHECK_EQ(bytes_written, 0u);
    //   b. Perform !
    //   ReadableByteStreamControllerRespondInClosedState(controller,
    //   firstDescriptor).
    RespondInClosedState(script_state, controller, first_descriptor,
                         exception_state);
  } else {
    // 6. Otherwise,
    //   a. Assert: state is "readable".
    DCHECK_EQ(state, ReadableStream::kReadable);
    //   b. Assert: bytesWritten > 0.
    DCHECK_GT(bytes_written, 0u);
    //   c. Perform ?
    //   ReadableByteStreamControllerRespondInReadableState(controller,
    //   bytesWritten, firstDescriptor).
    RespondInReadableState(script_state, controller, bytes_written,
                           first_descriptor, exception_state);
  }
  // 7. Perform ! ReadableByteStreamControllerCallPullIfNeeded(controller).
  CallPullIfNeeded(script_state, controller);
}

void ReadableByteStreamController::RespondWithNewView(
    ScriptState* script_state,
    ReadableByteStreamController* controller,
    NotShared<DOMArrayBufferView> view,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#readable-byte-stream-controller-respond-with-new-view
  // 1. Assert: controller.[[pendingPullIntos]] is not empty.
  DCHECK(!controller->pending_pull_intos_.empty());
  // 2. Assert: ! IsDetachedBuffer(view.[[ViewedArrayBuffer]]) is false.
  DCHECK(!view->buffer()->IsDetached());
  // 3. Let firstDescriptor be controller.[[pendingPullIntos]][0].
  PullIntoDescriptor* first_descriptor = controller->pending_pull_intos_[0];
  // 4. Let state be controller.[[stream]].[[state]].
  const ReadableStream::State state =
      controller->controlled_readable_stream_->state_;
  // 5. If state is "closed",
  if (state == ReadableStream::kClosed) {
    //   a. If view.[[ByteLength]] is not 0, throw a TypeError exception.
    if (view->byteLength() != 0) {
      exception_state.ThrowTypeError("view's byte length is not 0");
      return;
    }
    // 6. Otherwise,
  } else {
    //   a. Assert: state is "readable".
    DCHECK_EQ(state, ReadableStream::kReadable);
    //   b. If view.[[ByteLength]] is 0, throw a TypeError exception.
    if (view->byteLength() == 0) {
      exception_state.ThrowTypeError("view's byte length is 0");
      return;
    }
  }
  // 7. If firstDescriptor’s byte offset + firstDescriptor’ bytes filled is not
  // view.[[ByteOffset]], throw a RangeError exception.
  // We don't expect this addition to overflow as the bytes are expected to be
  // equal.
  if (first_descriptor->byte_offset + first_descriptor->bytes_filled !=
      view->byteOffset()) {
    exception_state.ThrowRangeError(
        "supplied view's byte offset doesn't match the expected value");
    return;
  }
  // 8. If firstDescriptor’s buffer byte length is not
  // view.[[ViewedArrayBuffer]].[[ByteLength]], throw a RangeError exception.
  if (first_descriptor->buffer_byte_length != view->buffer()->ByteLength()) {
    exception_state.ThrowRangeError("buffer byte lengths are not equal");
    return;
  }
  // 9. If firstDescriptor's bytes filled + view.[[ByteLength]] >
  // firstDescriptor's byte length, throw a RangeError exception.
  if (base::ClampAdd(first_descriptor->bytes_filled, view->byteLength()) >
      first_descriptor->byte_length) {
    exception_state.ThrowRangeError(
        "supplied view is too large for the read buffer");
    return;
  }
  // 10. Let viewByteLength be view.[[ByteLength]].
  const size_t view_byte_length = view->byteLength();
  // 11. Set firstDescriptor’s buffer to ? TransferArrayBuffer(
  // view.[[ViewedArrayBuffer]]).
  first_descriptor->buffer =
      TransferArrayBuffer(script_state, view->buffer(), exception_state);
  if (exception_state.HadException()) {
    return;
  }
  // 12. Perform ? ReadableByteStreamControllerRespondInternal(controller,
  // viewByteLength).
  RespondInternal(script_state, controller, view_byte_length, exception_state);
}

bool ReadableByteStreamController::CanTransferArrayBuffer(
    DOMArrayBuffer* buffer) {
  return !buffer->IsDetached();
}

DOMArrayBuffer* ReadableByteStreamController::TransferArrayBuffer(
    ScriptState* script_state,
    DOMArrayBuffer* buffer,
    ExceptionState& exception_state) {
  DCHECK(!buffer->IsDetached());
  if (!buffer->IsDetachable(script_state->GetIsolate())) {
    exception_state.ThrowTypeError("Could not transfer ArrayBuffer");
    return nullptr;
  }
  ArrayBufferContents contents;
  if (!buffer->Transfer(script_state->GetIsolate(), contents,
                        exception_state)) {
    return nullptr;
  }
  return DOMArrayBuffer::Create(std::move(contents));
}

void ReadableByteStreamController::Trace(Visitor* visitor) const {
  visitor->Trace(byob_request_);
  visitor->Trace(cancel_algorithm_);
  visitor->Trace(controlled_readable_stream_);
  visitor->Trace(pending_pull_intos_);
  visitor->Trace(pull_algorithm_);
  visitor->Trace(queue_);
  ScriptWrappable::Trace(visitor);
}

//
// Readable byte stream controller internal methods
//

v8::Local<v8::Promise> ReadableByteStreamController::CancelSteps(
    ScriptState* script_state,
    v8::Local<v8::Value> reason) {
  // https://streams.spec.whatwg.org/#rbs-controller-private-cancel
  // 1. Perform ! ReadableByteStreamControllerClearPendingPullIntos(this).
  ClearPendingPullIntos(this);
  // 2. Perform ! ResetQueue(this).
  ResetQueue(this);
  // 3. Let result be the result of performing this.[[cancelAlgorithm]], passing
  // in reason.
  auto result = cancel_algorithm_->Run(script_state, 1, &reason);
  // 4. Perform ! ReadableByteStreamControllerClearAlgorithms(this).
  ClearAlgorithms(this);
  // 5. Return result.
  return result;
}

void ReadableByteStreamController::PullSteps(ScriptState* script_state,
                                             ReadRequest* read_request,
                                             ExceptionState& exception_state) {
  // https://whatpr.org/streams/1029.html#rbs-controller-private-pull
  // TODO: This function follows an old version of the spec referenced above, so
  // it needs to be updated to the new version on
  // https://streams.spec.whatwg.org when the ReadableStreamDefaultReader
  // implementation is updated.
  // 1. Let stream be this.[[stream]].
  ReadableStream* const stream = controlled_readable_stream_;
  // 2. Assert: ! ReadableStreamHasDefaultReader(stream) is true.
  DCHECK(ReadableStream::HasDefaultReader(stream));
  // 3. If this.[[queueTotalSize]] > 0,
  if (queue_total_size_ > 0) {
    //   a. Assert: ! ReadableStreamGetNumReadRequests(stream) is 0.
    DCHECK_EQ(ReadableStream::GetNumReadRequests(stream), 0);
    //   b. Perform ! ReadableByteStreamControllerFillReadRequestFromQueue(this,
    //   readRequest).
    FillReadRequestFromQueue(script_state, this, read_request, exception_state);
    //   c. Return.
    return;
  }
  // 4. Let autoAllocateChunkSize be this.[[autoAllocateChunkSize]].
  const size_t auto_allocate_chunk_size = auto_allocate_chunk_size_;
  // 5. If autoAllocateChunkSize is not undefined,
  if (auto_allocate_chunk_size) {
    //   a. Let buffer be Construct(%ArrayBuffer%, « autoAllocateChunkSize »).
    auto* buffer = DOMArrayBuffer::Create(auto_allocate_chunk_size, 1);
    //   b. If buffer is an abrupt completion,
    //     i. Perform readRequest’s error steps, given buffer.[[Value]].
    //     ii. Return.
    //   This is not needed as DOMArrayBuffer::Create() is designed to
    //   crash if it cannot allocate the memory.

    //   c. Let pullIntoDescriptor be Record {[[buffer]]: buffer.[[Value]],
    //   [[bufferByteLength]]: autoAllocateChunkSize, [[byteOffset]]: 0,
    //   [[byteLength]]: autoAllocateChunkSize, [[bytesFilled]]: 0,
    //   [[elementSize]]: 1, [[ctor]]: %Uint8Array%, [[readerType]]: "default"}.
    auto* ctor = &CreateAsArrayBufferView<DOMUint8Array>;
    PullIntoDescriptor* pull_into_descriptor =
        MakeGarbageCollected<PullIntoDescriptor>(
            buffer, auto_allocate_chunk_size, 0, auto_allocate_chunk_size, 0, 1,
            ctor, ReaderType::kDefault);
    //   d. Append pullIntoDescriptor as the last element of
    //   this.[[pendingPullIntos]].
    pending_pull_intos_.push_back(pull_into_descriptor);
  }
  // 6. Perform ! ReadableStreamAddReadRequest(stream, readRequest).
  ReadableStream::AddReadRequest(script_state, stream, read_request);
  // 7. Perform ! ReadableByteStreamControllerCallPullIfNeeded(this).
  CallPullIfNeeded(script_state, this);
}

void ReadableByteStreamController::ReleaseSteps() {
  // https://streams.spec.whatwg.org/#abstract-opdef-readablebytestreamcontroller-releasesteps
  // 1. If this.[[pendingPullIntos]] is not empty,
  if (!pending_pull_intos_.empty()) {
    //   a. Let firstPendingPullInto be this.[[pendingPullIntos]][0].
    PullIntoDescriptor* first_pending_pull_into = pending_pull_intos_[0];
    //   b. Set firstPendingPullInto’s reader type to "none".
    first_pending_pull_into->reader_type = ReaderType::kNone;
    //   c. Set this.[[pendingPullIntos]] to the list « firstPendingPullInto ».
    pending_pull_intos_.clear();
    pending_pull_intos_.push_back(first_pending_pull_into);
  }
}

}  // namespace blink

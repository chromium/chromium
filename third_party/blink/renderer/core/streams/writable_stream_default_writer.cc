// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/core/streams/writable_stream_default_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

namespace {

String CreateWriterLockReleasedMessage(const char* verbed) {
  return String::Format(
      "This writable stream writer has been released and cannot be %s", verbed);
}

v8::Local<v8::Value> CreateWriterLockReleasedException(v8::Isolate* isolate,
                                                       const char* verbed) {
  return v8::Exception::TypeError(
      V8String(isolate, CreateWriterLockReleasedMessage(verbed)));
}

}  // namespace

WritableStreamDefaultWriter* WritableStreamDefaultWriter::Create(
    ScriptState* script_state,
    WritableStream* stream,
    ExceptionState& exception_state) {
  auto* writer = MakeGarbageCollected<WritableStreamDefaultWriter>(
      script_state, static_cast<WritableStream*>(stream), exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return writer;
}

// TODO(ricea): Does using the ScriptState supplied by IDL result in promises
// being created with the correct global?
WritableStreamDefaultWriter::WritableStreamDefaultWriter(
    ScriptState* script_state,
    WritableStream* stream,
    ExceptionState& exception_state)
    //  3. Set this.[[ownerWritableStream]] to stream.
    : owner_writable_stream_(stream) {
  // https://streams.spec.whatwg.org/#default-writer-constructor 2. If !
  //  IsWritableStreamLocked(stream) is true, throw a TypeError exception.
  if (WritableStream::IsLocked(stream)) {
    exception_state.ThrowTypeError(
        "Cannot create writer when WritableStream is locked");
    return;
  }
  //  4. Set stream.[[writer]] to this.
  stream->SetWriter(this);

  //  5. Let state be stream.[[state]].
  const auto state = stream->GetState();
  auto* isolate = script_state->GetIsolate();

  switch (state) {
    //  6. If state is "writable",
    case WritableStream::kWritable: {
      //      a. If ! WritableStreamCloseQueuedOrInFlight(stream) is false and
      //         stream.[[backpressure]] is true, set this.[[readyPromise]] to
      //         a new promise.
      if (!WritableStream::CloseQueuedOrInFlight(stream) &&
          stream->HasBackpressure()) {
        ready_promise_ = MakeGarbageCollected<StreamPromiseResolver>(
            script_state, exception_state);
      } else {
        //      b. Otherwise, set this.[[readyPromise]] to a promise resolved
        //         with undefined.
        ready_promise_ =
            StreamPromiseResolver::CreateResolvedWithUndefined(script_state);
      }
      //      c. Set this.[[closedPromise]] to a new promise.
      closed_promise_ = MakeGarbageCollected<StreamPromiseResolver>(
          script_state, exception_state);
      break;
    }

    //  7. Otherwise, if state is "erroring",
    case WritableStream::kErroring: {
      //      a. Set this.[[readyPromise]] to a promise rejected with
      //         stream.[[storedError]].
      ready_promise_ = StreamPromiseResolver::CreateRejectedAndSilent(
          script_state, stream->GetStoredError(isolate));

      //      b. Set this.[[readyPromise]].[[PromiseIsHandled]] to true.
      ready_promise_->MarkAsHandled(isolate);

      //      c. Set this.[[closedPromise]] to a new promise.
      closed_promise_ = MakeGarbageCollected<StreamPromiseResolver>(
          script_state, exception_state);
      break;
    }

    //  8. Otherwise, if state is "closed",
    case WritableStream::kClosed: {
      //      a. Set this.[[readyPromise]] to a promise resolved with undefined.
      ready_promise_ =
          StreamPromiseResolver::CreateResolvedWithUndefined(script_state);

      //      b. Set this.[[closedPromise]] to a promise resolved with
      //         undefined.
      closed_promise_ =
          StreamPromiseResolver::CreateResolvedWithUndefined(script_state);
      break;
    }

    //  9. Otherwise,
    case WritableStream::kErrored: {
      //      a. Assert: state is "errored".
      // Check omitted as it is not meaningful.

      //      b. Let storedError be stream.[[storedError]].
      const auto stored_error = stream->GetStoredError(isolate);

      //      c. Set this.[[readyPromise]] to a promise rejected with
      //         storedError.
      ready_promise_ = StreamPromiseResolver::CreateRejectedAndSilent(
          script_state, stored_error);

      //      d. Set this.[[readyPromise]].[[PromiseIsHandled]] to true.
      ready_promise_->MarkAsHandled(isolate);

      //      e. Set this.[[closedPromise]] to a promise rejected with
      //         storedError.
      closed_promise_ = StreamPromiseResolver::CreateRejectedAndSilent(
          script_state, stored_error);

      //      f. Set this.[[closedPromise]].[[PromiseIsHandled]] to true.
      closed_promise_->MarkAsHandled(isolate);
      break;
    }
  }
}

WritableStreamDefaultWriter::~WritableStreamDefaultWriter() = default;

ScriptPromise WritableStreamDefaultWriter::closed(
    ScriptState* script_state) const {
  // https://streams.spec.whatwg.org/#default-writer-closed
  //  2. Return this.[[closedPromise]].
  return closed_promise_->GetScriptPromise(script_state);
}

ScriptValue WritableStreamDefaultWriter::desiredSize(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  auto* isolate = script_state->GetIsolate();
  // https://streams.spec.whatwg.org/#default-writer-desired-size
  //  2. If this.[[ownerWritableStream]] is undefined, throw a TypeError
  //     exception.
  if (!owner_writable_stream_) {
    exception_state.RethrowV8Exception(CreateWriterLockReleasedException(
        isolate, "used to get the desiredSize"));
    return ScriptValue();
  }

  //  3. Return ! WritableStreamDefaultWriterGetDesiredSize(this).
  return ScriptValue(isolate, GetDesiredSize(isolate, this));
}

ScriptPromise WritableStreamDefaultWriter::ready(
    ScriptState* script_state) const {
  // https://streams.spec.whatwg.org/#default-writer-ready
  //  2. Return this.[[readyPromise]].
  return ready_promise_->GetScriptPromise(script_state);
}

ScriptPromise WritableStreamDefaultWriter::abort(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return abort(script_state,
               ScriptValue(script_state->GetIsolate(),
                           v8::Undefined(script_state->GetIsolate())),
               exception_state);
}

ScriptPromise WritableStreamDefaultWriter::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#default-writer-abort
  //  2. If this.[[ownerWritableStream]] is undefined, return a promise rejected
  //     with a TypeError exception.
  if (!owner_writable_stream_) {
    exception_state.ThrowTypeError(CreateWriterLockReleasedMessage("aborted"));
    return ScriptPromise();
  }

  //  3. Return ! WritableStreamDefaultWriterAbort(this, reason).
  return ScriptPromise(script_state,
                       Abort(script_state, this, reason.V8Value()));
}

ScriptPromise WritableStreamDefaultWriter::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#default-writer-close
  //  2. Let stream be this.[[ownerWritableStream]].
  WritableStream* stream = owner_writable_stream_;

  //  3. If stream is undefined, return a promise rejected with a TypeError
  //     exception.
  if (!stream) {
    exception_state.ThrowTypeError(CreateWriterLockReleasedMessage("closed"));
    return ScriptPromise();
  }

  //  4. If ! WritableStreamCloseQueuedOrInFlight(stream) is true, return a
  //      promise rejected with a TypeError exception.
  if (WritableStream::CloseQueuedOrInFlight(stream)) {
    exception_state.ThrowTypeError(
        "Cannot close a writable stream that has "
        "already been requested to be closed");
    return ScriptPromise();
  }

  //  5. Return ! WritableStreamDefaultWriterClose(this).
  return ScriptPromise(script_state, Close(script_state, this));
}

void WritableStreamDefaultWriter::releaseLock(ScriptState* script_state) {
  // https://streams.spec.whatwg.org/#default-writer-release-lock
  //  2. Let stream be this.[[ownerWritableStream]].
  WritableStream* stream = owner_writable_stream_;

  //  3. If stream is undefined, return.
  if (!stream) {
    return;
  }

  //  4. Assert: stream.[[writer]] is not undefined.
  DCHECK(stream->Writer());

  //  5. Perform ! WritableStreamDefaultWriterRelease(this).
  Release(script_state, this);
}

ScriptPromise WritableStreamDefaultWriter::write(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return write(script_state,
               ScriptValue(script_state->GetIsolate(),
                           v8::Undefined(script_state->GetIsolate())),
               exception_state);
}

ScriptPromise WritableStreamDefaultWriter::write(
    ScriptState* script_state,
    ScriptValue chunk,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#default-writer-write
  //  2. If this.[[ownerWritableStream]] is undefined, return a promise rejected
  //     with a TypeError exception.
  if (!owner_writable_stream_) {
    exception_state.ThrowTypeError(
        CreateWriterLockReleasedMessage("written to"));
    return ScriptPromise();
  }

  //  3. Return ! WritableStreamDefaultWriterWrite(this, chunk).
  return ScriptPromise(script_state, Write(script_state, this, chunk.V8Value(),
                                           exception_state));
}

void WritableStreamDefaultWriter::EnsureReadyPromiseRejected(
    ScriptState* script_state,
    WritableStreamDefaultWriter* writer,
    v8::Local<v8::Value> error) {
  auto* isolate = script_state->GetIsolate();
  // https://streams.spec.whatwg.org/#writable-stream-default-writer-ensure-ready-promise-rejected
  //  1. If writer.[[readyPromise]].[[PromiseState]] is "pending", reject
  //     writer.[[readyPromise]] with error.
  if (!writer->ready_promise_->IsSettled()) {
    writer->ready_promise_->MarkAsSilent(isolate);
    writer->ready_promise_->Reject(script_state, error);
  } else {
    //  2. Otherwise, set writer.[[readyPromise]] to a promise rejected with
    //     error.
    writer->ready_promise_ =
        StreamPromiseResolver::CreateRejectedAndSilent(script_state, error);
  }

  //  3. Set writer.[[readyPromise]].[[PromiseIsHandled]] to true.
  writer->ready_promise_->MarkAsHandled(isolate);
}

v8::Local<v8::Promise> WritableStreamDefaultWriter::CloseWithErrorPropagation(
    ScriptState* script_state,
    WritableStreamDefaultWriter* writer) {
  // https://streams.spec.whatwg.org/#writable-stream-default-writer-close-with-error-propagation
  //  1. Let stream be writer.[[ownerWritableStream]].
  WritableStream* stream = writer->owner_writable_stream_;

  //  2. Assert: stream is not undefined.
  DCHECK(stream);

  //  3. Let state be stream.[[state]].
  const auto state = stream->GetState();

  //  4. If ! WritableStreamCloseQueuedOrInFlight(stream) is true or state is
  //     "closed", return a promise resolved with undefined.
  if (WritableStream::CloseQueuedOrInFlight(stream) ||
      state == WritableStream::kClosed) {
    return PromiseResolveWithUndefined(script_state);
  }

  //  5. If state is "errored", return a promise rejected with
  //     stream.[[storedError]].
  if (state == WritableStream::kErrored) {
    return PromiseReject(script_state,
                         stream->GetStoredError(script_state->GetIsolate()));
  }

  //  6. Assert: state is "writable" or "erroring".
  CHECK(state == WritableStream::kWritable ||
        state == WritableStream::kErroring);

  //  7. Return ! WritableStreamDefaultWriterClose(writer).
  return Close(script_state, writer);
}

void WritableStreamDefaultWriter::Release(ScriptState* script_state,
                                          WritableStreamDefaultWriter* writer) {
  // https://streams.spec.whatwg.org/#writable-stream-default-writer-release
  //  1. Let stream be writer.[[ownerWritableStream]].
  WritableStream* stream = writer->owner_writable_stream_;

  //  2. Assert: stream is not undefined.
  DCHECK(stream);

  //  3. Assert: stream.[[writer]] is writer.
  DCHECK_EQ(stream->Writer(), writer);

  //  4. Let releasedError be a new TypeError.
  const auto released_error = v8::Exception::TypeError(V8String(
      script_state->GetIsolate(),
      "This writable stream writer has been released and cannot be used to "
      "monitor the stream\'s state"));

  //  5. Perform ! WritableStreamDefaultWriterEnsureReadyPromiseRejected(writer,
  //     releasedError).
  EnsureReadyPromiseRejected(script_state, writer, released_error);

  //  6. Perform !
  //     WritableStreamDefaultWriterEnsureClosedPromiseRejected(writer,
  //     releasedError).
  EnsureClosedPromiseRejected(script_state, writer, released_error);

  //  7. Set stream.[[writer]] to undefined.
  stream->SetWriter(nullptr);

  //  8. Set writer.[[ownerWritableStream]] to undefined.
  writer->owner_writable_stream_ = nullptr;
}

v8::Local<v8::Promise> WritableStreamDefaultWriter::Write(
    ScriptState* script_state,
    WritableStreamDefaultWriter* writer,
    v8::Local<v8::Value> chunk,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#writable-stream-default-writer-write
  //  1. Let stream be writer.[[ownerWritableStream]].
  WritableStream* stream = writer->owner_writable_stream_;

  //  2. Assert: stream is not undefined.
  DCHECK(stream);

  //  3. Let controller be stream.[[writableStreamController]].
  WritableStreamDefaultController* controller = stream->Controller();

  auto* isolate = script_state->GetIsolate();
  //  4. Let chunkSize be !
  //     WritableStreamDefaultControllerGetChunkSize(controller, chunk).
  double chunk_size = WritableStreamDefaultController::GetChunkSize(
      script_state, controller, chunk, exception_state);

  //  5. If stream is not equal to writer.[[ownerWritableStream]], return a
  //     promise rejected with a TypeError exception.
  if (stream != writer->owner_writable_stream_) {
    return PromiseReject(
        script_state, CreateWriterLockReleasedException(isolate, "written to"));
  }

  //  6. Let state be stream.[[state]].
  const auto state = stream->GetState();

  //  7. If state is "errored", return a promise rejected with
  //     stream.[[storedError]].
  if (state == WritableStream::kErrored) {
    return PromiseReject(script_state, stream->GetStoredError(isolate));
  }

  //  8. If ! WritableStreamCloseQueuedOrInFlight(stream) is true or state is
  //     "closed", return a promise rejected with a TypeError exception
  //     indicating that the stream is closing or closed.
  if (WritableStream::CloseQueuedOrInFlight(stream)) {
    return PromiseReject(
        script_state,
        v8::Exception::TypeError(
            WritableStream::CreateCannotActionOnStateStreamMessage(
                isolate, "write to", "closing")));
  }
  if (state == WritableStream::kClosed) {
    return PromiseReject(
        script_state, WritableStream::CreateCannotActionOnStateStreamException(
                          isolate, "write to", WritableStream::kClosed));
  }

  //  9. If state is "erroring", return a promise rejected with
  //     stream.[[storedError]].
  if (state == WritableStream::kErroring) {
    return PromiseReject(script_state, stream->GetStoredError(isolate));
  }

  // 10. Assert: state is "writable".
  DCHECK_EQ(state, WritableStream::kWritable);

  // 11. Let promise be ! WritableStreamAddWriteRequest(stream).
  auto promise = WritableStream::AddWriteRequest(script_state, stream);

  // 12. Perform ! WritableStreamDefaultControllerWrite(controller, chunk,
  //     chunkSize).
  WritableStreamDefaultController::Write(script_state, controller, chunk,
                                         chunk_size, exception_state);

  // 13. Return promise.
  return promise;
}

absl::optional<double> WritableStreamDefaultWriter::GetDesiredSizeInternal()
    const {
  // https://streams.spec.whatwg.org/#writable-stream-default-writer-get-desired-size
  //  1. Let stream be writer.[[ownerWritableStream]].
  const WritableStream* stream = owner_writable_stream_;

  //  2. Let state be stream.[[state]].
  const auto state = stream->GetState();

  switch (state) {
    //  3. If state is "errored" or "erroring", return null.
    case WritableStream::kErrored:
    case WritableStream::kErroring:
      return absl::nullopt;

      //  4. If state is "closed", return 0.
    case WritableStream::kClosed:
      return 0.0;

    default:
      //  5. Return ! WritableStreamDefaultControllerGetDesiredSize(
      //     stream.[[writableStreamController]]).
      return WritableStreamDefaultController::GetDesiredSize(
          stream->Controller());
  }
}

void WritableStreamDefaultWriter::SetReadyPromise(
    StreamPromiseResolver* ready_promise) {
  ready_promise_ = ready_promise;
}

void WritableStreamDefaultWriter::Trace(Visitor* visitor) const {
  visitor->Trace(closed_promise_);
  visitor->Trace(owner_writable_stream_);
  visitor->Trace(ready_promise_);
  ScriptWrappable::Trace(visitor);
}

// Writable Stream Writer Abstract Operations

v8::Local<v8::Promise> WritableStreamDefaultWriter::Abort(
    ScriptState* script_state,
    WritableStreamDefaultWriter* writer,
    v8::Local<v8::Value> reason) {
  // https://streams.spec.whatwg.org/#writable-stream-default-writer-abort
  //  1. Let stream be writer.[[ownerWritableStream]].
  WritableStream* stream = writer->owner_writable_stream_;

  //  2. Assert: stream is not undefined.
  DCHECK(stream);

  //  3. Return ! WritableStreamAbort(stream, reason).
  return WritableStream::Abort(script_state, stream, reason);
}

v8::Local<v8::Promise> WritableStreamDefaultWriter::Close(
    ScriptState* script_state,
    WritableStreamDefaultWriter* writer) {
  // https://streams.spec.whatwg.org/#writable-stream-default-writer-close
  //  1. Let stream be writer.[[ownerWritableStream]].
  WritableStream* stream = writer->owner_writable_stream_;

  //  2. Assert: stream is not undefined.
  DCHECK(stream);

  //  3. Return ! WritableStreamClose(stream).
  return WritableStream::Close(script_state, stream);
}

void WritableStreamDefaultWriter::EnsureClosedPromiseRejected(
    ScriptState* script_state,
    WritableStreamDefaultWriter* writer,
    v8::Local<v8::Value> error) {
  auto* isolate = script_state->GetIsolate();
  // https://streams.spec.whatwg.org/#writable-stream-default-writer-ensure-closed-promise-rejected
  //  1. If writer.[[closedPromise]].[[PromiseState]] is "pending", reject
  //     writer.[[closedPromise]] with error.
  if (!writer->closed_promise_->IsSettled()) {
    writer->closed_promise_->MarkAsSilent(isolate);
    writer->closed_promise_->Reject(script_state, error);
  } else {
    //  2. Otherwise, set writer.[[closedPromise]] to a promise rejected with
    //     error.
    writer->closed_promise_ =
        StreamPromiseResolver::CreateRejectedAndSilent(script_state, error);
  }

  //  3. Set writer.[[closedPromise]].[[PromiseIsHandled]] to true.
  writer->closed_promise_->MarkAsHandled(isolate);
}

v8::Local<v8::Value> WritableStreamDefaultWriter::GetDesiredSize(
    v8::Isolate* isolate,
    const WritableStreamDefaultWriter* writer) {
  // https://streams.spec.whatwg.org/#writable-stream-default-writer-get-desired-size
  //  1. Let stream be writer.[[ownerWritableStream]].
  //  2. Let state be stream.[[state]].
  //  3. If state is "errored" or "erroring", return null.
  absl::optional<double> desired_size = writer->GetDesiredSizeInternal();
  if (!desired_size.has_value()) {
    return v8::Null(isolate);
  }

  //  4. If state is "closed", return 0.
  //  5. Return ! WritableStreamDefaultControllerGetDesiredSize(
  //     stream.[[writableStreamController]]).
  return v8::Number::New(isolate, desired_size.value());
}

}  // namespace blink

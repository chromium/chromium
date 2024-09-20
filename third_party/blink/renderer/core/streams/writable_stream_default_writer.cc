// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/writable_stream_default_writer.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/miscellaneous_operations.h"
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
    : owner_writable_stream_(stream),
      closed_resolver_(
          MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
              script_state)),
      ready_resolver_(MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(
          script_state)) {
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
      // The step above is done in the initializer list.

      if (WritableStream::CloseQueuedOrInFlight(stream) ||
          !stream->HasBackpressure()) {
        //      b. Otherwise, set this.[[readyPromise]] to a promise resolved
        //         with undefined.
        ready_resolver_->Resolve();
      }
      //      c. Set this.[[closedPromise]] to a new promise.
      break;
    }

    //  7. Otherwise, if state is "erroring",
    case WritableStream::kErroring: {
      //      a. Set this.[[readyPromise]] to a promise rejected with
      //         stream.[[storedError]].
      ready_resolver_->Promise().MarkAsSilent();
      ready_resolver_->Reject(stream->GetStoredError(isolate));

      //      b. Set this.[[readyPromise]].[[PromiseIsHandled]] to true.
      ready_resolver_->Promise().MarkAsHandled();

      //      c. Set this.[[closedPromise]] to a new promise.
      break;
    }

    //  8. Otherwise, if state is "closed",
    case WritableStream::kClosed: {
      //      a. Set this.[[readyPromise]] to a promise resolved with undefined.
      ready_resolver_->Resolve();

      //      b. Set this.[[closedPromise]] to a promise resolved with
      //         undefined.
      closed_resolver_->Resolve();
      break;
    }

    //  9. Otherwise,
    case WritableStream::kErrored: {
      //      a. Assert: state is "errored".
      // Check omitted as it is not meaningful.

      //      b. Let storedError be stream.[[storedError]].
      const auto stored_error =
          ScriptValue(isolate, stream->GetStoredError(isolate));

      //      c. Set this.[[readyPromise]] to a promise rejected with
      //         storedError.
      ready_resolver_->Promise().MarkAsSilent();
      ready_resolver_->Reject(stored_error);

      //      d. Set this.[[readyPromise]].[[PromiseIsHandled]] to true.
      ready_resolver_->Promise().MarkAsHandled();

      //      e. Set this.[[closedPromise]] to a promise rejected with
      //         storedError.
      closed_resolver_->Promise().MarkAsSilent();
      closed_resolver_->Reject(stored_error);

      //      f. Set this.[[closedPromise]].[[PromiseIsHandled]] to true.
      closed_resolver_->Promise().MarkAsHandled();
      break;
    }
  }
}

WritableStreamDefaultWriter::~WritableStreamDefaultWriter() = default;

ScriptPromise<IDLUndefined> WritableStreamDefaultWriter::closed(
    ScriptState* script_state) const {
  // https://streams.spec.whatwg.org/#default-writer-closed
  //  2. Return this.[[closedPromise]].
  return closed_resolver_->Promise();
}

ScriptValue WritableStreamDefaultWriter::desiredSize(
    ScriptState* script_state,
    ExceptionState& exception_state) const {
  auto* isolate = script_state->GetIsolate();
  // https://streams.spec.whatwg.org/#default-writer-desired-size
  //  2. If this.[[ownerWritableStream]] is undefined, throw a TypeError
  //     exception.
  if (!owner_writable_stream_) {
    exception_state.ThrowTypeError(
        CreateWriterLockReleasedMessage("used to get the desiredSize"));
    return ScriptValue();
  }

  //  3. Return ! WritableStreamDefaultWriterGetDesiredSize(this).
  return ScriptValue(isolate, GetDesiredSize(isolate, this));
}

ScriptPromise<IDLUndefined> WritableStreamDefaultWriter::ready(
    ScriptState* script_state) const {
  // https://streams.spec.whatwg.org/#default-writer-ready
  //  2. Return this.[[readyPromise]].
  return ready_resolver_->Promise();
}

ScriptPromise<IDLUndefined> WritableStreamDefaultWriter::abort(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return abort(script_state,
               ScriptValue(script_state->GetIsolate(),
                           v8::Undefined(script_state->GetIsolate())),
               exception_state);
}

ScriptPromise<IDLUndefined> WritableStreamDefaultWriter::abort(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#default-writer-abort
  //  2. If this.[[ownerWritableStream]] is undefined, return a promise rejected
  //     with a TypeError exception.
  if (!owner_writable_stream_) {
    exception_state.ThrowTypeError(CreateWriterLockReleasedMessage("aborted"));
    return EmptyPromise();
  }

  //  3. Return ! WritableStreamDefaultWriterAbort(this, reason).
  return Abort(script_state, this, reason.V8Value());
}

ScriptPromise<IDLUndefined> WritableStreamDefaultWriter::close(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#default-writer-close
  //  2. Let stream be this.[[ownerWritableStream]].
  WritableStream* stream = owner_writable_stream_;

  //  3. If stream is undefined, return a promise rejected with a TypeError
  //     exception.
  if (!stream) {
    exception_state.ThrowTypeError(CreateWriterLockReleasedMessage("closed"));
    return EmptyPromise();
  }

  //  4. If ! WritableStreamCloseQueuedOrInFlight(stream) is true, return a
  //      promise rejected with a TypeError exception.
  if (WritableStream::CloseQueuedOrInFlight(stream)) {
    exception_state.ThrowTypeError(
        "Cannot close a writable stream that has "
        "already been requested to be closed");
    return EmptyPromise();
  }

  //  5. Return ! WritableStreamDefaultWriterClose(this).
  return Close(script_state, this);
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

ScriptPromise<IDLUndefined> WritableStreamDefaultWriter::write(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return write(script_state,
               ScriptValue(script_state->GetIsolate(),
                           v8::Undefined(script_state->GetIsolate())),
               exception_state);
}

ScriptPromise<IDLUndefined> WritableStreamDefaultWriter::write(
    ScriptState* script_state,
    ScriptValue chunk,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#default-writer-write
  //  2. If this.[[ownerWritableStream]] is undefined, return a promise rejected
  //     with a TypeError exception.
  if (!owner_writable_stream_) {
    exception_state.ThrowTypeError(
        CreateWriterLockReleasedMessage("written to"));
    return EmptyPromise();
  }

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowTypeError("invalid realm");
    return EmptyPromise();
  }

  //  3. Return ! WritableStreamDefaultWriterWrite(this, chunk).
  return Write(script_state, this, chunk.V8Value(), exception_state)->Promise();
}

void WritableStreamDefaultWriter::EnsureReadyPromiseRejected(
    ScriptState* script_state,
    WritableStreamDefaultWriter* writer,
    v8::Local<v8::Value> error) {
  if (!script_state->ContextIsValid()) {
    return;
  }
  // https://streams.spec.whatwg.org/#writable-stream-default-writer-ensure-ready-promise-rejected
  //  1. If writer.[[readyPromise]].[[PromiseState]] is "pending", reject
  //     writer.[[readyPromise]] with error.
  if (writer->ready_resolver_->Promise().V8Promise()->State() !=
      v8::Promise::kPending) {
    //  2. Otherwise, set writer.[[readyPromise]] to a promise rejected with
    //     error.
    writer->ready_resolver_ =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  }
  writer->ready_resolver_->Promise().MarkAsSilent();
  writer->ready_resolver_->Reject(error);

  //  3. Set writer.[[readyPromise]].[[PromiseIsHandled]] to true.
  writer->ready_resolver_->Promise().MarkAsHandled();
}

ScriptPromise<IDLUndefined>
WritableStreamDefaultWriter::CloseWithErrorPropagation(
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
    return ToResolvedUndefinedPromise(script_state);
  }

  //  5. If state is "errored", return a promise rejected with
  //     stream.[[storedError]].
  if (state == WritableStream::kErrored) {
    return ScriptPromise<IDLUndefined>::Reject(
        script_state, stream->GetStoredError(script_state->GetIsolate()));
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

ScriptPromiseResolver<IDLUndefined>* WritableStreamDefaultWriter::Write(
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
      script_state, controller, chunk);

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);

  //  5. If stream is not equal to writer.[[ownerWritableStream]], return a
  //     promise rejected with a TypeError exception.
  if (stream != writer->owner_writable_stream_) {
    resolver->Reject(CreateWriterLockReleasedException(isolate, "written to"));
    return resolver;
  }

  //  6. Let state be stream.[[state]].
  const auto state = stream->GetState();

  //  7. If state is "errored", return a promise rejected with
  //     stream.[[storedError]].
  if (state == WritableStream::kErrored) {
    resolver->Reject(stream->GetStoredError(isolate));
    return resolver;
  }

  //  8. If ! WritableStreamCloseQueuedOrInFlight(stream) is true or state is
  //     "closed", return a promise rejected with a TypeError exception
  //     indicating that the stream is closing or closed.
  if (WritableStream::CloseQueuedOrInFlight(stream)) {
    resolver->Reject(v8::Exception::TypeError(
        WritableStream::CreateCannotActionOnStateStreamMessage(
            isolate, "write to", "closing")));
    return resolver;
  }
  if (state == WritableStream::kClosed) {
    resolver->Reject(WritableStream::CreateCannotActionOnStateStreamException(
        isolate, "write to", WritableStream::kClosed));
    return resolver;
  }

  //  9. If state is "erroring", return a promise rejected with
  //     stream.[[storedError]].
  if (state == WritableStream::kErroring) {
    resolver->Reject(stream->GetStoredError(isolate));
    return resolver;
  }

  // 10. Assert: state is "writable".
  DCHECK_EQ(state, WritableStream::kWritable);

  // 11. Let promise be ! WritableStreamAddWriteRequest(stream).
  WritableStream::AddWriteRequest(stream, resolver);

  // 12. Perform ! WritableStreamDefaultControllerWrite(controller, chunk,
  //     chunkSize).
  WritableStreamDefaultController::Write(script_state, controller, chunk,
                                         chunk_size, exception_state);

  // 13. Return promise.
  return resolver;
}

std::optional<double> WritableStreamDefaultWriter::GetDesiredSizeInternal()
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
      return std::nullopt;

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

void WritableStreamDefaultWriter::ResetReadyPromise(ScriptState* script_state) {
  ready_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
}

void WritableStreamDefaultWriter::Trace(Visitor* visitor) const {
  visitor->Trace(owner_writable_stream_);
  visitor->Trace(closed_resolver_);
  visitor->Trace(ready_resolver_);
  ScriptWrappable::Trace(visitor);
}

// Writable Stream Writer Abstract Operations

ScriptPromise<IDLUndefined> WritableStreamDefaultWriter::Abort(
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

ScriptPromise<IDLUndefined> WritableStreamDefaultWriter::Close(
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
  if (!script_state->ContextIsValid()) {
    return;
  }

  // https://streams.spec.whatwg.org/#writable-stream-default-writer-ensure-closed-promise-rejected
  //  1. If writer.[[closedPromise]].[[PromiseState]] is "pending", reject
  //     writer.[[closedPromise]] with error.
  if (writer->closed_resolver_->Promise().V8Promise()->State() !=
      v8::Promise::kPending) {
    //  2. Otherwise, set writer.[[closedPromise]] to a promise rejected with
    //     error.
    writer->closed_resolver_ =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  }
  writer->closed_resolver_->Promise().MarkAsSilent();
  writer->closed_resolver_->Reject(error);

  //  3. Set writer.[[closedPromise]].[[PromiseIsHandled]] to true.
  writer->closed_resolver_->Promise().MarkAsHandled();
}

v8::Local<v8::Value> WritableStreamDefaultWriter::GetDesiredSize(
    v8::Isolate* isolate,
    const WritableStreamDefaultWriter* writer) {
  // https://streams.spec.whatwg.org/#writable-stream-default-writer-get-desired-size
  //  1. Let stream be writer.[[ownerWritableStream]].
  //  2. Let state be stream.[[state]].
  //  3. If state is "errored" or "erroring", return null.
  std::optional<double> desired_size = writer->GetDesiredSizeInternal();
  if (!desired_size.has_value()) {
    return v8::Null(isolate);
  }

  //  4. If state is "closed", return 0.
  //  5. Return ! WritableStreamDefaultControllerGetDesiredSize(
  //     stream.[[writableStreamController]]).
  return v8::Number::New(isolate, desired_size.value());
}

}  // namespace blink

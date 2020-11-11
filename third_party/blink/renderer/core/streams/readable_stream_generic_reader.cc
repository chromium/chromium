// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_generic_reader.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "v8/include/v8.h"

namespace blink {

ReadableStreamGenericReader* ReadableStreamGenericReader::Create(
    ScriptState* script_state,
    ReadableStream* stream,
    ExceptionState& exception_state) {
  auto* reader = MakeGarbageCollected<ReadableStreamGenericReader>(
      script_state, stream, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return reader;
}

ReadableStreamGenericReader::ReadableStreamGenericReader(
    ScriptState* script_state,
    ReadableStream* stream,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#default-reader-constructor
  // 2. If ! IsReadableStreamLocked(stream) is true, throw a TypeError
  //    exception.
  if (ReadableStream::IsLocked(stream)) {
    exception_state.ThrowTypeError(
        "ReadableStreamDefaultReader constructor can only accept readable "
        "streams "
        "that are not yet locked to a reader");
    return;
  }

  // 3. Perform ! ReadableStreamReaderGenericInitialize(this, stream).
  GenericInitialize(script_state, this, stream);

  // 4. Set this.[[readRequests]] to a new empty List.
  DCHECK_EQ(read_requests_.size(), 0u);
}

ReadableStreamGenericReader::~ReadableStreamGenericReader() = default;

ScriptPromise ReadableStreamGenericReader::closed(
    ScriptState* script_state) const {
  // https://streams.spec.whatwg.org/#default-reader-closed
  //  2. Return this.[[closedPromise]].
  return closed_promise_->GetScriptPromise(script_state);
}

ScriptPromise ReadableStreamGenericReader::cancel(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return cancel(script_state,
                ScriptValue(script_state->GetIsolate(),
                            v8::Undefined(script_state->GetIsolate())),
                exception_state);
}

ScriptPromise ReadableStreamGenericReader::cancel(
    ScriptState* script_state,
    ScriptValue reason,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#default-reader-cancel
  // 2. If this.[[ownerReadableStream]] is undefined, return a promise rejected
  //    with a TypeError exception.
  if (!owner_readable_stream_) {
    exception_state.ThrowTypeError(
        "This readable stream reader has been released and cannot be used to "
        "cancel its previous owner stream");
    return ScriptPromise();
  }

  // 3. Return ! ReadableStreamReaderGenericCancel(this, reason).
  v8::Local<v8::Promise> result =
      GenericCancel(script_state, this, reason.V8Value());
  return ScriptPromise(script_state, result);
}

ScriptPromise ReadableStreamGenericReader::read(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#default-reader-read
  // 2. If this.[[ownerReadableStream]] is undefined, return a promise rejected
  //  with a TypeError exception.
  if (!owner_readable_stream_) {
    exception_state.ThrowTypeError(
        "This readable stream reader has been released and cannot be used to "
        "read from its previous owner stream");
    return ScriptPromise();
  }

  // 3. Return ! ReadableStreamReaderRead(this).
  return ReadableStreamGenericReader::Read(script_state, this)
      ->GetScriptPromise(script_state);
}

void ReadableStreamGenericReader::releaseLock(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#default-reader-release-lock
  // 2. If this.[[ownerReadableStream]] is undefined, return.
  if (!owner_readable_stream_) {
    return;
  }

  // 3. If this.[[readRequests]] is not empty, throw a TypeError exception.
  if (read_requests_.size() > 0) {
    exception_state.ThrowTypeError(
        "Cannot release a readable stream reader when it still has outstanding "
        "read() calls that have not yet settled");
    return;
  }

  // 4. Perform ! ReadableStreamReaderGenericRelease(this).
  GenericRelease(script_state, this);
}

StreamPromiseResolver* ReadableStreamGenericReader::Read(
    ScriptState* script_state,
    ReadableStreamGenericReader* reader) {
  auto* isolate = script_state->GetIsolate();
  // https://streams.spec.whatwg.org/#readable-stream-default-reader-read
  // 1. Let stream be reader.[[ownerReadableStream]].
  ReadableStream* stream = reader->owner_readable_stream_;

  // 2. Assert: stream is not undefined.
  DCHECK(stream);

  // 3. Set stream.[[disturbed]] to true.
  stream->is_disturbed_ = true;

  switch (stream->state_) {
    // 4. If stream.[[state]] is "closed", return a promise resolved with !
    //    ReadableStreamCreateReadResult(undefined, true,
    //    reader.[[forAuthorCode]]).
    case ReadableStream::kClosed:
      return StreamPromiseResolver::CreateResolved(
          script_state,
          ReadableStream::CreateReadResult(script_state, v8::Undefined(isolate),
                                           true, reader->for_author_code_));

    // 5. If stream.[[state]] is "errored", return a promise rejected with
    //    stream.[[storedError]].
    case ReadableStream::kErrored:
      return StreamPromiseResolver::CreateRejected(
          script_state, stream->GetStoredError(isolate));

    case ReadableStream::kReadable:
      // 6. Assert: stream.[[state]] is "readable".
      DCHECK_EQ(stream->state_, ReadableStream::kReadable);

      // 7. Return ! stream.[[readableStreamController]].[[PullSteps]]().
      return stream->GetController()->PullSteps(script_state);
  }
}

void ReadableStreamGenericReader::GenericRelease(
    ScriptState* script_state,
    ReadableStreamGenericReader* reader) {
  // https://streams.spec.whatwg.org/#readable-stream-reader-generic-release
  // 1. Assert: reader.[[ownerReadableStream]] is not undefined.
  DCHECK(reader->owner_readable_stream_);

  // 2. Assert: reader.[[ownerReadableStream]].[[reader]] is reader.
  DCHECK_EQ(reader->owner_readable_stream_->reader_, reader);

  auto* isolate = script_state->GetIsolate();

  // 3. If reader.[[ownerReadableStream]].[[state]] is "readable", reject
  //    reader.[[closedPromise]] with a TypeError exception.
  if (reader->owner_readable_stream_->state_ == ReadableStream::kReadable) {
    reader->closed_promise_->Reject(
        script_state,
        v8::Exception::TypeError(V8String(
            isolate,
            "This readable stream reader has been released and cannot be used "
            "to monitor the stream's state")));
  } else {
    // 4. Otherwise, set reader.[[closedPromise]] to a promise rejected with a
    //    TypeError exception.
    reader->closed_promise_ = StreamPromiseResolver::CreateRejected(
        script_state, v8::Exception::TypeError(V8String(
                          isolate,
                          "This readable stream reader has been released and "
                          "cannot be used to monitor the stream's state")));
  }

  // 5. Set reader.[[closedPromise]].[[PromiseIsHandled]] to true.
  reader->closed_promise_->MarkAsHandled(isolate);

  // 6. Set reader.[[ownerReadableStream]].[[reader]] to undefined.
  reader->owner_readable_stream_->reader_ = nullptr;

  // 7. Set reader.[[ownerReadableStream]] to undefined.
  reader->owner_readable_stream_ = nullptr;
}

void ReadableStreamGenericReader::Trace(Visitor* visitor) const {
  visitor->Trace(closed_promise_);
  visitor->Trace(owner_readable_stream_);
  visitor->Trace(read_requests_);
  ScriptWrappable::Trace(visitor);
}

v8::Local<v8::Promise> ReadableStreamGenericReader::GenericCancel(
    ScriptState* script_state,
    ReadableStreamGenericReader* reader,
    v8::Local<v8::Value> reason) {
  // https://streams.spec.whatwg.org/#readable-stream-reader-generic-cancel
  // 1. Let stream be reader.[[ownerReadableStream]].
  ReadableStream* stream = reader->owner_readable_stream_;

  // 2. Assert: stream is not undefined.
  DCHECK(stream);

  // 3. Return ! ReadableStreamCancel(stream, reason).
  return ReadableStream::Cancel(script_state, stream, reason);
}

void ReadableStreamGenericReader::GenericInitialize(
    ScriptState* script_state,
    ReadableStreamGenericReader* reader,
    ReadableStream* stream) {
  auto* isolate = script_state->GetIsolate();

  // https://streams.spec.whatwg.org/#readable-stream-reader-generic-initialize
  // 1. Set reader.[[forAuthorCode]] to true.
  DCHECK(reader->for_author_code_);

  // 2. Set reader.[[ownerReadableStream]] to stream.
  reader->owner_readable_stream_ = stream;

  // 3. Set stream.[[reader]] to reader.
  stream->reader_ = reader;

  switch (stream->state_) {
    // 4. If stream.[[state]] is "readable",
    case ReadableStream::kReadable:
      // a. Set reader.[[closedPromise]] to a new promise.
      reader->closed_promise_ =
          MakeGarbageCollected<StreamPromiseResolver>(script_state);
      break;

    // 5. Otherwise, if stream.[[state]] is "closed",
    case ReadableStream::kClosed:
      // a. Set reader.[[closedPromise]] to a promise resolved with undefined.
      reader->closed_promise_ =
          StreamPromiseResolver::CreateResolvedWithUndefined(script_state);
      break;

    // 6. Otherwise,
    case ReadableStream::kErrored:
      // a. Assert: stream.[[state]] is "errored".
      DCHECK_EQ(stream->state_, ReadableStream::kErrored);

      // b. Set reader.[[closedPromise]] to a promise rejected with stream.
      //    [[storedError]].
      reader->closed_promise_ = StreamPromiseResolver::CreateRejected(
          script_state, stream->GetStoredError(isolate));

      // c. Set reader.[[closedPromise]].[[PromiseIsHandled]] to true.
      reader->closed_promise_->MarkAsHandled(isolate);
      break;
  }
}

}  // namespace blink

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

namespace blink {

ReadableStreamDefaultReader* ReadableStreamDefaultReader::Create(
    ScriptState* script_state,
    ReadableStream* stream,
    ExceptionState& exception_state) {
  auto* reader = MakeGarbageCollected<ReadableStreamDefaultReader>(
      script_state, stream, exception_state);
  if (exception_state.HadException()) {
    return nullptr;
  }
  return reader;
}

ReadableStreamDefaultReader::ReadableStreamDefaultReader(
    ScriptState* script_state,
    ReadableStream* stream,
    ExceptionState& exception_state)
    : ExecutionContextClient(ExecutionContext::From(script_state)) {
  // https://streams.spec.whatwg.org/#default-reader-constructor
  // 1. Perform ? SetUpReadableStreamDefaultReader(this, stream).
  SetUpDefaultReader(script_state, this, stream, exception_state);
}

ReadableStreamDefaultReader::~ReadableStreamDefaultReader() = default;

ScriptPromise ReadableStreamDefaultReader::read(
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
  return Read(script_state, this)->GetScriptPromise(script_state);
}

StreamPromiseResolver* ReadableStreamDefaultReader::Read(
    ScriptState* script_state,
    ReadableStreamDefaultReader* reader) {
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

void ReadableStreamDefaultReader::ErrorReadRequests(
    ScriptState* script_state,
    ReadableStreamDefaultReader* reader,
    v8::Local<v8::Value> e) {
  // https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaultreadererrorreadrequests
  // 1. Let readRequests be reader.[[readRequests]].
  // 2. Set reader.[[readRequests]] to a new empty list.
  // 3. For each readRequest of readRequests,
  for (StreamPromiseResolver* promise : reader->read_requests_) {
    //   a. Perform readRequest’s error steps, given e.
    promise->Reject(script_state, e);
  }
  reader->read_requests_.clear();
}

void ReadableStreamDefaultReader::Release(ScriptState* script_state,
                                          ReadableStreamDefaultReader* reader) {
  // https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaultreaderrelease
  // 1. Perform ! ReadableStreamReaderGenericRelease(reader).
  ReadableStreamGenericReader::GenericRelease(script_state, reader);

  // 2. Let e be a new TypeError exception.
  v8::Local<v8::Value> e = V8ThrowException::CreateTypeError(
      script_state->GetIsolate(), "Releasing Default reader");

  // 3. Perform ! ReadableStreamDefaultReaderErrorReadRequests(reader, e).
  ErrorReadRequests(script_state, reader, e);
}

void ReadableStreamDefaultReader::releaseLock(ScriptState* script_state,
                                              ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#default-reader-release-lock
  // 1. If this.[[stream]] is undefined, return.
  if (!owner_readable_stream_) {
    return;
  }

  // 2. Perform ! ReadableStreamDefaultReaderRelease(this).
  Release(script_state, this);
}

void ReadableStreamDefaultReader::SetUpDefaultReader(
    ScriptState* script_state,
    ReadableStreamDefaultReader* reader,
    ReadableStream* stream,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#set-up-readable-stream-default-reader
  // 1. If ! IsReadableStreamLocked(stream) is true, throw a TypeError
  //    exception.
  if (ReadableStream::IsLocked(stream)) {
    exception_state.ThrowTypeError(
        "ReadableStreamDefaultReader constructor can only accept readable "
        "streams "
        "that are not yet locked to a reader");
    return;
  }

  DCHECK(reader->for_author_code_);

  // 2. Perform ! ReadableStreamReaderGenericInitialize(reader, stream).
  ReadableStreamGenericReader::GenericInitialize(script_state, reader, stream);

  // 3. Set reader.[[readRequests]] to a new empty List.
  DCHECK_EQ(reader->read_requests_.size(), 0u);
}

void ReadableStreamDefaultReader::Trace(Visitor* visitor) const {
  visitor->Trace(read_requests_);
  ReadableStreamGenericReader::Trace(visitor);
  ExecutionContextClient::Trace(visitor);
}

bool ReadableStreamDefaultReader::HasPendingActivity() const {
  return !read_requests_.empty();
}

}  // namespace blink

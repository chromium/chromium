// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_default_reader.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_readable_stream_read_result.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/streams/read_request.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"

namespace blink {

class ReadableStreamDefaultReader::DefaultReaderReadRequest final
    : public ReadRequest {
 public:
  explicit DefaultReaderReadRequest(
      ScriptPromiseResolver<ReadableStreamReadResult>* resolver)
      : resolver_(resolver) {}

  void ChunkSteps(ScriptState* script_state,
                  v8::Local<v8::Value> chunk,
                  ExceptionState&) const override {
    auto* read_result = ReadableStreamReadResult::Create();
    read_result->setValue(ScriptValue(script_state->GetIsolate(), chunk));
    read_result->setDone(false);
    resolver_->Resolve(read_result);
  }

  void CloseSteps(ScriptState* script_state) const override {
    auto* read_result = ReadableStreamReadResult::Create();
    read_result->setValue(ScriptValue(
        script_state->GetIsolate(), v8::Undefined(script_state->GetIsolate())));
    read_result->setDone(true);
    resolver_->ResolveOverridingToCurrentContext(read_result);
  }

  void ErrorSteps(ScriptState*, v8::Local<v8::Value> e) const override {
    resolver_->Reject(e);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(resolver_);
    ReadRequest::Trace(visitor);
  }

 private:
  Member<ScriptPromiseResolver<ReadableStreamReadResult>> resolver_;
};

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
    : ActiveScriptWrappable<ReadableStreamDefaultReader>({}),
      ExecutionContextClient(ExecutionContext::From(script_state)) {
  // https://streams.spec.whatwg.org/#default-reader-constructor
  // 1. Perform ? SetUpReadableStreamDefaultReader(this, stream).
  SetUpDefaultReader(script_state, this, stream, exception_state);
}

ReadableStreamDefaultReader::~ReadableStreamDefaultReader() = default;

ScriptPromise<ReadableStreamReadResult> ReadableStreamDefaultReader::read(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#default-reader-read
  // 1. If this.[[stream]] is undefined, return a promise rejected
  //  with a TypeError exception.
  if (!owner_readable_stream_) {
    exception_state.ThrowTypeError(
        "This readable stream reader has been released and cannot be used to "
        "read from its previous owner stream");
    return EmptyPromise();
  }

  if (!script_state->ContextIsValid()) {
    exception_state.ThrowTypeError("Context is detached");
    return EmptyPromise();
  }

  // 2. Let promise be a new promise.
  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver<ReadableStreamReadResult>>(
          script_state, exception_state.GetContext());
  auto promise = resolver->Promise();

  // 3. Let readRequest be a new read request with the following items:
  //    chunk steps, given chunk
  //      1. Resolve promise with «[ "value" → chunk, "done" → false ]».
  //    close steps
  //      1. Resolve promise with «[ "value" → undefined, "done" → true ]».
  //    error steps, given e
  //      1. Reject promise with e.
  auto* read_request = MakeGarbageCollected<DefaultReaderReadRequest>(resolver);

  // 4. Perform ! ReadableStreamReaderRead(this).
  Read(script_state, this, read_request, exception_state);

  // 5. Return promise.
  return resolver->Promise();
}

void ReadableStreamDefaultReader::Read(ScriptState* script_state,
                                       ReadableStreamDefaultReader* reader,
                                       ReadRequest* read_request,
                                       ExceptionState& exception_state) {
  auto* isolate = script_state->GetIsolate();
  // https://streams.spec.whatwg.org/#readable-stream-default-reader-read
  // 1. Let stream be reader.[[stream]].
  ReadableStream* stream = reader->owner_readable_stream_;

  // 2. Assert: stream is not undefined.
  DCHECK(stream);

  // 3. Set stream.[[disturbed]] to true.
  stream->is_disturbed_ = true;

  switch (stream->state_) {
    // 4. If stream.[[state]] is "closed", perform readRequest's close steps.
    case ReadableStream::kClosed:
      read_request->CloseSteps(script_state);
      break;

    // 5. Otherwise, if stream.[[state]] is "errored", perform readRequest's
    // error steps
    //    given stream.[[storedError]].
    case ReadableStream::kErrored:
      read_request->ErrorSteps(script_state, stream->GetStoredError(isolate));
      break;

    case ReadableStream::kReadable:
      // 6. Otherwise,
      //   1. Assert: stream.[[state]] is "readable".
      DCHECK_EQ(stream->state_, ReadableStream::kReadable);

      //   2. Perform ! stream.[[controller]].[[PullSteps]](readRequest).
      stream->GetController()->PullSteps(script_state, read_request,
                                         exception_state);
      break;
  }
}

void ReadableStreamDefaultReader::ErrorReadRequests(
    ScriptState* script_state,
    ReadableStreamDefaultReader* reader,
    v8::Local<v8::Value> e) {
  // https://streams.spec.whatwg.org/#abstract-opdef-readablestreamdefaultreadererrorreadrequests
  // 1. Let readRequests be reader.[[readRequests]].
  // 2. Set reader.[[readRequests]] to a new empty list.
  HeapDeque<Member<ReadRequest>> read_requests;
  read_requests.Swap(reader->read_requests_);
  // 3. For each readRequest of readRequests,
  for (ReadRequest* read_request : read_requests) {
    //   a. Perform readRequest’s error steps, given e.
    read_request->ErrorSteps(script_state, e);
  }
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

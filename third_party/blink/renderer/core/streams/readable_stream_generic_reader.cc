// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_generic_reader.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/readable_stream_default_controller.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"
#include "v8/include/v8.h"

namespace blink {

ReadableStreamGenericReader::ReadableStreamGenericReader() = default;

ReadableStreamGenericReader::~ReadableStreamGenericReader() = default;

ScriptPromise<IDLUndefined> ReadableStreamGenericReader::closed(
    ScriptState*) const {
  // https://streams.spec.whatwg.org/#default-reader-closed
  // 1. Return this.[[closedPromise]].
  return closed_promise_;
}

ScriptPromise<IDLUndefined> ReadableStreamGenericReader::cancel(
    ScriptState* script_state,
    ExceptionState& exception_state) {
  return cancel(script_state,
                ScriptValue(script_state->GetIsolate(),
                            v8::Undefined(script_state->GetIsolate())),
                exception_state);
}

ScriptPromise<IDLUndefined> ReadableStreamGenericReader::cancel(
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
    return EmptyPromise();
  }

  // 3. Return ! ReadableStreamReaderGenericCancel(this, reason).
  return GenericCancel(script_state, this, reason.V8Value());
}

void ReadableStreamGenericReader::GenericRelease(
    ScriptState* script_state,
    ReadableStreamGenericReader* reader) {
  // https://streams.spec.whatwg.org/#readable-stream-reader-generic-release
  // 1. Let stream be reader.[[stream]].
  ReadableStream* stream = reader->owner_readable_stream_;

  // 2. Assert: stream is not undefined.
  DCHECK(stream);

  // 3. Assert: stream.[[reader]] is reader.
  DCHECK_EQ(stream->reader_, reader);

  auto* isolate = script_state->GetIsolate();

  // 4. If stream.[[state]] is "readable", reject reader.[[closedPromise]] with
  // a TypeError exception.
  // 5. Otherwise, set reader.[[closedPromise]] to a promise rejected with a
  // TypeError exception.
  if (stream->state_ != ReadableStream::kReadable) {
    reader->closed_resolver_ =
        MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
    reader->closed_promise_ = reader->closed_resolver_->Promise();
  }
  reader->closed_promise_.MarkAsSilent();
  reader->closed_resolver_->Reject(v8::Exception::TypeError(V8String(
      isolate,
      "This readable stream reader has been released and cannot be used "
      "to monitor the stream's state")));

  // 6. Set reader.[[closedPromise]].[[PromiseIsHandled]] to true.
  reader->closed_promise_.MarkAsHandled();

  // 7. Perform ! stream.[[controller]].[[ReleaseSteps]]().
  stream->readable_stream_controller_->ReleaseSteps();

  // 8. Set stream.[[reader]] to undefined.
  stream->reader_ = nullptr;

  // 9. Set reader.[[stream]] to undefined.
  reader->owner_readable_stream_ = nullptr;
}

void ReadableStreamGenericReader::Trace(Visitor* visitor) const {
  visitor->Trace(closed_resolver_);
  visitor->Trace(closed_promise_);
  visitor->Trace(owner_readable_stream_);
  ScriptWrappable::Trace(visitor);
}

ScriptPromise<IDLUndefined> ReadableStreamGenericReader::GenericCancel(
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
  // 1. Set reader.[[ownerReadableStream]] to stream.
  reader->owner_readable_stream_ = stream;

  // 2. Set stream.[[reader]] to reader.
  stream->reader_ = reader;
  reader->closed_resolver_ =
      MakeGarbageCollected<ScriptPromiseResolver<IDLUndefined>>(script_state);
  reader->closed_promise_ = reader->closed_resolver_->Promise();

  switch (stream->state_) {
    // 3. If stream.[[state]] is "readable",
    case ReadableStream::kReadable:
      // a. Set reader.[[closedPromise]] to a new promise.
      break;

    // 4. Otherwise, if stream.[[state]] is "closed",
    case ReadableStream::kClosed:
      // a. Set reader.[[closedPromise]] to a promise resolved with undefined.
      reader->closed_resolver_->Resolve();
      break;

    // 5. Otherwise,
    case ReadableStream::kErrored:
      // a. Assert: stream.[[state]] is "errored".
      DCHECK_EQ(stream->state_, ReadableStream::kErrored);

      // b. Set reader.[[closedPromise]] to a promise rejected with stream.
      //    [[storedError]].
      reader->closed_promise_.MarkAsSilent();
      reader->closed_resolver_->Reject(stream->GetStoredError(isolate));

      // c. Set reader.[[closedPromise]].[[PromiseIsHandled]] to true.
      reader->closed_promise_.MarkAsHandled();
      break;
  }
}

}  // namespace blink

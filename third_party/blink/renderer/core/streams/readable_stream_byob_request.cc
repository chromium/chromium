// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_byob_request.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

ReadableStreamBYOBRequest::ReadableStreamBYOBRequest(
    ReadableByteStreamController* controller,
    NotShared<DOMUint8Array> view)
    : controller_(controller), view_(view) {}

NotShared<DOMArrayBufferView> ReadableStreamBYOBRequest::view() const {
  // https://streams.spec.whatwg.org/#rs-byob-request-view
  // 1. Return this.[[view]].
  return view_;
}

void ReadableStreamBYOBRequest::respond(ScriptState* script_state,
                                        uint64_t bytes_written,
                                        ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-byob-request-respond
  // 1. If this.[[controller]] is undefined, throw a TypeError exception.
  if (!controller_) {
    exception_state.ThrowTypeError(
        "Cannot respond to an invalidated ReadableStreamBYOBRequest");
    return;
  }
  // 2. If ! IsDetachedBuffer(this.[[view]].[[ArrayBuffer]]) is true, throw a
  // TypeError exception.
  if (view_->buffer()->IsDetached()) {
    exception_state.ThrowTypeError("ArrayBufferView is detached");
    return;
  }
  // 3. Assert: this.[[view]].[[ByteLength]] > 0.
  DCHECK_GT(view_->byteLength(), 0u);
  // 4. Assert: this.[[view]].[[ViewedArrayBuffer]].[[ByteLength]] > 0.
  DCHECK_GT(view_->buffer()->ByteLength(), 0.0);
  // 5. Perform ? ReadableByteStreamControllerRespond(this.[[controller]],
  // bytesWritten).
  ReadableByteStreamController::Respond(script_state, controller_,
                                        static_cast<size_t>(bytes_written),
                                        exception_state);
}

void ReadableStreamBYOBRequest::respondWithNewView(
    ScriptState* script_state,
    NotShared<DOMArrayBufferView> view,
    ExceptionState& exception_state) {
  // https://streams.spec.whatwg.org/#rs-byob-request-respond-with-new-view
  // 1. If this.[[controller]] is undefined, throw a TypeError exception.
  if (!controller_) {
    exception_state.ThrowTypeError(
        "Cannot respond to an invalidated ReadableStreamBYOBRequest");
    return;
  }
  // 2. If ! IsDetachedBuffer(view.[[ViewedArrayBuffer]]) is true, throw a
  // TypeError exception.
  if (view->buffer()->IsDetached()) {
    exception_state.ThrowTypeError("ViewedArrayBuffer is detached");
    return;
  }
  // 3. Return ?
  // ReadableByteStreamControllerRespondWithNewView(this.[[controller]], view).
  ReadableByteStreamController::RespondWithNewView(script_state, controller_,
                                                   view, exception_state);
}

void ReadableStreamBYOBRequest::Trace(Visitor* visitor) const {
  visitor->Trace(controller_);
  visitor->Trace(view_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

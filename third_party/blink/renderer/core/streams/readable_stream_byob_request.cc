// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_byob_request.h"

#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

NotShared<DOMArrayBufferView> ReadableStreamBYOBRequest::view(
    ExceptionState& exception_state) const {
  ThrowUnimplemented(exception_state);
  return NotShared<DOMArrayBufferView>();
}

void ReadableStreamBYOBRequest::respond(ScriptState* script_state,
                                        uint64_t bytesWritten,
                                        ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return;
}

void ReadableStreamBYOBRequest::respondWithNewView(
    ScriptState* script_state,
    NotShared<DOMArrayBufferView> view,
    ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return;
}

void ReadableStreamBYOBRequest::ThrowUnimplemented(
    ExceptionState& exception_state) {
  exception_state.ThrowTypeError("unimplemented");
}

}  // namespace blink

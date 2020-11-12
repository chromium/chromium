// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_stream_byob_reader.h"

#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

ReadableStreamBYOBReader* ReadableStreamBYOBReader::Create(
    ScriptState* script_state,
    ReadableStream* stream,
    ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return nullptr;
}

ReadableStreamBYOBReader::ReadableStreamBYOBReader(
    ScriptState* script_state,
    ReadableStream* stream,
    ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return;
}

ReadableStreamBYOBReader::~ReadableStreamBYOBReader() = default;

ScriptPromise ReadableStreamBYOBReader::read(ScriptState* script_state,
                                             NotShared<DOMArrayBufferView> view,
                                             ExceptionState& exception_state) {
  return RejectUnimplemented(script_state);
}

void ReadableStreamBYOBReader::releaseLock(ScriptState* script_state,
                                           ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return;
}

void ReadableStreamBYOBReader::ThrowUnimplemented(
    ExceptionState& exception_state) {
  exception_state.ThrowTypeError("unimplemented");
}

ScriptPromise ReadableStreamBYOBReader::RejectUnimplemented(
    ScriptState* script_state) {
  return StreamPromiseResolver::CreateRejected(
             script_state, v8::Exception::TypeError(V8String(
                               script_state->GetIsolate(), "unimplemented")))
      ->GetScriptPromise(script_state);
}

}  // namespace blink

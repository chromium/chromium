// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/readable_byte_stream_controller.h"

#include "third_party/blink/renderer/core/streams/readable_stream_byob_request.h"
#include "third_party/blink/renderer/core/streams/stream_promise_resolver.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_binding.h"

namespace blink {

ReadableStreamBYOBRequest* ReadableByteStreamController::byobRequest(
    ExceptionState& exception_state) const {
  ThrowUnimplemented(exception_state);
  return nullptr;
}

base::Optional<double> ReadableByteStreamController::desiredSize(
    ExceptionState& exception_state) const {
  ThrowUnimplemented(exception_state);
  return base::nullopt;
}

void ReadableByteStreamController::close(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return;
}

void ReadableByteStreamController::enqueue(ScriptState* script_state,
                                           NotShared<DOMArrayBufferView> chunk,
                                           ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return;
}

void ReadableByteStreamController::error(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return;
}

void ReadableByteStreamController::error(ScriptState* script_state,
                                         ScriptValue e,
                                         ExceptionState& exception_state) {
  ThrowUnimplemented(exception_state);
  return;
}

//
// Readable byte stream controller internal methods
//

v8::Local<v8::Promise> ReadableByteStreamController::CancelSteps(
    ScriptState* script_state,
    v8::Local<v8::Value> reason) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kExecutionContext, nullptr,
                                 nullptr);
  ThrowUnimplemented(exception_state);
  return v8::Local<v8::Promise>();
}

StreamPromiseResolver* ReadableByteStreamController::PullSteps(
    ScriptState* script_state) {
  ExceptionState exception_state(script_state->GetIsolate(),
                                 ExceptionState::kExecutionContext, nullptr,
                                 nullptr);
  ThrowUnimplemented(exception_state);
  return nullptr;
}

void ReadableByteStreamController::ThrowUnimplemented(
    ExceptionState& exception_state) {
  exception_state.ThrowTypeError("unimplemented");
}

}  // namespace blink

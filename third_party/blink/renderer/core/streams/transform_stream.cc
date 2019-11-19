// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/streams/transform_stream.h"

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/core/streams/readable_stream.h"
#include "third_party/blink/renderer/core/streams/transform_stream_native.h"
#include "third_party/blink/renderer/core/streams/transform_stream_transformer.h"
#include "third_party/blink/renderer/core/streams/writable_stream.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"

namespace blink {

TransformStream::TransformStream() = default;
TransformStream::TransformStream(ReadableStream* readable,
                                 WritableStream* writable)
    : readable_(readable), writable_(writable) {}

TransformStream::~TransformStream() = default;

TransformStream* TransformStream::Create(ScriptState* script_state,
                                         ExceptionState& exception_state) {
  ScriptValue undefined(script_state->GetIsolate(),
                        v8::Undefined(script_state->GetIsolate()));
  return Create(script_state, undefined, undefined, undefined, exception_state);
}

TransformStream* TransformStream::Create(
    ScriptState* script_state,
    ScriptValue transform_stream_transformer,
    ExceptionState& exception_state) {
  ScriptValue undefined(script_state->GetIsolate(),
                        v8::Undefined(script_state->GetIsolate()));
  return Create(script_state, transform_stream_transformer, undefined,
                undefined, exception_state);
}

TransformStream* TransformStream::Create(
    ScriptState* script_state,
    ScriptValue transform_stream_transformer,
    ScriptValue writable_strategy,
    ExceptionState& exception_state) {
  ScriptValue undefined(script_state->GetIsolate(),
                        v8::Undefined(script_state->GetIsolate()));
  return Create(script_state, transform_stream_transformer, writable_strategy,
                undefined, exception_state);
}

TransformStream* TransformStream::Create(ScriptState* script_state,
                                         ScriptValue transformer,
                                         ScriptValue writable_strategy,
                                         ScriptValue readable_strategy,
                                         ExceptionState& exception_state) {
  auto* ts = MakeGarbageCollected<TransformStream>();

  TransformStreamNative::InitFromJS(
      script_state, transformer, writable_strategy, readable_strategy,
      &ts->readable_, &ts->writable_, exception_state);

  if (exception_state.HadException()) {
    return nullptr;
  }

  return ts;
}

void TransformStream::Init(TransformStreamTransformer* transformer,
                           ScriptState* script_state,
                           ExceptionState& exception_state) {
  TransformStreamNative::Init(script_state, transformer, &readable_, &writable_,
                              exception_state);

  if (exception_state.HadException()) {
    return;
  }
}

void TransformStream::Trace(Visitor* visitor) {
  visitor->Trace(readable_);
  visitor->Trace(writable_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

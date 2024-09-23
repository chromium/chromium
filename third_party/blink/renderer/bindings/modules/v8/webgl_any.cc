// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/bindings/modules/v8/webgl_any.h"

#include "base/containers/span.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

ScriptValue WebGLAny(ScriptState* script_state, bool value) {
  return ScriptValue(script_state->GetIsolate(),
                     v8::Boolean::New(script_state->GetIsolate(), value));
}

ScriptValue WebGLAny(ScriptState* script_state,
                     const bool* value,
                     uint32_t size) {
  auto span = base::make_span(value, size);
  return ScriptValue(
      script_state->GetIsolate(),
      ToV8Traits<IDLSequence<IDLBoolean>>::ToV8(script_state, span));
}

ScriptValue WebGLAny(ScriptState* script_state, const Vector<bool>& value) {
  return ScriptValue(
      script_state->GetIsolate(),
      ToV8Traits<IDLSequence<IDLBoolean>>::ToV8(script_state, value));
}

ScriptValue WebGLAny(ScriptState* script_state, const Vector<unsigned>& value) {
  return ScriptValue(
      script_state->GetIsolate(),
      ToV8Traits<IDLSequence<IDLUnsignedShort>>::ToV8(script_state, value));
}

ScriptValue WebGLAny(ScriptState* script_state, const Vector<int>& value) {
  return ScriptValue(
      script_state->GetIsolate(),
      ToV8Traits<IDLSequence<IDLLong>>::ToV8(script_state, value));
}

ScriptValue WebGLAny(ScriptState* script_state, int value) {
  return ScriptValue(script_state->GetIsolate(),
                     v8::Integer::New(script_state->GetIsolate(), value));
}

ScriptValue WebGLAny(ScriptState* script_state, unsigned value) {
  return ScriptValue(
      script_state->GetIsolate(),
      v8::Integer::NewFromUnsigned(script_state->GetIsolate(),
                                   static_cast<unsigned>(value)));
}

ScriptValue WebGLAny(ScriptState* script_state, int64_t value) {
  return ScriptValue(
      script_state->GetIsolate(),
      v8::Number::New(script_state->GetIsolate(), static_cast<double>(value)));
}

ScriptValue WebGLAny(ScriptState* script_state, uint64_t value) {
  return ScriptValue(
      script_state->GetIsolate(),
      v8::Number::New(script_state->GetIsolate(), static_cast<double>(value)));
}

ScriptValue WebGLAny(ScriptState* script_state, float value) {
  return ScriptValue(script_state->GetIsolate(),
                     v8::Number::New(script_state->GetIsolate(), value));
}

ScriptValue WebGLAny(ScriptState* script_state, String value) {
  return ScriptValue(script_state->GetIsolate(),
                     V8String(script_state->GetIsolate(), value));
}

ScriptValue WebGLAny(ScriptState* script_state, WebGLObject* value) {
  return ScriptValue(
      script_state->GetIsolate(),
      ToV8Traits<IDLNullable<WebGLObject>>::ToV8(script_state, value));
}

ScriptValue WebGLAny(ScriptState* script_state, DOMFloat32Array* value) {
  return ScriptValue(script_state->GetIsolate(),
                     ToV8Traits<DOMFloat32Array>::ToV8(script_state, value));
}

ScriptValue WebGLAny(ScriptState* script_state, DOMInt32Array* value) {
  return ScriptValue(script_state->GetIsolate(),
                     ToV8Traits<DOMInt32Array>::ToV8(script_state, value));
}

ScriptValue WebGLAny(ScriptState* script_state, DOMUint8Array* value) {
  return ScriptValue(script_state->GetIsolate(),
                     ToV8Traits<DOMUint8Array>::ToV8(script_state, value));
}

ScriptValue WebGLAny(ScriptState* script_state, DOMUint32Array* value) {
  return ScriptValue(script_state->GetIsolate(),
                     ToV8Traits<DOMUint32Array>::ToV8(script_state, value));
}

}  // namespace blink

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_ITERATOR_RESULT_VALUE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_ITERATOR_RESULT_VALUE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "v8/include/v8.h"

namespace blink {

// "Iterator result" in this file is an object returned from iterator.next()
// having two members "done" and "value".

CORE_EXPORT v8::Local<v8::Object> V8IteratorResultValue(ScriptState*,
                                                        bool done,
                                                        v8::Local<v8::Value>);

// Unpacks |result|, stores the value of "done" member to |done| and returns
// the value of "value" member. Returns an empty handle when errored.
CORE_EXPORT v8::MaybeLocal<v8::Value>
V8UnpackIteratorResult(ScriptState*, v8::Local<v8::Object> result, bool* done);

template <typename T>
inline ScriptValue V8IteratorResult(ScriptState* script_state, const T& value) {
  return ScriptValue(
      script_state,
      V8IteratorResultValue(script_state, false,
                            ToV8(value, script_state->GetContext()->Global(),
                                 script_state->GetIsolate())));
}

inline ScriptValue V8IteratorResultDone(ScriptState* script_state) {
  return ScriptValue(
      script_state,
      V8IteratorResultValue(script_state, true,
                            v8::Undefined(script_state->GetIsolate())));
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_V8_ITERATOR_RESULT_VALUE_H_

// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_MAPLIKE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_MAPLIKE_H_

#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"

namespace blink {

template <typename KeyType, typename ValueType>
class Maplike : public PairIterable<KeyType, ValueType> {
 public:
  bool hasForBinding(ScriptState* script_state,
                     const KeyType& key,
                     ExceptionState& exception_state) {
    ValueType value;
    return GetMapEntry(script_state, key, value, exception_state);
  }

  ScriptValue getForBinding(ScriptState* script_state,
                            const KeyType& key,
                            ExceptionState& exception_state) {
    ValueType value;
    if (GetMapEntry(script_state, key, value, exception_state))
      return ScriptValue(script_state->GetIsolate(),
                         ToV8(value, script_state->GetContext()->Global(),
                              script_state->GetIsolate()));
    return ScriptValue(script_state->GetIsolate(),
                       v8::Undefined(script_state->GetIsolate()));
  }

 private:
  virtual bool GetMapEntry(ScriptState*,
                           const KeyType&,
                           ValueType&,
                           ExceptionState&) = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_MAPLIKE_H_

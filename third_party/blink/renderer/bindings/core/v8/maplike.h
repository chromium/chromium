// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_MAPLIKE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_MAPLIKE_H_

#include "third_party/blink/renderer/bindings/core/v8/iterable.h"
#include "third_party/blink/renderer/bindings/core/v8/script_value.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_for_core.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"

namespace blink {

namespace bindings {

template <typename IDLKeyType,
          typename IDLValueType,
          typename KeyType,
          typename ValueType>
class MaplikeReadAcccess {
 public:
  // https://webidl.spec.whatwg.org/#es-map-get
  v8::Local<v8::Value> getForBinding(ScriptState* script_state,
                                     const KeyType& key,
                                     ExceptionState& exception_state) {
    IDLTypeDefaultConstructible<ValueType> value;
    if (!GetMapEntry(script_state, key, value.content, exception_state)) {
      return v8::Undefined(script_state->GetIsolate());
    }
    return ToV8Traits<IDLValueType>::ToV8(script_state, value.content)
        .ToLocalChecked();
  }

  // https://webidl.spec.whatwg.org/#es-map-has
  bool hasForBinding(ScriptState* script_state,
                     const KeyType& key,
                     ExceptionState& exception_state) {
    IDLTypeDefaultConstructible<ValueType> unused_value;
    return GetMapEntry(script_state, key, unused_value.content,
                       exception_state);
  }

 private:
  virtual bool GetMapEntry(ScriptState* script_state,
                           const KeyType& key,
                           ValueType& value,
                           ExceptionState& exception_state) = 0;
};

}  // namespace bindings

template <typename IDLInterface>
class MaplikeReadAPIs
    : public PairSyncIterable<IDLInterface>,
      public bindings::MaplikeReadAcccess<
          typename PairSyncIterable<IDLInterface>::IDLKeyType,
          typename PairSyncIterable<IDLInterface>::IDLValueType,
          typename PairSyncIterable<IDLInterface>::KeyType,
          typename PairSyncIterable<IDLInterface>::ValueType> {};

// KeyType and ValueType define the key and value types correspondingly.
// IDLKey and IDLValue only define the types of
// ToV8Traits<[IDLKey,IDLValue]>::ToV8 converters.
template <typename KeyType,
          typename IDLKeyType,
          typename ValueType,
          typename IDLValueType>
class Maplike
    : public PairIterable<KeyType, IDLKeyType, ValueType, IDLValueType> {
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

// KeyType and ValueType define the key and value types correspondingly.
// IDLKey and IDLValue only define the types of
// ToV8Traits<[IDLKey,IDLValue]>::ToV8 converters.
template <typename KeyType, typename IDLKeyType>
class Setlike : public Maplike<KeyType, IDLKeyType, KeyType, IDLKeyType> {};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_MAPLIKE_H_

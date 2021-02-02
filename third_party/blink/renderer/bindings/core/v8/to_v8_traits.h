// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_TO_V8_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_TO_V8_TRAITS_H_

#include "third_party/blink/renderer/bindings/core/v8/idl_dictionary_base.h"
#include "third_party/blink/renderer/platform/bindings/to_v8_traits.h"

namespace blink {

// Partial specialization of ToV8Traits<> in platform/bindings/to_v8_traits.h.
// We support the types which depend on core/.

// Old implementation of Dictionary
template <typename T>
struct ToV8Traits<
    T,
    typename std::enable_if_t<std::is_base_of<IDLDictionaryBase, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        const IDLDictionaryBase* dictionary) {
    DCHECK(dictionary);
    return dictionary->ToV8Impl(script_state->GetContext(),
                                script_state->GetIsolate());
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_TO_V8_TRAITS_H_

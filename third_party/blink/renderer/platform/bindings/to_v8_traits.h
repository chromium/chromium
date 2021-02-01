// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_TO_V8_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_TO_V8_TRAITS_H_

#include "third_party/blink/renderer/platform/bindings/dom_data_store.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "v8/include/v8.h"

namespace blink {

// ToV8Traits provides C++ -> V8 conversion.
// Currently, you can use ToV8() which is defined in to_v8.h for this
// conversion, but it cannot throw an exception when an error occurs.
// We will solve this problem and replace ToV8() in to_v8.h with
// ToV8Traits::ToV8().

// Primary template for ToV8Traits.
template <typename T, typename SFINAEHelper = void>
struct ToV8Traits;

// ScriptWrappable
template <typename T>
struct ToV8Traits<
    T,
    typename std::enable_if_t<std::is_base_of<ScriptWrappable, T>::value>> {
  static v8::MaybeLocal<v8::Value> ToV8(ScriptState* script_state,
                                        ScriptWrappable* impl) {
    return ToV8(script_state->GetIsolate(), impl,
                script_state->GetContext()->Global());
  }

  // This overload is used for the case when the ToV8() caller already has
  // a receiver object (a creation context object) which is needed to create
  // a wrapper.
  static v8::MaybeLocal<v8::Value> ToV8(
      v8::Isolate* isolate,
      ScriptWrappable* impl,
      v8::Local<v8::Object> creation_context) {
    if (UNLIKELY(!impl)) {
      return v8::Null(isolate);
    }
    v8::Local<v8::Value> wrapper = DOMDataStore::GetWrapper(impl, isolate);
    if (!wrapper.IsEmpty()) {
      return wrapper;
    }

    if (!impl->WrapV2(isolate, creation_context).ToLocal(&wrapper)) {
      return v8::MaybeLocal<v8::Value>();
    }
    return wrapper;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_BINDINGS_TO_V8_TRAITS_H_

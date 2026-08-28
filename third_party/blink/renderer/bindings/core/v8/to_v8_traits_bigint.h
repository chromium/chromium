// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_TO_V8_TRAITS_BIGINT_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_TO_V8_TRAITS_BIGINT_H_

#include <optional>

#include "third_party/blink/renderer/bindings/core/v8/idl_types.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/platform/bindings/bigint.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "v8/include/v8.h"

namespace blink {

template <>
struct ToV8Traits<IDLBigint> {
  [[nodiscard]] static v8::Local<v8::Value> ToV8(ScriptState* script_state,
                                                 const BigInt& bigint) {
    return bigint.ToV8(script_state->GetContext());
  }
};

// Nullable Bigints
template <>
struct ToV8Traits<IDLNullable<IDLBigint>> {
  [[nodiscard]] static v8::Local<v8::Value> ToV8(
      ScriptState* script_state,
      const std::optional<BigInt>& value) {
    if (!value) {
      return v8::Null(script_state->GetIsolate());
    }
    return ToV8Traits<IDLBigint>::ToV8(script_state, *value);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_TO_V8_TRAITS_BIGINT_H_

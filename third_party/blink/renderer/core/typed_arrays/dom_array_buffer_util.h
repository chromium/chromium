// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_UTIL_H_

#include <concepts>
#include <cstdint>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer/array_buffer_contents.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_base.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/platform/bindings/union_base.h"
#include "v8/include/v8-forward.h"

namespace blink {

class ExceptionState;

enum class SharedBufferPolicy { kAllow, kDisallow };

template <typename Union, SharedBufferPolicy policy>
concept ArrayBufferOrViewUnion =
    std::derived_from<Union, bindings::UnionBase> &&
    (requires(const Union& u) { u.IsArrayBuffer(); } ||
     requires(const Union& u) { u.IsArrayBufferView(); } ||
     (policy == SharedBufferPolicy::kAllow &&
      (requires(const Union& u) { u.IsArrayBufferAllowShared(); } ||
       requires(const Union& u) { u.IsArrayBufferViewAllowShared(); })));

template <SharedBufferPolicy policy, ArrayBufferOrViewUnion<policy> Union>
base::span<uint8_t> AsSpan(Union& u) {
  if constexpr (requires { u.IsArrayBuffer(); }) {
    if (u.IsArrayBuffer()) {
      return u.GetAsArrayBuffer()->ByteSpan();
    }
  }
  if constexpr (requires { u.IsArrayBufferView(); }) {
    if (u.IsArrayBufferView()) {
      return u.GetAsArrayBufferView()->ByteSpan();
    }
  }
  if constexpr (policy == SharedBufferPolicy::kAllow) {
    if constexpr (requires { u.IsArrayBufferAllowShared(); }) {
      if (u.IsArrayBufferAllowShared()) {
        return u.GetAsArrayBufferAllowShared()->ByteSpanMaybeShared();
      }
    }
    if constexpr (requires { u.IsArrayBufferViewAllowShared(); }) {
      if (u.IsArrayBufferViewAllowShared()) {
        return u.GetAsArrayBufferViewAllowShared()->ByteSpanMaybeShared();
      }
    }
  }
  NOTREACHED() << "Union contains a non-array member";
}

template <SharedBufferPolicy policy, ArrayBufferOrViewUnion<policy> Union>
base::span<const uint8_t> AsSpan(const Union& u) {
  return AsSpan<policy>(const_cast<Union&>(u));
}

// Returns the array buffer view held by `u`, or, if `u` holds an array buffer,
// a newly created Uint8Array view spanning the entire buffer.
template <typename Union>
  requires std::derived_from<Union, bindings::UnionBase> &&
           requires(const Union& u) {
             u.IsArrayBufferAllowShared();
             u.IsArrayBufferViewAllowShared();
           }
DOMArrayBufferView* CoerceToArrayBufferView(const Union& u) {
  if (u.IsArrayBufferViewAllowShared()) {
    return u.GetAsArrayBufferViewAllowShared().Get();
  }
  CHECK(u.IsArrayBufferAllowShared()) << "Union contains a non-array member";
  DOMArrayBufferBase* buffer = u.GetAsArrayBufferAllowShared();
  return DOMUint8Array::Create(buffer, /*byte_offset=*/0, buffer->ByteLength());
}

// Transfers the ArrayBuffer backing `view` into an ArrayBufferContents and
// returns it along with the span of the transferred contents that `view`
// referred to. `view` must not be backed by a SharedArrayBuffer. On failure, an
// exception is thrown via `exception_state` and an invalid ArrayBufferContents
// with an empty span is returned.
CORE_EXPORT std::pair<ArrayBufferContents, base::span<uint8_t>>
TransferArrayBufferView(v8::Isolate* isolate,
                        DOMArrayBufferView* view,
                        ExceptionState& exception_state);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_UTIL_H_

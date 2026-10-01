// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_UTIL_H_

#include <concepts>
#include <cstdint>

#include "base/containers/span.h"
#include "base/notreached.h"
#include "third_party/blink/renderer/core/typed_arrays/array_buffer_view_helpers.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_view.h"
#include "third_party/blink/renderer/platform/bindings/union_base.h"

namespace blink {

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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_TYPED_ARRAYS_DOM_ARRAY_BUFFER_UTIL_H_

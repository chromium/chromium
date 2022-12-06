// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ALLOW_SHARED_BUFFER_SOURCE_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ALLOW_SHARED_BUFFER_SOURCE_UTIL_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybufferallowshared_arraybufferviewallowshared.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"

namespace blink {

using AllowSharedBufferSource =
    V8UnionArrayBufferAllowSharedOrArrayBufferViewAllowShared;

// Helper function for turning various DOMArray-like things into a pointer+size.
template <typename T>
base::span<T> AsSpan(const AllowSharedBufferSource* buffer_union) {
  switch (buffer_union->GetContentType()) {
    case AllowSharedBufferSource::ContentType::kArrayBufferAllowShared: {
      auto* buffer = buffer_union->GetAsArrayBufferAllowShared();
      return (buffer && !buffer->IsDetached())
                 ? base::span<T>(
                       reinterpret_cast<T*>(buffer->DataMaybeShared()),
                       buffer->ByteLength())
                 : base::span<T>();
    }
    case AllowSharedBufferSource::ContentType::kArrayBufferViewAllowShared: {
      auto* buffer = buffer_union->GetAsArrayBufferViewAllowShared().Get();
      return (buffer && !buffer->IsDetached())
                 ? base::span<T>(
                       reinterpret_cast<T*>(buffer->BaseAddressMaybeShared()),
                       buffer->byteLength())
                 : base::span<T>();
    }
  }
}

// Ensures that the underlying memory for `buffer_union` remains valid
// (owned by a returned instance of ArrayBufferContents)
// even if the buffer is detached or truncated by client activity.
ArrayBufferContents PinArrayBufferContent(
    const AllowSharedBufferSource* buffer_union);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ALLOW_SHARED_BUFFER_SOURCE_UTIL_H_

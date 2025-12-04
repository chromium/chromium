// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_ARRAY_BUFFER_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_ARRAY_BUFFER_UTIL_H_

#include "base/containers/span.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybufferallowshared_arraybufferviewallowshared.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"

namespace blink {

using AllowSharedBufferSource =
    V8UnionArrayBufferAllowSharedOrArrayBufferViewAllowShared;

// Helper function for turning various DOMArray-like things into a span.
base::span<uint8_t> RtcTransportBufferSourceAsByteSpan(
    const AllowSharedBufferSource& buffer_union);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_RTC_TRANSPORT_ARRAY_BUFFER_UTIL_H_

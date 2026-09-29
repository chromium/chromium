// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/array_buffer_util.h"

#include <cstdint>

#include "base/containers/span.h"

namespace blink {

// Helper function for turning various DOMArray-like things into a span.
base::span<uint8_t> RtcTransportBufferSourceAsByteSpan(
    const AllowSharedBufferSource& buffer_union) {
  switch (buffer_union.GetContentType()) {
    case AllowSharedBufferSource::ContentType::kArrayBufferAllowShared:
      return buffer_union.GetAsArrayBufferAllowShared()->ByteSpanMaybeShared();
    case AllowSharedBufferSource::ContentType::kArrayBufferViewAllowShared:
      return buffer_union.GetAsArrayBufferViewAllowShared()
          ->ByteSpanMaybeShared();
  }
}

}  // namespace blink

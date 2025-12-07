// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_transport/array_buffer_util.h"

namespace blink {

// Helper function for turning various DOMArray-like things into a span.
base::span<uint8_t> RtcTransportBufferSourceAsByteSpan(
    const AllowSharedBufferSource& buffer_union) {
  switch (buffer_union.GetContentType()) {
    case AllowSharedBufferSource::ContentType::kArrayBufferAllowShared: {
      auto* buffer = buffer_union.GetAsArrayBufferAllowShared();
      return (buffer && !buffer->IsDetached()) ? buffer->ByteSpanMaybeShared()
                                               : base::span<uint8_t>();
    }
    case AllowSharedBufferSource::ContentType::kArrayBufferViewAllowShared: {
      auto* buffer = buffer_union.GetAsArrayBufferViewAllowShared().Get();
      return (buffer && !buffer->IsDetached()) ? buffer->ByteSpanMaybeShared()
                                               : base::span<uint8_t>();
    }
  }
}

}  // namespace blink

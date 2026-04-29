// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBRTC_RTC_LOGGING_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBRTC_RTC_LOGGING_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/map_traits_wtf_hash_map.h"
#include "mojo/public/cpp/bindings/string_traits_wtf.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/blink/public/common/webrtc/rtc_logging_utils.h"
#include "third_party/blink/public/mojom/webrtc/rtc_logging.mojom-shared.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"

namespace mojo {

template <>
struct StructTraits<blink::mojom::RTCMetadataDataView,
                    blink::HashMap<blink::String, blink::String>> {
  static const blink::HashMap<blink::String, blink::String>& values(
      const blink::HashMap<blink::String, blink::String>& metadata) {
    return metadata;
  }

  static bool Read(blink::mojom::RTCMetadataDataView data,
                   blink::HashMap<blink::String, blink::String>* out) {
    if (!data.ReadValues(out)) {
      return false;
    }

    return blink::RTCMetadataValidator::Validate(*out) ==
           blink::RTCMetadataValidationError::kNone;
  }
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WEBRTC_RTC_LOGGING_MOJOM_TRAITS_H_

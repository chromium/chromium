// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_WEBRTC_RTC_LOGGING_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_WEBRTC_RTC_LOGGING_MOJOM_TRAITS_H_

#include <string>

#include "base/containers/flat_map.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/webrtc/rtc_logging.mojom-shared.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT StructTraits<
    blink::mojom::RTCMetadataDataView,
    base::flat_map<std::string, std::string>> {
  static const base::flat_map<std::string, std::string>& values(
      const base::flat_map<std::string, std::string>& metadata) {
    return metadata;
  }

  static bool Read(blink::mojom::RTCMetadataDataView data,
                   base::flat_map<std::string, std::string>* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_WEBRTC_RTC_LOGGING_MOJOM_TRAITS_H_

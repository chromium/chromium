// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/webrtc/rtc_logging_mojom_traits.h"

#include "third_party/blink/public/common/webrtc/rtc_logging_utils.h"

namespace mojo {

// static
bool StructTraits<blink::mojom::RTCMetadataDataView,
                  base::flat_map<std::string, std::string>>::
    Read(blink::mojom::RTCMetadataDataView data,
         base::flat_map<std::string, std::string>* out) {
  if (!data.ReadValues(out)) {
    return false;
  }

  return blink::RTCMetadataValidator::Validate(*out) ==
         blink::RTCMetadataValidationError::kNone;
}

}  // namespace mojo

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_WEBRTC_RTC_LOGGING_UTILS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_WEBRTC_RTC_LOGGING_UTILS_H_

#include <stddef.h>

#include "third_party/blink/public/common/common_export.h"

namespace blink {

enum class RTCMetadataValidationError {
  kNone,
  kTooManyEntries,
  kEntryTooLong,
};

class BLINK_COMMON_EXPORT RTCMetadataValidator {
 public:
  static constexpr size_t kMaxMetadataSize = 5;
  static constexpr size_t kMaxMetadataLength = 100;

  template <typename MapType>
  static RTCMetadataValidationError Validate(const MapType& metadata) {
    if (metadata.size() > kMaxMetadataSize) {
      return RTCMetadataValidationError::kTooManyEntries;
    }

    auto get_len = [](const auto& s) {
      if constexpr (requires { s.Utf8(); }) {
        return s.Utf8().length();
      } else {
        return s.length();
      }
    };

    for (const auto& entry : metadata) {
      if constexpr (requires { entry.first; }) {
        if (get_len(entry.first) > kMaxMetadataLength ||
            get_len(entry.second) > kMaxMetadataLength) {
          return RTCMetadataValidationError::kEntryTooLong;
        }
      } else {
        if (get_len(entry.key) > kMaxMetadataLength ||
            get_len(entry.value) > kMaxMetadataLength) {
          return RTCMetadataValidationError::kEntryTooLong;
        }
      }
    }
    return RTCMetadataValidationError::kNone;
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_WEBRTC_RTC_LOGGING_UTILS_H_

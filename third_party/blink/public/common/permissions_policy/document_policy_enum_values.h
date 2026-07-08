// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_DOCUMENT_POLICY_ENUM_VALUES_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_DOCUMENT_POLICY_ENUM_VALUES_H_

#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "base/notreached.h"
#include "third_party/blink/public/common/permissions_policy/js_profiling_mode.h"
#include "third_party/blink/public/mojom/permissions_policy/document_policy_feature.mojom-shared.h"

namespace blink {

// Returns the int32 enum value for a given token string for an enum-typed
// Document Policy feature, or std::nullopt if the token is not recognized.
// Accepts both mojom::DocumentPolicyFeature and
// mojom::blink::DocumentPolicyFeature since the latter is a type alias for the
// former.
//
// When adding a new enum Document Policy feature, add a case here.
inline std::optional<int32_t> DocumentPolicyEnumTokenToValue(
    mojom::DocumentPolicyFeature feature,
    std::string_view token) {
  switch (feature) {
    case mojom::DocumentPolicyFeature::kJSProfilingMode:
      if (token == "eager") {
        return std::to_underlying(JSProfilingMode::kEager);
      }
      if (token == "lazy") {
        return std::to_underlying(JSProfilingMode::kLazy);
      }
      return std::nullopt;
    default:
      NOTREACHED() << "No token-to-value mapping for enum Document Policy "
                      "feature "
                   << static_cast<int>(feature);
  }
}

// Returns the token string for a given int32 enum value for an enum-typed
// Document Policy feature, or std::nullopt if the value has no token
// representation (e.g. a default sentinel that is not expressible in a header).
//
// When adding a new enum Document Policy feature, add a case here.
inline std::optional<std::string_view> DocumentPolicyEnumValueToToken(
    mojom::DocumentPolicyFeature feature,
    int32_t value) {
  switch (feature) {
    case mojom::DocumentPolicyFeature::kJSProfilingMode:
      switch (value) {
        case std::to_underlying(JSProfilingMode::kEager):
          return "eager";
        case std::to_underlying(JSProfilingMode::kLazy):
          return "lazy";
        default:
          return std::nullopt;
      }
    default:
      NOTREACHED() << "No value-to-token mapping for enum Document Policy "
                      "feature "
                   << static_cast<int>(feature);
  }
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_PERMISSIONS_POLICY_DOCUMENT_POLICY_ENUM_VALUES_H_

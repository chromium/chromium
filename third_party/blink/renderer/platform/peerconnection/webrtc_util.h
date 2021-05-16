// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_UTIL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"

namespace blink {

// TODO(crbug.com/787254): Move these template definitions out of the Blink
// exposed API when all their clients get Onion souped.
template <typename OptionalT>
absl::optional<typename OptionalT::value_type> ToBaseOptional(
    const OptionalT& optional) {
  return optional ? absl::make_optional(*optional) : absl::nullopt;
}

template <typename OptionalT>
absl::optional<typename OptionalT::value_type> ToBaseOptional(
    OptionalT&& optional) {
  return optional ? absl::make_optional(*optional) : absl::nullopt;
}

template <typename OptionalT>
absl::optional<typename OptionalT::value_type> ToAbslOptional(
    const OptionalT& optional) {
  return optional ? absl::make_optional(*optional) : absl::nullopt;
}

template <typename OptionalT>
absl::optional<typename OptionalT::value_type> ToAbslOptional(
    OptionalT&& optional) {
  return optional ? absl::make_optional(*optional) : absl::nullopt;
}

template <typename OptionalT1, typename OptionalT2>
bool OptionalEquals(const OptionalT1& lhs, const OptionalT2& rhs) {
  if (!lhs)
    return !rhs;
  if (!rhs)
    return false;
  return *lhs == *rhs;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_UTIL_H_

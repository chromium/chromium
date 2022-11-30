// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_UTIL_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"

namespace blink {

// TODO(crbug.com/787254): Move these template definitions out of the Blink
// exposed API when all their clients get Onion souped.
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

String PLATFORM_EXPORT WebrtcCodecNameFromMimeType(const String& mime_type,
                                                   const char* prefix);

webrtc::SdpVideoFormat::Parameters PLATFORM_EXPORT
ConvertToSdpVideoFormatParameters(
    const ParsedContentHeaderFieldParameters& parameters);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_UTIL_H_

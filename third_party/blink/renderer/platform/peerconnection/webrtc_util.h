// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_UTIL_H_

#include <optional>

#include "base/time/time.h"
#include "media/base/video_codecs.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/units/time_delta.h"
#include "third_party/webrtc/api/units/timestamp.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"

namespace blink {

String PLATFORM_EXPORT WebrtcCodecNameFromMimeType(const String& mime_type,
                                                   const char* prefix);
std::map<std::string, std::string> PLATFORM_EXPORT
ConvertToSdpVideoFormatParameters(
    const ParsedContentHeaderFieldParameters& parameters);

base::TimeTicks PLATFORM_EXPORT ConvertToBaseTimeTicks(webrtc::Timestamp time);

base::TimeDelta PLATFORM_EXPORT
ConvertToBaseTimeDelta(webrtc::TimeDelta time_delta);

std::optional<media::VideoCodecProfile> PLATFORM_EXPORT
WebRTCFormatToCodecProfile(const webrtc::SdpVideoFormat& sdp);

// Converts an optional webrtc::Timestamp into an optional TimeTicks and
// optionally adds an offset to the result.
std::optional<base::TimeTicks> PLATFORM_EXPORT ConvertToOptionalTimeTicks(
    std::optional<webrtc::Timestamp> time,
    std::optional<base::TimeTicks> offset = std::nullopt);

// Converts an optional webrtc::TimesDelta into an base::TimeDelta.
std::optional<base::TimeDelta> PLATFORM_EXPORT
ConvertToOptionalTimeDelta(std::optional<webrtc::TimeDelta> time_delta);

// Checks if H.264 CBP is available for accelerated encoding on the current
// platform.
bool PLATFORM_EXPORT
IsH264ConstrainedBaselineProfileAvailableForAcceleratedEncoder();

// Checks whether the accelerated H.264 encoder can be used in WebRTC.
bool PLATFORM_EXPORT UseH264AcceleratedEncoderForWebRTC();

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_UTIL_H_

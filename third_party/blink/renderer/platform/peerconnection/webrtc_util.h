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
#include "third_party/webrtc/api/units/timestamp.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"

namespace blink {

String PLATFORM_EXPORT WebrtcCodecNameFromMimeType(const String& mime_type,
                                                   const char* prefix);
std::map<std::string, std::string> PLATFORM_EXPORT
ConvertToSdpVideoFormatParameters(
    const ParsedContentHeaderFieldParameters& parameters);

base::TimeTicks PLATFORM_EXPORT ConvertToBaseTimeTicks(webrtc::Timestamp time);

std::optional<media::VideoCodecProfile> PLATFORM_EXPORT
WebRTCFormatToCodecProfile(const webrtc::SdpVideoFormat& sdp);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_UTIL_H_

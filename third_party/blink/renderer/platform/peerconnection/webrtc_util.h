// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_UTIL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_UTIL_H_

#include "base/time/time.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/units/timestamp.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"

namespace blink {

String PLATFORM_EXPORT WebrtcCodecNameFromMimeType(const String& mime_type,
                                                   const char* prefix);

webrtc::SdpVideoFormat::Parameters PLATFORM_EXPORT
ConvertToSdpVideoFormatParameters(
    const ParsedContentHeaderFieldParameters& parameters);

base::TimeTicks PLATFORM_EXPORT ConvertToBaseTimeTicks(webrtc::Timestamp time);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_PEERCONNECTION_WEBRTC_UTIL_H_

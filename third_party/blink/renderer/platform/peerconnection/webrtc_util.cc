// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"

#include <cstring>

#include "base/feature_list.h"
#include "build/build_config.h"
#include "media/base/video_codecs.h"
#include "media/media_buildflags.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"

namespace blink {

String WebrtcCodecNameFromMimeType(const String& mime_type,
                                   const char* prefix) {
  if (mime_type.StartsWith(prefix)) {
    wtf_size_t length =
        static_cast<wtf_size_t>(mime_type.length() - strlen(prefix) - 1);
    const String codec_name = mime_type.Right(length);
    return codec_name;
  }
  return "";
}

std::map<std::string, std::string> ConvertToSdpVideoFormatParameters(
    const ParsedContentHeaderFieldParameters& parameters) {
  std::map<std::string, std::string> sdp_parameters;
  for (const auto& parameter : parameters) {
    sdp_parameters[parameter.name.Utf8()] = parameter.value.Utf8();
  }
  return sdp_parameters;
}

base::TimeTicks PLATFORM_EXPORT ConvertToBaseTimeTicks(webrtc::Timestamp time) {
  if (time == webrtc::Timestamp::PlusInfinity()) {
    return base::TimeTicks::Max();
  } else if (time == webrtc::Timestamp::MinusInfinity()) {
    return base::TimeTicks::Min();
  } else {
    return base::TimeTicks() + base::Microseconds(time.us());
  }
}

std::optional<media::VideoCodecProfile> WebRTCFormatToCodecProfile(
    const webrtc::SdpVideoFormat& sdp) {
  if (sdp.name == "H264") {
#if !BUILDFLAG(IS_ANDROID)
    // Enable H264 HW encode for WebRTC when SW fallback is available, which is
    // checked by kWebRtcH264WithOpenH264FFmpeg flag. This check should be
    // removed when SW implementation is fully enabled.
    bool webrtc_h264_sw_enabled = false;
// TODO(crbug.com/355256378): OpenH264 for encoding and FFmpeg for H264 decoding
// should be detangled such that software decoding can be enabled without
// software encoding.
#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS) && \
    BUILDFLAG(ENABLE_OPENH264)
    webrtc_h264_sw_enabled = base::FeatureList::IsEnabled(
        blink::features::kWebRtcH264WithOpenH264FFmpeg);
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS) &&
        // BUILDFLAG(ENABLE_OPENH264)
    if (!webrtc_h264_sw_enabled) {
      return std::nullopt;
    }
#endif

    return media::VideoCodecProfile::H264PROFILE_MIN;
  } else if (sdp.name == "VP8") {
    return media::VideoCodecProfile::VP8PROFILE_MIN;
  } else if (sdp.name == "VP9") {
    return media::VideoCodecProfile::VP9PROFILE_MIN;
  } else if (sdp.name == "AV1") {
    return media::VideoCodecProfile::AV1PROFILE_MIN;
  }
#if BUILDFLAG(RTC_USE_H265)
  else if (sdp.name == "H265") {
    return media::VideoCodecProfile::HEVCPROFILE_MIN;
  }
#endif  // BUILDFLAG(RTC_USE_H265)
  return std::nullopt;
}
}  // namespace blink

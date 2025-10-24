// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"

#include <cstring>
#include <optional>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "media/base/video_codecs.h"
#include "media/media_buildflags.h"
#include "media/webrtc/webrtc_features.h"
#include "third_party/blink/public/common/buildflags.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/webrtc/api/units/time_delta.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"

namespace blink {

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
// Enables H.264 CBP encode acceleration.
BASE_FEATURE(kPlatformH264CbpEncoding,
#if BUILDFLAG(IS_WIN)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

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

base::TimeDelta PLATFORM_EXPORT
ConvertToBaseTimeDelta(webrtc::TimeDelta time_delta) {
  if (time_delta == webrtc::TimeDelta::PlusInfinity()) {
    return base::TimeDelta::Max();
  } else if (time_delta == webrtc::TimeDelta::MinusInfinity()) {
    return base::TimeDelta::Min();
  } else {
    return base::Microseconds(time_delta.us());
  }
}

std::optional<media::VideoCodecProfile> WebRTCFormatToCodecProfile(
    const webrtc::SdpVideoFormat& sdp) {
  if (sdp.name == "H264") {
    if (UseH264AcceleratedEncoderForWebRTC()) {
      return media::VideoCodecProfile::H264PROFILE_MIN;
    } else {
      return std::nullopt;
    }
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

std::optional<base::TimeTicks> PLATFORM_EXPORT
ConvertToOptionalTimeTicks(std::optional<webrtc::Timestamp> time,
                           std::optional<base::TimeTicks> offset) {
  // Converting minimal timestamps to DOMHighResTimeStamps can result in UB.
  // Return nullopt in that case. See https://crbug.com/399818722
  if (!time || time->IsMinusInfinity()) {
    return std::nullopt;
  }
  base::TimeTicks time_ticks = ConvertToBaseTimeTicks(*time);
  if (offset) {
    return *offset + (time_ticks - base::TimeTicks());
  }
  return time_ticks;
}

std::optional<base::TimeDelta> PLATFORM_EXPORT
ConvertToOptionalTimeDelta(std::optional<webrtc::TimeDelta> time_delta) {
  // Converting minimal deltas to DOMHighResTimeStamps can result in UB.
  // Return nullopt in that case. See https://crbug.com/399818722
  if (!time_delta || time_delta->IsMinusInfinity()) {
    return std::nullopt;
  }
  return ConvertToBaseTimeDelta(*time_delta);
}

bool PLATFORM_EXPORT
IsH264ConstrainedBaselineProfileAvailableForAcceleratedEncoder() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || \
    BUILDFLAG(IS_ANDROID)
  return base::FeatureList::IsEnabled(kPlatformH264CbpEncoding);
#else
  return false;
#endif
}

bool PLATFORM_EXPORT UseH264AcceleratedEncoderForWebRTC() {
#if !BUILDFLAG(IS_ANDROID)
  // On non-Android, H264 HW encoder cannot be used unless SW encoder is also
  // available because of assumptions that SW fallback is always possible. This
  // check should be removed when SW implementation is always available.
  return ::features::IsOpenH264SoftwareEncoderEnabledForWebRTC();
#elif BUILDFLAG(RTC_USE_H264)
  // On Android, H264 HW encoder can be used without SW encoder because the SW
  // fallback logic has been explicitly disabled.
  return true;
#else
  return false;
#endif
}

}  // namespace blink

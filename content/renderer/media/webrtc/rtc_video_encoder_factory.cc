// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/rtc_video_encoder_factory.h"

#include <memory>

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/feature_h264_with_openh264_ffmpeg.h"
#include "content/renderer/media/webrtc/rtc_video_encoder.h"
#include "media/media_buildflags.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/webrtc/common_video/h264/profile_level_id.h"

namespace content {

namespace {

// Translate from media::VideoEncodeAccelerator::SupportedProfile to
// cricket::WebRtcVideoEncoderFactory::VideoCodec, or return nothing if the
// profile isn't supported.
base::Optional<cricket::VideoCodec> VEAToWebRTCCodec(
    const media::VideoEncodeAccelerator::SupportedProfile& profile) {
  DCHECK_EQ(profile.max_framerate_denominator, 1U);

  if (profile.profile >= media::VP8PROFILE_MIN &&
      profile.profile <= media::VP8PROFILE_MAX) {
    if (base::FeatureList::IsEnabled(features::kWebRtcHWVP8Encoding)) {
      return base::Optional<cricket::VideoCodec>(cricket::VideoCodec("VP8"));
    }
  } else if (profile.profile >= media::H264PROFILE_MIN &&
             profile.profile <= media::H264PROFILE_MAX) {
    // Enable H264 HW encode for WebRTC when SW fallback is available, which is
    // checked by kWebRtcH264WithOpenH264FFmpeg flag. This check should be
    // removed when SW implementation is fully enabled.
    bool webrtc_h264_sw_enabled = false;
#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
    webrtc_h264_sw_enabled =
        base::FeatureList::IsEnabled(kWebRtcH264WithOpenH264FFmpeg);
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
    if (webrtc_h264_sw_enabled ||
        base::FeatureList::IsEnabled(features::kWebRtcHWH264Encoding)) {
      webrtc::H264::Profile h264_profile;
      switch (profile.profile) {
        case media::H264PROFILE_BASELINE:
#if defined(OS_ANDROID)
          // Force HW H264 on Android to be CBP for most compatibility, since:
          // - Only HW H264 is available on Android at present.
          // - MediaCodec only advise BP, which works same as CBP in most cases.
          // - Some peers only expect CBP in negotiation.
          h264_profile = webrtc::H264::kProfileConstrainedBaseline;
#else
          h264_profile = webrtc::H264::kProfileBaseline;
#endif
          break;
        case media::H264PROFILE_MAIN:
          h264_profile = webrtc::H264::kProfileMain;
          break;
        case media::H264PROFILE_HIGH:
          h264_profile = webrtc::H264::kProfileHigh;
          break;
        default:
          // Unsupported H264 profile in WebRTC.
          return base::Optional<cricket::VideoCodec>();
      }

      const int width = profile.max_resolution.width();
      const int height = profile.max_resolution.height();
      const int fps = profile.max_framerate_numerator;
      DCHECK_EQ(1u, profile.max_framerate_denominator);

      const absl::optional<webrtc::H264::Level> h264_level =
          webrtc::H264::SupportedLevel(width * height, fps);
      const webrtc::H264::ProfileLevelId profile_level_id(
          h264_profile, h264_level.value_or(webrtc::H264::kLevel1));

      cricket::VideoCodec codec("H264");
      codec.SetParam(cricket::kH264FmtpProfileLevelId,
                     *webrtc::H264::ProfileLevelIdToString(profile_level_id));
      codec.SetParam(cricket::kH264FmtpLevelAsymmetryAllowed, "1");
      codec.SetParam(cricket::kH264FmtpPacketizationMode, "1");
      return base::Optional<cricket::VideoCodec>(codec);
    }
  }
  return base::Optional<cricket::VideoCodec>();
}

}  // anonymous namespace

RTCVideoEncoderFactory::RTCVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : gpu_factories_(gpu_factories) {
  const media::VideoEncodeAccelerator::SupportedProfiles& profiles =
      gpu_factories_->GetVideoEncodeAcceleratorSupportedProfiles();
  for (const auto& profile : profiles) {
    base::Optional<cricket::VideoCodec> codec = VEAToWebRTCCodec(profile);
    if (codec) {
      supported_codecs_.push_back(std::move(*codec));
      profiles_.push_back(profile.profile);
    }
  }
  // There should be a 1:1 mapping between media::VideoCodecProfile and
  // cricket::VideoCodec.
  CHECK_EQ(profiles_.size(), supported_codecs_.size());
}

RTCVideoEncoderFactory::~RTCVideoEncoderFactory() {}

webrtc::VideoEncoder* RTCVideoEncoderFactory::CreateVideoEncoder(
    const cricket::VideoCodec& codec) {
  for (size_t i = 0; i < supported_codecs_.size(); ++i) {
    if (!base::EqualsCaseInsensitiveASCII(codec.name,
                                          supported_codecs_[i].name)) {
      continue;
    }
    // Check H264 profile.
    using webrtc::H264::ParseSdpProfileLevelId;
    if (base::EqualsCaseInsensitiveASCII(codec.name, cricket::kH264CodecName) &&
        ParseSdpProfileLevelId(codec.params)->profile !=
            ParseSdpProfileLevelId(supported_codecs_[i].params)->profile) {
      continue;
    }
    // There should be a 1:1 mapping between media::VideoCodecProfile and
    // cricket::VideoCodec.
    CHECK_EQ(profiles_.size(), supported_codecs_.size());
    return new RTCVideoEncoder(profiles_[i], gpu_factories_);
  }
  return nullptr;
}

const std::vector<cricket::VideoCodec>&
RTCVideoEncoderFactory::supported_codecs() const {
  return supported_codecs_;
}

void RTCVideoEncoderFactory::DestroyVideoEncoder(
    webrtc::VideoEncoder* encoder) {
  delete encoder;
}

}  // namespace content

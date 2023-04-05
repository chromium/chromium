// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder_factory.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/media_buildflags.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder.h"
#include "third_party/webrtc/api/video_codecs/h264_profile_level_id.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_encoder.h"
#include "third_party/webrtc/api/video_codecs/vp9_profile.h"
#include "third_party/webrtc/media/base/codec.h"

namespace blink {

namespace {

#if BUILDFLAG(IS_WIN)
// Enables AV1 encode acceleration for Windows.
BASE_FEATURE(kMediaFoundationAV1Encoding,
             "MediaFoundationAV1Encoding",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables H.264 CBP encode acceleration for Windows.
BASE_FEATURE(kMediaFoundationH264CbpEncoding,
             "MediaFoundationH264CbpEncoding",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables VP9 encode acceleration for Windows.
BASE_FEATURE(kMediaFoundationVP9Encoding,
             "MediaFoundationVP9Encoding",
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif

absl::optional<media::VideoCodecProfile> WebRTCFormatToCodecProfile(
    const webrtc::SdpVideoFormat& sdp) {
  if (sdp.name == "H264") {
#if !BUILDFLAG(IS_ANDROID)
    // Enable H264 HW encode for WebRTC when SW fallback is available, which is
    // checked by kWebRtcH264WithOpenH264FFmpeg flag. This check should be
    // removed when SW implementation is fully enabled.
    bool webrtc_h264_sw_enabled = false;
#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
    webrtc_h264_sw_enabled = base::FeatureList::IsEnabled(
        blink::features::kWebRtcH264WithOpenH264FFmpeg);
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
    if (!webrtc_h264_sw_enabled)
      return absl::nullopt;
#endif

    return media::VideoCodecProfile::H264PROFILE_MIN;
  } else if (sdp.name == "VP8") {
    return media::VideoCodecProfile::VP8PROFILE_MIN;
  } else if (sdp.name == "VP9") {
    return media::VideoCodecProfile::VP9PROFILE_MIN;
  } else if (sdp.name == "AV1") {
    return media::VideoCodecProfile::AV1PROFILE_MIN;
  }
  return absl::nullopt;
}

// Translate from media::VideoEncodeAccelerator::SupportedProfile to
// webrtc::SdpVideoFormat, or return nothing if the profile isn't supported.
absl::optional<webrtc::SdpVideoFormat> VEAToWebRTCFormat(
    const media::VideoEncodeAccelerator::SupportedProfile& profile) {
  DCHECK_EQ(profile.max_framerate_denominator, 1U);

  if (profile.profile >= media::VP8PROFILE_MIN &&
      profile.profile <= media::VP8PROFILE_MAX) {
    return webrtc::SdpVideoFormat("VP8");
  }
  if (profile.profile >= media::H264PROFILE_MIN &&
      profile.profile <= media::H264PROFILE_MAX) {
#if !BUILDFLAG(IS_ANDROID)
    // Enable H264 HW encode for WebRTC when SW fallback is available, which is
    // checked by kWebRtcH264WithOpenH264FFmpeg flag. This check should be
    // removed when SW implementation is fully enabled.
    bool webrtc_h264_sw_enabled = false;
#if BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
    webrtc_h264_sw_enabled = base::FeatureList::IsEnabled(
        blink::features::kWebRtcH264WithOpenH264FFmpeg);
#endif  // BUILDFLAG(RTC_USE_H264) && BUILDFLAG(ENABLE_FFMPEG_VIDEO_DECODERS)
    if (!webrtc_h264_sw_enabled)
      return absl::nullopt;
#endif

    webrtc::H264Profile h264_profile;
    switch (profile.profile) {
      case media::H264PROFILE_BASELINE:
#if BUILDFLAG(IS_ANDROID)
        // Force HW H264 on Android to be CBP for most compatibility, since:
        // - Only HW H264 is available on Android at present.
        // - MediaCodec only advise BP, which works same as CBP in most cases.
        // - Some peers only expect CBP in negotiation.
        h264_profile = webrtc::H264Profile::kProfileConstrainedBaseline;
#else
        h264_profile = webrtc::H264Profile::kProfileBaseline;
#endif  // BUILDFLAG(IS_ANDROID)
        break;
      case media::H264PROFILE_MAIN:
        h264_profile = webrtc::H264Profile::kProfileMain;
        break;
      case media::H264PROFILE_HIGH:
        h264_profile = webrtc::H264Profile::kProfileHigh;
        break;
      default:
        // Unsupported H264 profile in WebRTC.
        return absl::nullopt;
    }

    const int width = profile.max_resolution.width();
    const int height = profile.max_resolution.height();
    const int fps = profile.max_framerate_numerator;
    DCHECK_EQ(1u, profile.max_framerate_denominator);

    const absl::optional<webrtc::H264Level> h264_level =
        webrtc::H264SupportedLevel(width * height, fps);
    const webrtc::H264ProfileLevelId profile_level_id(
        h264_profile, h264_level.value_or(webrtc::H264Level::kLevel1));

    webrtc::SdpVideoFormat format("H264");
    format.parameters = {
        {cricket::kH264FmtpProfileLevelId,
         *webrtc::H264ProfileLevelIdToString(profile_level_id)},
        {cricket::kH264FmtpLevelAsymmetryAllowed, "1"},
        {cricket::kH264FmtpPacketizationMode, "1"}};
    return format;
  }

  if (profile.profile >= media::VP9PROFILE_MIN &&
      profile.profile <= media::VP9PROFILE_MAX) {
    webrtc::VP9Profile vp9_profile;
    switch (profile.profile) {
      case media::VP9PROFILE_PROFILE0:
        vp9_profile = webrtc::VP9Profile::kProfile0;
        break;
      case media::VP9PROFILE_PROFILE2:
        vp9_profile = webrtc::VP9Profile::kProfile2;
        break;
      default:
        // Unsupported VP9 profiles (profile1 & profile3) in WebRTC.
        return absl::nullopt;
    }
    webrtc::SdpVideoFormat format("VP9");
    format.parameters = {
        {webrtc::kVP9FmtpProfileId,
         webrtc::VP9ProfileToString(vp9_profile)}};
    return format;
  }

  if (profile.profile >= media::AV1PROFILE_MIN &&
      profile.profile <= media::AV1PROFILE_MAX) {
    return webrtc::SdpVideoFormat("AV1");
  }

  return absl::nullopt;
}  // namespace

struct SupportedFormats {
  bool unknown = true;
  std::vector<media::VideoCodecProfile> profiles
      ALLOW_DISCOURAGED_TYPE("Matches webrtc API");
  std::vector<std::vector<media::SVCScalabilityMode>> scalability_modes
      ALLOW_DISCOURAGED_TYPE("Matches webrtc API");
  std::vector<webrtc::SdpVideoFormat> sdp_formats
      ALLOW_DISCOURAGED_TYPE("Matches webrtc API");
};

SupportedFormats GetSupportedFormatsInternal(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    const std::vector<media::VideoCodecProfile>& disabled_profiles) {
  SupportedFormats supported_formats;
  auto profiles = gpu_factories->GetVideoEncodeAcceleratorSupportedProfiles();
  if (!profiles)
    return supported_formats;

  // |profiles| are either the info at GpuInfo instance or the info got by
  // querying GPU process.
  supported_formats.unknown = false;
  for (const auto& profile : *profiles) {
    if (base::Contains(disabled_profiles, profile.profile))
      continue;

    absl::optional<webrtc::SdpVideoFormat> format = VEAToWebRTCFormat(profile);
    if (format) {
      supported_formats.profiles.push_back(profile.profile);
      supported_formats.scalability_modes.push_back(profile.scalability_modes);
      supported_formats.sdp_formats.push_back(std::move(*format));

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX)
#if BUILDFLAG(IS_WIN)
      const bool kShouldAddH264Cbp =
          base::FeatureList::IsEnabled(kMediaFoundationH264CbpEncoding) &&
          profile.profile == media::VideoCodecProfile::H264PROFILE_BASELINE;
#elif BUILDFLAG(IS_LINUX)
      const bool kShouldAddH264Cbp =
          profile.profile == media::VideoCodecProfile::H264PROFILE_BASELINE;
#endif
      if (kShouldAddH264Cbp) {
        supported_formats.profiles.push_back(profile.profile);
        supported_formats.scalability_modes.push_back(
            profile.scalability_modes);
        cricket::AddH264ConstrainedBaselineProfileToSupportedFormats(
            &supported_formats.sdp_formats);
      }
#endif
    }
  }

  return supported_formats;
}

bool IsConstrainedH264(const webrtc::SdpVideoFormat& format) {
  bool is_constrained_h264 = false;

  if (format.name == cricket::kH264CodecName) {
    const absl::optional<webrtc::H264ProfileLevelId> profile_level_id =
        webrtc::ParseSdpForH264ProfileLevelId(format.parameters);
    if (profile_level_id &&
        profile_level_id->profile ==
            webrtc::H264Profile::kProfileConstrainedBaseline) {
      is_constrained_h264 = true;
    }
  }

  return is_constrained_h264;
}

bool IsScalabiltiyModeSupported(
    const std::string& scalability_mode,
    const std::vector<media::SVCScalabilityMode>& supported_scalability_modes) {
  for (const auto& supported_scalability_mode : supported_scalability_modes) {
    if (scalability_mode ==
        media::GetScalabilityModeName(supported_scalability_mode)) {
      return true;
    }
  }
  return false;
}

}  // anonymous namespace

RTCVideoEncoderFactory::RTCVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories)
    : gpu_factories_(gpu_factories), gpu_codec_support_waiter_(gpu_factories) {
#if BUILDFLAG(IS_WIN)
  if (!base::FeatureList::IsEnabled(kMediaFoundationVP9Encoding)) {
    disabled_profiles_.emplace_back(media::VP9PROFILE_PROFILE0);
    disabled_profiles_.emplace_back(media::VP9PROFILE_PROFILE1);
    disabled_profiles_.emplace_back(media::VP9PROFILE_PROFILE2);
    disabled_profiles_.emplace_back(media::VP9PROFILE_PROFILE3);
  }
  if (!base::FeatureList::IsEnabled(kMediaFoundationAV1Encoding)) {
    disabled_profiles_.emplace_back(media::AV1PROFILE_PROFILE_MAIN);
    disabled_profiles_.emplace_back(media::AV1PROFILE_PROFILE_HIGH);
    disabled_profiles_.emplace_back(media::AV1PROFILE_PROFILE_PRO);
  }
#endif
}

RTCVideoEncoderFactory::~RTCVideoEncoderFactory() {}

void RTCVideoEncoderFactory::CheckAndWaitEncoderSupportStatusIfNeeded() const {
  if (!gpu_codec_support_waiter_.IsEncoderSupportKnown()) {
    DLOG(WARNING) << "Encoder support is unknown. Timeout "
                  << gpu_codec_support_waiter_.wait_timeout_ms()
                         .value_or(base::TimeDelta())
                         .InMilliseconds()
                  << "ms. Encoders might not be available.";
  }
}

std::unique_ptr<webrtc::VideoEncoder>
RTCVideoEncoderFactory::CreateVideoEncoder(
    const webrtc::SdpVideoFormat& format) {
  CheckAndWaitEncoderSupportStatusIfNeeded();

  std::unique_ptr<webrtc::VideoEncoder> encoder;
  bool is_constrained_h264 = IsConstrainedH264(format);
  auto supported_formats =
      GetSupportedFormatsInternal(gpu_factories_, disabled_profiles_);
  if (!supported_formats.unknown) {
    for (size_t i = 0; i < supported_formats.sdp_formats.size(); ++i) {
      if (format.IsSameCodec(supported_formats.sdp_formats[i])) {
        encoder = std::make_unique<RTCVideoEncoder>(
            supported_formats.profiles[i], is_constrained_h264, gpu_factories_);
        break;
      }
    }
  } else {
    auto profile = WebRTCFormatToCodecProfile(format);
    if (profile) {
      encoder = std::make_unique<RTCVideoEncoder>(*profile, is_constrained_h264,
                                                  gpu_factories_);
    }
  }

  return encoder;
}

std::vector<webrtc::SdpVideoFormat>
RTCVideoEncoderFactory::GetSupportedFormats() const {
  CheckAndWaitEncoderSupportStatusIfNeeded();

  return GetSupportedFormatsInternal(gpu_factories_, disabled_profiles_)
      .sdp_formats;
}

webrtc::VideoEncoderFactory::CodecSupport
RTCVideoEncoderFactory::QueryCodecSupport(
    const webrtc::SdpVideoFormat& format,
    absl::optional<std::string> scalability_mode) const {
  CheckAndWaitEncoderSupportStatusIfNeeded();
  SupportedFormats supported_formats =
      GetSupportedFormatsInternal(gpu_factories_, disabled_profiles_);

  for (size_t i = 0; i < supported_formats.sdp_formats.size(); ++i) {
    if (format.IsSameCodec(supported_formats.sdp_formats[i])) {
      if (!scalability_mode ||
          IsScalabiltiyModeSupported(*scalability_mode,
                                     supported_formats.scalability_modes[i])) {
        return {/*is_supported=*/true, /*is_power_efficient=*/true};
      }
      break;
    }
  }
  return {/*is_supported=*/false, /*is_power_efficient=*/false};
}

}  // namespace blink

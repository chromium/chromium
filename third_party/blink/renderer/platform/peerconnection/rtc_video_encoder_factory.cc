// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder_factory.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/media_buildflags.h"
#include "media/mojo/clients/mojo_video_encoder_metrics_provider.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "media/webrtc/webrtc_features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/allow_discouraged_type.h"
#include "third_party/blink/renderer/platform/peerconnection/rtc_video_encoder.h"
#include "third_party/blink/renderer/platform/peerconnection/webrtc_util.h"
#include "third_party/webrtc/api/video/resolution.h"
#include "third_party/webrtc/api/video_codecs/h264_profile_level_id.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/api/video_codecs/video_encoder.h"
#include "third_party/webrtc/api/video_codecs/vp9_profile.h"
#include "third_party/webrtc/media/base/codec.h"

#if BUILDFLAG(RTC_USE_H265)
#include "third_party/webrtc/api/video_codecs/h265_profile_tier_level.h"
#endif  // BUILDFLAG(RTC_USE_H265)

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

// Translate from media::VideoEncodeAccelerator::SupportedProfile to
// webrtc::SdpVideoFormat, or return nothing if the profile isn't supported.
std::optional<webrtc::SdpVideoFormat> VEAToWebRTCFormat(
    const media::VideoEncodeAccelerator::SupportedProfile& profile) {
  const int width = profile.max_resolution.width();
  const int height = profile.max_resolution.height();
  const int fps = profile.max_framerate_numerator;
  DCHECK_EQ(1u, profile.max_framerate_denominator);

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
        return std::nullopt;
    }

    const std::optional<webrtc::H264Level> h264_level =
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
        return std::nullopt;
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

  if (profile.profile >= media::HEVCPROFILE_MIN &&
      profile.profile <= media::HEVCPROFILE_MAX) {
#if BUILDFLAG(RTC_USE_H265)
    // Unlikely H.264, there is no SW encoder implementation for H.265, so we
    // will not check SW support here.
    webrtc::H265Profile h265_profile;
    switch (profile.profile) {
      case media::HEVCPROFILE_MAIN:
        h265_profile = webrtc::H265Profile::kProfileMain;
        break;
      case media::HEVCPROFILE_MAIN10:
        h265_profile = webrtc::H265Profile::kProfileMain10;
        break;
      default:
        // Unsupported H.265 profiles(main still/range extensions etc) in
        // WebRTC.
        return std::nullopt;
    }
    const webrtc::Resolution resolution = {
        .width = width,
        .height = height,
    };
    const std::optional<webrtc::H265Level> h265_level =
        webrtc::GetSupportedH265Level(resolution, fps);
    const webrtc::H265ProfileTierLevel profile_tier_level(
        h265_profile, webrtc::H265Tier::kTier0,
        h265_level.value_or(webrtc::H265Level::kLevel1));
    webrtc::SdpVideoFormat format("H265");
    format.parameters = {
        {cricket::kH265FmtpProfileId,
         webrtc::H265ProfileToString(profile_tier_level.profile)},
        {cricket::kH265FmtpTierFlag,
         webrtc::H265TierToString(profile_tier_level.tier)},
        {cricket::kH265FmtpLevelId,
         webrtc::H265LevelToString(profile_tier_level.level)},
        {cricket::kH265FmtpTxMode, "SRST"}};
    return format;
#else
    return std::nullopt;
#endif  // BUILDFLAG(RTC_USE_H265)
  }

  return std::nullopt;
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
  SupportedFormats low_priority_formats;

  auto profiles = gpu_factories->GetVideoEncodeAcceleratorSupportedProfiles();
  if (!profiles)
    return supported_formats;

  // |profiles| are either the info at GpuInfo instance or the info got by
  // querying GPU process.
  supported_formats.unknown = false;
  for (const auto& profile : *profiles) {
    if (base::Contains(disabled_profiles, profile.profile))
      continue;

    std::optional<webrtc::SdpVideoFormat> format = VEAToWebRTCFormat(profile);
    if (format) {
      if (format->IsCodecInList(supported_formats.sdp_formats)) {
        continue;
      }
      // Supported H.265 formats must be added to the end of supported codecs.
#if BUILDFLAG(RTC_USE_H265)
      if (format->name == cricket::kH265CodecName) {
        const std::optional<webrtc::H265ProfileTierLevel> profile_tier_level =
            webrtc::ParseSdpForH265ProfileTierLevel(format->parameters);
        // https://datatracker.ietf.org/doc/draft-ietf-avtcore-hevc-webrtc/:
        // according to above spec, level 3.1 is mandatory to support. So
        // unlike H.264 which has level-asymmetry-allowed parameter in SDP to
        // signal support for asymmetric levels, we need to add level 3.1
        // explicitly if GPU factory reports supporting of level higher than
        // 3.1, to make sure that if remote only supports level 3.1, we still
        // allow the SDP negotiation to succeed.
        if (profile_tier_level &&
            profile_tier_level->level > webrtc::H265Level::kLevel3_1) {
          webrtc::SdpVideoFormat level_3_1_format(*format);
          format->parameters[cricket::kH265FmtpLevelId] =
              webrtc::H265LevelToString(webrtc::H265Level::kLevel3_1);
          low_priority_formats.profiles.push_back(profile.profile);
          low_priority_formats.scalability_modes.push_back(
              profile.scalability_modes);
          low_priority_formats.sdp_formats.push_back(level_3_1_format);
        }

        low_priority_formats.profiles.push_back(profile.profile);
        low_priority_formats.scalability_modes.push_back(
            profile.scalability_modes);
        low_priority_formats.sdp_formats.push_back(std::move(*format));
        continue;
      }
#endif  // BUILDFLAG(RTC_USE_H265)
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

  supported_formats.profiles.insert(supported_formats.profiles.end(),
                                    low_priority_formats.profiles.begin(),
                                    low_priority_formats.profiles.end());
  supported_formats.scalability_modes.insert(
      supported_formats.scalability_modes.end(),
      low_priority_formats.scalability_modes.begin(),
      low_priority_formats.scalability_modes.end());
  supported_formats.sdp_formats.insert(supported_formats.sdp_formats.end(),
                                       low_priority_formats.sdp_formats.begin(),
                                       low_priority_formats.sdp_formats.end());

  DCHECK_EQ(supported_formats.profiles.size(),
            supported_formats.sdp_formats.size());
  DCHECK_EQ(supported_formats.profiles.size(),
            supported_formats.scalability_modes.size());

  return supported_formats;
}

bool IsConstrainedH264(const webrtc::SdpVideoFormat& format) {
  bool is_constrained_h264 = false;

  if (format.name == cricket::kH264CodecName) {
    const std::optional<webrtc::H264ProfileLevelId> profile_level_id =
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
    media::GpuVideoAcceleratorFactories* gpu_factories,
    scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
        encoder_metrics_provider_factory)
    : gpu_factories_(gpu_factories),
      encoder_metrics_provider_factory_(
          std::move(encoder_metrics_provider_factory)),
      gpu_codec_support_waiter_(gpu_factories) {
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
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(RTC_USE_H265)
  // We may not need to add check for media::kPlatformHEVCEncoderSupport here
  // but it's added for consistency with other codecs like H264 and AV1.
  if (
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
      !base::FeatureList::IsEnabled(media::kPlatformHEVCEncoderSupport) ||
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_ANDROID)
      !base::FeatureList::IsEnabled(::features::kWebRtcAllowH265Send)) {
    disabled_profiles_.emplace_back(media::HEVCPROFILE_MAIN);
    disabled_profiles_.emplace_back(media::HEVCPROFILE_MAIN10);
    disabled_profiles_.emplace_back(media::HEVCPROFILE_MAIN_STILL_PICTURE);
    disabled_profiles_.emplace_back(media::HEVCPROFILE_REXT);
    disabled_profiles_.emplace_back(media::HEVCPROFILE_HIGH_THROUGHPUT);
    disabled_profiles_.emplace_back(media::HEVCPROFILE_MULTIVIEW_MAIN);
    disabled_profiles_.emplace_back(media::HEVCPROFILE_SCALABLE_MAIN);
    disabled_profiles_.emplace_back(media::HEVCPROFILE_3D_MAIN);
    disabled_profiles_.emplace_back(media::HEVCPROFILE_SCREEN_EXTENDED);
    disabled_profiles_.emplace_back(media::HEVCPROFILE_SCALABLE_REXT);
    disabled_profiles_.emplace_back(
        media::HEVCPROFILE_HIGH_THROUGHPUT_SCREEN_EXTENDED);
  }
#endif  // BUILDFLAG(RTC_USE_H265)
}

RTCVideoEncoderFactory::~RTCVideoEncoderFactory() {
  // |encoder_metrics_provider_factory_| needs to be destroyed on the same
  // sequence as one that destroys the VideoEncoderMetricsProviders created by
  // it. It is gpu task runner in this case.
  gpu_factories_->GetTaskRunner()->ReleaseSoon(
      FROM_HERE, std::move(encoder_metrics_provider_factory_));
}

void RTCVideoEncoderFactory::CheckAndWaitEncoderSupportStatusIfNeeded() const {
  if (!gpu_codec_support_waiter_.IsEncoderSupportKnown()) {
    DLOG(WARNING) << "Encoder support is unknown. Timeout "
                  << gpu_codec_support_waiter_.wait_timeout_ms()
                         .value_or(base::TimeDelta())
                         .InMilliseconds()
                  << "ms. Encoders might not be available.";
  }
}

std::unique_ptr<webrtc::VideoEncoder> RTCVideoEncoderFactory::Create(
    const webrtc::Environment& env,
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
            supported_formats.profiles[i], is_constrained_h264, gpu_factories_,
            encoder_metrics_provider_factory_);
        break;
      }
    }
  } else {
    auto profile = WebRTCFormatToCodecProfile(format);
    if (profile) {
      encoder = std::make_unique<RTCVideoEncoder>(
          *profile, is_constrained_h264, gpu_factories_,
          encoder_metrics_provider_factory_);
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
    std::optional<std::string> scalability_mode) const {
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

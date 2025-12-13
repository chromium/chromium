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
#include "media/base/supported_types.h"
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
#include "third_party/webrtc/modules/video_coding/svc/scalability_mode_util.h"

#if BUILDFLAG(RTC_USE_H265)
#include "third_party/webrtc/api/video_codecs/h265_profile_tier_level.h"
#endif  // BUILDFLAG(RTC_USE_H265)

namespace blink {

namespace {

// Convert media::SVCScalabilityMode to webrtc::ScalabilityMode and fill
// format.scalability_modes.
void FillScalabilityModes(
    webrtc::SdpVideoFormat& format,
    const media::VideoEncodeAccelerator::SupportedProfile& profile) {
  bool disable_h265_l1t2 =
      !base::FeatureList::IsEnabled(::features::kWebRtcH265L1T2);
  bool disable_h265_l1t3 =
      disable_h265_l1t2 ||
      !base::FeatureList::IsEnabled(::features::kWebRtcH265L1T3);

  for (const media::SVCScalabilityMode& mode : profile.scalability_modes) {
    std::optional<webrtc::ScalabilityMode> scalability_mode =
        webrtc::ScalabilityModeFromString(media::GetScalabilityModeName(mode));
    if (!scalability_mode.has_value()) {
      LOG(WARNING) << "Unrecognized SVC scalability mode: "
                   << media::GetScalabilityModeName(mode);
      continue;
    }

    if (profile.profile >= media::HEVCPROFILE_MIN &&
        profile.profile <= media::HEVCPROFILE_MAX) {
      if ((scalability_mode == webrtc::ScalabilityMode::kL1T2 &&
           disable_h265_l1t2) ||
          (scalability_mode == webrtc::ScalabilityMode::kL1T3 &&
           disable_h265_l1t3)) {
        continue;
      }
    }

    format.scalability_modes.push_back(scalability_mode.value());
  }
}

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
    webrtc::SdpVideoFormat format("VP8");
    FillScalabilityModes(format, profile);
    return format;
  }
  if (profile.profile >= media::H264PROFILE_MIN &&
      profile.profile <= media::H264PROFILE_MAX) {
    if (!UseH264AcceleratedEncoderForWebRTC()) {
      return std::nullopt;
    }

    webrtc::H264Profile h264_profile;
    switch (profile.profile) {
      case media::H264PROFILE_BASELINE:
        h264_profile = webrtc::H264Profile::kProfileBaseline;
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
    const std::optional<std::string> h264_profile_level_string =
        webrtc::H264ProfileLevelIdToString(profile_level_id);
    if (!h264_profile_level_string) {
      // Unsupported combination of profile and level.
      return std::nullopt;
    }

    webrtc::SdpVideoFormat format("H264");
    format.parameters = {
        {webrtc::kH264FmtpProfileLevelId, *h264_profile_level_string},
        {webrtc::kH264FmtpLevelAsymmetryAllowed, "1"},
        {webrtc::kH264FmtpPacketizationMode, "1"}};
    FillScalabilityModes(format, profile);
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
    FillScalabilityModes(format, profile);
    return format;
  }

  if (profile.profile >= media::AV1PROFILE_MIN &&
      profile.profile <= media::AV1PROFILE_MAX) {
    webrtc::SdpVideoFormat format("AV1");
    FillScalabilityModes(format, profile);
    return format;
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
        {webrtc::kH265FmtpProfileId,
         webrtc::H265ProfileToString(profile_tier_level.profile)},
        {webrtc::kH265FmtpTierFlag,
         webrtc::H265TierToString(profile_tier_level.tier)},
        {webrtc::kH265FmtpLevelId,
         webrtc::H265LevelToString(profile_tier_level.level)},
        {webrtc::kH265FmtpTxMode, "SRST"}};
    FillScalabilityModes(format, profile);
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
  std::vector<webrtc::SdpVideoFormat> sdp_formats
      ALLOW_DISCOURAGED_TYPE("Matches webrtc API");
};

#if BUILDFLAG(RTC_USE_H265)
// Insert or replace the H.265 format in |supported_formats| with the higher
// level for the same profile. Assume VEA always reports same scalability modes
// for the same video profile, the scalability mode of the highest level format
// will be used, and we don't handle the case that same profile has different
// scalability modes.
void InsertOrReplaceWithHigherLevelH265Format(
    SupportedFormats* supported_formats,
    const webrtc::SdpVideoFormat& format,
    media::VideoCodecProfile profile) {
  std::optional<webrtc::H265ProfileTierLevel> new_profile_tier_level =
      webrtc::ParseSdpForH265ProfileTierLevel(format.parameters);
  if (!new_profile_tier_level.has_value()) {
    return;
  }

  DCHECK_EQ(supported_formats->profiles.size(),
            supported_formats->sdp_formats.size());

  std::optional<webrtc::H265ProfileTierLevel> existing_profile_tier_level;
  auto profile_it = std::find(supported_formats->profiles.begin(),
                              supported_formats->profiles.end(), profile);

  if (profile_it != supported_formats->profiles.end()) {
    auto index = std::distance(supported_formats->profiles.begin(), profile_it);
    existing_profile_tier_level = webrtc::ParseSdpForH265ProfileTierLevel(
        supported_formats->sdp_formats[index].parameters);

    if (existing_profile_tier_level.has_value() &&
        new_profile_tier_level->level > existing_profile_tier_level->level) {
      supported_formats->sdp_formats[index] = format;
    }
  } else {
    supported_formats->sdp_formats.push_back(format);
    supported_formats->profiles.push_back(profile);
  }
}
#endif

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
    // Skip if profile is OS software encoder profile and we don't allow use
    // OS software encoder.
    if (profile.is_software_codec &&
        !media::MayHaveAndAllowSelectOSSoftwareEncoder(
            media::VideoCodecProfileToVideoCodec(profile.profile))) {
      continue;
    }

    if (base::Contains(disabled_profiles, profile.profile)) {
      continue;
    }

    std::optional<webrtc::SdpVideoFormat> format = VEAToWebRTCFormat(profile);
    if (format) {
      if (format->IsCodecInList(supported_formats.sdp_formats)) {
        continue;
      }
      // Supported H.265 formats must be added to the end of supported codecs.
#if BUILDFLAG(RTC_USE_H265)
      if (format->name == webrtc::kH265CodecName) {
        // Avoid having duplicated formats reported via GetSupportedFormats().
        // Also ensure only the highest level format is reported for the same
        // H.265 profile.
        InsertOrReplaceWithHigherLevelH265Format(
            &low_priority_formats, format.value(), profile.profile);
        continue;
      }
#endif  // BUILDFLAG(RTC_USE_H265)
      supported_formats.profiles.push_back(profile.profile);
      supported_formats.sdp_formats.push_back(std::move(*format));

      const bool kShouldAddH264Cbp =
          IsH264ConstrainedBaselineProfileAvailableForAcceleratedEncoder() &&
          profile.profile == media::VideoCodecProfile::H264PROFILE_BASELINE;

      if (kShouldAddH264Cbp) {
        supported_formats.profiles.push_back(profile.profile);
        webrtc::AddH264ConstrainedBaselineProfileToSupportedFormats(
            &supported_formats.sdp_formats);
      }
    }
  }

  supported_formats.profiles.insert(supported_formats.profiles.end(),
                                    low_priority_formats.profiles.begin(),
                                    low_priority_formats.profiles.end());
  supported_formats.sdp_formats.insert(supported_formats.sdp_formats.end(),
                                       low_priority_formats.sdp_formats.begin(),
                                       low_priority_formats.sdp_formats.end());

  DCHECK_EQ(supported_formats.profiles.size(),
            supported_formats.sdp_formats.size());

  return supported_formats;
}

bool IsConstrainedH264(const webrtc::SdpVideoFormat& format) {
  bool is_constrained_h264 = false;

  if (format.name == webrtc::kH264CodecName) {
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

}  // anonymous namespace

RTCVideoEncoderFactory::RTCVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
        encoder_metrics_provider_factory,
    bool override_disabled_profiles)
    : RTCVideoEncoderFactory(gpu_factories,
                             std::move(encoder_metrics_provider_factory)) {
  if (override_disabled_profiles) {
    disabled_profiles_.clear();
  }
}

RTCVideoEncoderFactory::RTCVideoEncoderFactory(
    media::GpuVideoAcceleratorFactories* gpu_factories,
    scoped_refptr<media::MojoVideoEncoderMetricsProviderFactory>
        encoder_metrics_provider_factory)
    : gpu_factories_(gpu_factories),
      encoder_metrics_provider_factory_(
          std::move(encoder_metrics_provider_factory)),
      gpu_codec_support_waiter_(gpu_factories) {
  if (!base::FeatureList::IsEnabled(::features::kWebRtcAV1HWEncode)) {
    disabled_profiles_.emplace_back(media::AV1PROFILE_PROFILE_MAIN);
    disabled_profiles_.emplace_back(media::AV1PROFILE_PROFILE_HIGH);
    disabled_profiles_.emplace_back(media::AV1PROFILE_PROFILE_PRO);
  }

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
#if BUILDFLAG(RTC_USE_H265)
      // For H.265 we further check that the level-id supported is no smaller
      // than that being queried.
      if (format.name == webrtc::kH265CodecName) {
        const std::optional<webrtc::H265ProfileTierLevel> profile_tier_level =
            webrtc::ParseSdpForH265ProfileTierLevel(format.parameters);
        if (profile_tier_level) {
          const std::optional<webrtc::H265ProfileTierLevel> supported_profile =
              webrtc::ParseSdpForH265ProfileTierLevel(
                  supported_formats.sdp_formats[i].parameters);
          if (supported_profile &&
              profile_tier_level->level > supported_profile->level) {
            return {/*is_supported=*/false, /*is_power_efficient=*/false};
          }
        } else {
          // If invalid format parameters are passed, we should not support it.
          break;
        }
      }
#endif  // BUILDFLAG(RTC_USE_H265)
      std::optional<webrtc::ScalabilityMode> mode =
          scalability_mode.has_value()
              ? webrtc::ScalabilityModeFromString(scalability_mode.value())
              : std::nullopt;
      if (!scalability_mode ||
          (mode.has_value() &&
           base::Contains(supported_formats.sdp_formats[i].scalability_modes,
                          mode.value()))) {
        return {/*is_supported=*/true, /*is_power_efficient=*/true};
      }
      break;
    }
  }
  return {/*is_supported=*/false, /*is_power_efficient=*/false};
}

}  // namespace blink

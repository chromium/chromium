// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/encoding_support.h"

#include <algorithm>
#include <bitset>
#include <optional>
#include <string>
#include <string_view>

#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/media_buildflags.h"
#include "ui/gfx/geometry/size.h"

namespace media::cast::encoding_support {
namespace {

using VideoCodecBitset =
    std::bitset<static_cast<size_t>(VideoCodec::kMaxValue) + 1>;

static VideoCodecBitset& GetHardwareCodecDenyList() {
  static VideoCodecBitset kInstance;
  return kInstance;
}

// Configuration parameters used for determining if hardware encoding is enabled
// for a codec.
struct HardwareCodecParameters {
  // Minimum and maximum supported profiles for the codec.
  VideoCodecProfile min_profile;
  VideoCodecProfile max_profile;

  // Command line switches to force enable/disable hardware encoding.
  const char* force_enable_switch;
  const char* force_disable_switch;

  // Platform-specific feature flag to control hardware encoding. Can be null if
  // no feature flag is used.
  raw_ptr<const base::Feature> feature;
};

std::optional<HardwareCodecParameters> GetHardwareCodecParameters(
    VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
      return HardwareCodecParameters{
          H264PROFILE_MIN, H264PROFILE_MAX,
          switches::kCastStreamingForceEnableHardwareH264,
          switches::kCastStreamingForceDisableHardwareH264,
#if BUILDFLAG(IS_MAC)
          &kCastStreamingMacHardwareH264
#elif BUILDFLAG(IS_WIN)
          &kCastStreamingWinHardwareH264
#else
          nullptr
#endif
      };
    case VideoCodec::kHEVC:
      return HardwareCodecParameters{
          HEVCPROFILE_MIN, HEVCPROFILE_MAX,
          switches::kCastStreamingForceEnableHardwareHevc,
          switches::kCastStreamingForceDisableHardwareHevc,
          &media::kCastStreamingHardwareHevc};
    case VideoCodec::kVP8:
      return HardwareCodecParameters{
          VP8PROFILE_MIN, VP8PROFILE_MAX,
          switches::kCastStreamingForceEnableHardwareVp8,
          switches::kCastStreamingForceDisableHardwareVp8, &kCastStreamingVp8};
    case VideoCodec::kVP9:
      return HardwareCodecParameters{
          VP9PROFILE_MIN, VP9PROFILE_MAX,
          switches::kCastStreamingForceEnableHardwareVp9,
          switches::kCastStreamingForceDisableHardwareVp9, &kCastStreamingVp9};
    case VideoCodec::kAV1:
      return HardwareCodecParameters{
          AV1PROFILE_MIN, AV1PROFILE_MAX,
          switches::kCastStreamingForceEnableHardwareAv1,
          switches::kCastStreamingForceDisableHardwareAv1, &kCastStreamingAv1};
    default:
      return std::nullopt;
  }
}

bool IsCastStreamingAv1Enabled() {
#if BUILDFLAG(ENABLE_LIBAOM)
  return base::FeatureList::IsEnabled(kCastStreamingAv1);
#else
  return false;
#endif
}

bool IsHardwareEncodingEnabled(
    const std::vector<VideoEncodeAccelerator::SupportedProfile>& profiles,
    VideoCodecProfile min_profile,
    VideoCodecProfile max_profile,
    gfx::Size requested_resolution,
    double requested_frame_rate) {
  return std::ranges::any_of(profiles, [&](const auto& vea_profile) {
    if (vea_profile.profile < min_profile ||
        vea_profile.profile > max_profile) {
      return false;
    }
    if (requested_resolution.width() < vea_profile.min_resolution.width() ||
        requested_resolution.height() < vea_profile.min_resolution.height() ||
        requested_resolution.width() > vea_profile.max_resolution.width() ||
        requested_resolution.height() > vea_profile.max_resolution.height()) {
      return false;
    }
    if (vea_profile.max_framerate_denominator == 0 ||
        vea_profile.max_framerate_numerator == 0) {
      return true;
    }
    const double max_fps =
        static_cast<double>(vea_profile.max_framerate_numerator) /
        vea_profile.max_framerate_denominator;
    return requested_frame_rate <= max_fps;
  });
}


constexpr int k1080pPixels = 1920 * 1080;

template <typename T>
T GetCodecLevel(gfx::Size resolution,
                double frame_rate,
                T level_low,
                T level_high) {
  if (resolution.Area64() > static_cast<uint64_t>(k1080pPixels) ||
      frame_rate > 30.0) {
    return level_high;
  }
  return level_low;
}

// Configuration parameters for generating codec-specific MIME parameter
// strings.
struct CodecParameterConfig {
  VideoCodec codec;
  // Prepending string for the codec parameter.
  std::string_view prefix;
  // Appending string for the codec parameter.
  std::string_view suffix;
  // Level representing <= 1080p30.
  std::string_view level_low;
  // Level representing > 1080p30.
  std::string_view level_high;
};

constexpr CodecParameterConfig kCodecParameterConfigs[] = {
    {
        VideoCodec::kH264,
        "avc1",  // Prefix for H.264 (AVC)
        "",      // Suffix is generated by BuildH264MimeSuffix
        "40",    // Level 4.0 (decimal 40) supports up to 1080p30 (H.264 Table
                 // A-1).
        "42",    // Level 4.2 (decimal 42) supports up to 1080p60 (H.264 Table
                 // A-1).
    },
    {
        VideoCodec::kVP9,
        "vp09.00.",  // Profile 0
        ".08",       // 8-bit depth
        "40",        // Level 4.0 supports up to 1080p30 (VP9 Spec Annex A).
        "41",        // Level 4.1 supports up to 1080p60 (VP9 Spec Annex A).
    },
    {
        VideoCodec::kHEVC,
        "hev1.1.6.L",  // Profile 1 (Main), compatibility flags, Tier L (Main)
        ".B0",         // Constraint flags
        "120",  // Level 4.0 (level * 30 = 120) supports up to 1080p30 (H.265
                // Table A.6).
        "123",  // Level 4.1 (level * 30 = 123) supports up to 1080p60 (H.265
                // Table A.6).
    },
    {
        VideoCodec::kAV1,
        "av01.0.",  // Profile 0 (Main)
        ".08",      // 8-bit depth
        "08M",      // Level 4.0 (08), Tier M (Main) supports up to 1080p30 (AV1
                    // Spec Annex A).
        "09M",      // Level 4.1 (09), Tier M (Main) supports up to 1080p60 (AV1
                    // Spec Annex A).
    },
};

}  // namespace

bool IsSoftwareEnabled(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kVP8:
      return base::FeatureList::IsEnabled(kCastStreamingVp8);

    case VideoCodec::kVP9:
      return base::FeatureList::IsEnabled(kCastStreamingVp9);

    case VideoCodec::kAV1:
      return IsCastStreamingAv1Enabled();

    // The test infrastructure is responsible for ensuring the fake codec is
    // used properly.
    case VideoCodec::kUnknown:
      return true;

    default:
      return false;
  }
}

bool IsHardwareEnabled(
    VideoCodec codec,
    const std::vector<VideoEncodeAccelerator::SupportedProfile>& profiles,
    gfx::Size requested_resolution,
    double requested_frame_rate) {
  if (IsHardwareDenyListed(codec)) {
    return false;
  }

  auto config = GetHardwareCodecParameters(codec);
  if (!config) {
    return false;
  }

  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();

  if (command_line.HasSwitch(config->force_disable_switch)) {
    return false;
  }

  bool feature_enabled = true;
  if (config->feature) {
    feature_enabled = base::FeatureList::IsEnabled(*config->feature);
  }

  if (!feature_enabled &&
      !command_line.HasSwitch(config->force_enable_switch)) {
    return false;
  }

  return IsHardwareEncodingEnabled(profiles, config->min_profile,
                                   config->max_profile, requested_resolution,
                                   requested_frame_rate);
}

bool IsHardwareDenyListed(VideoCodec codec) {
  return GetHardwareCodecDenyList().test(static_cast<size_t>(codec));
}

void DenyListHardwareCodec(VideoCodec codec) {
  // Codecs should not be disabled multiple times. This likely means that we
  // offered it again when we shouldn't have, somehow.
  CHECK(!IsHardwareDenyListed(codec));
  GetHardwareCodecDenyList().set(static_cast<size_t>(codec));
}

void ClearHardwareCodecDenyListForTesting() {
  GetHardwareCodecDenyList().reset();
}

VideoCodecProfile ToProfile(VideoCodec codec) {
  switch (codec) {
    case VideoCodec::kH264:
#if BUILDFLAG(IS_MAC)
      if (base::FeatureList::IsEnabled(media::kCastMacForceBaselineProfile)) {
        return H264PROFILE_BASELINE;
      }
#endif
      return H264PROFILE_MAIN;
    case VideoCodec::kHEVC:
      return HEVCPROFILE_MAIN;
    case VideoCodec::kVP8:
      return VP8PROFILE_ANY;
    case VideoCodec::kVP9:
      return VP9PROFILE_PROFILE0;
    case VideoCodec::kAV1:
      return AV1PROFILE_PROFILE_MAIN;
    default:
      NOTREACHED() << "Unhandled codec. value=" << static_cast<int>(codec);
  }
}

std::string GetCodecParameterString(VideoCodec codec,
                                    gfx::Size resolution,
                                    double frame_rate) {
  const auto* it =
      std::ranges::find_if(kCodecParameterConfigs,
                           [codec](const auto& c) { return c.codec == codec; });

  if (it == std::ranges::end(kCodecParameterConfigs)) {
    return std::string();
  }
  const auto& config = *it;

  std::string_view level_str = GetCodecLevel(
      resolution, frame_rate, config.level_low, config.level_high);

  // H264 is a special case, since we need to call into BuildH264MimeSuffix to
  // get the correct profile information.
  if (codec == VideoCodec::kH264) {
    const VideoCodecProfile profile = ToProfile(codec);
    unsigned int level_int = 0;
    CHECK(base::StringToUint(level_str, &level_int));
    std::string suffix =
        media::BuildH264MimeSuffix(profile, static_cast<uint8_t>(level_int));
    return base::StrCat({config.prefix, suffix});
  }

  return base::StrCat({config.prefix, level_str, config.suffix});
}

}  //  namespace media::cast::encoding_support

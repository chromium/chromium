// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/encoding_support.h"

#include <algorithm>
#include <bitset>
#include <string>
#include <string_view>

#include "base/command_line.h"
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
  static VideoCodecBitset* const kInstance = new VideoCodecBitset();
  return *kInstance;
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
  for (const auto& vea_profile : profiles) {
    if (vea_profile.profile < min_profile ||
        vea_profile.profile > max_profile) {
      continue;
    }
    if (requested_resolution.width() < vea_profile.min_resolution.width() ||
        requested_resolution.height() < vea_profile.min_resolution.height() ||
        requested_resolution.width() > vea_profile.max_resolution.width() ||
        requested_resolution.height() > vea_profile.max_resolution.height()) {
      continue;
    }
    if (vea_profile.max_framerate_denominator == 0 ||
        vea_profile.max_framerate_numerator == 0) {
      return true;
    }
    const double max_fps =
        static_cast<double>(vea_profile.max_framerate_numerator) /
        vea_profile.max_framerate_denominator;
    if (requested_frame_rate <= max_fps) {
      return true;
    }
  }
  return false;
}

// Scan profiles for hardware H.264 encoder support.
bool IsHardwareH264EncodingEnabled(
    const std::vector<VideoEncodeAccelerator::SupportedProfile>& profiles,
    gfx::Size requested_resolution,
    double requested_frame_rate) {
  // Force disabling takes precedent over other flags.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(
          switches::kCastStreamingForceDisableHardwareH264)) {
    return false;
  }

#if BUILDFLAG(IS_MAC)
  if (!command_line.HasSwitch(
          switches::kCastStreamingForceEnableHardwareH264) &&
      !base::FeatureList::IsEnabled(kCastStreamingMacHardwareH264)) {
    return false;
  }
#endif

#if BUILDFLAG(IS_WIN)
  // TODO(crbug.com/40653760): Now that we have software fallback for hardware
  // encoders, it is okay to enable hardware H264 for windows, as the one to
  // two percent of sessions that fail can gracefully fallback.
  if (!command_line.HasSwitch(
          switches::kCastStreamingForceEnableHardwareH264) &&
      !base::FeatureList::IsEnabled(kCastStreamingWinHardwareH264)) {
    return false;
  }
#endif

  return IsHardwareEncodingEnabled(profiles, H264PROFILE_MIN, H264PROFILE_MAX,
                                   requested_resolution, requested_frame_rate);
}

// Scan profiles for hardware HEVC encoder support.
bool IsHardwareHevcEncodingEnabled(
    const std::vector<VideoEncodeAccelerator::SupportedProfile>& profiles,
    gfx::Size requested_resolution,
    double requested_frame_rate) {
  if (!base::FeatureList::IsEnabled(media::kCastStreamingHardwareHevc)) {
    return false;
  }

  return IsHardwareEncodingEnabled(profiles, HEVCPROFILE_MIN, HEVCPROFILE_MAX,
                                   requested_resolution, requested_frame_rate);
}

// Scan profiles for hardware VP8 encoder support.
bool IsHardwareVP8EncodingEnabled(
    const std::vector<VideoEncodeAccelerator::SupportedProfile>& profiles,
    gfx::Size requested_resolution,
    double requested_frame_rate) {
  if (!base::FeatureList::IsEnabled(kCastStreamingVp8)) {
    return false;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCastStreamingForceDisableHardwareVp8)) {
    return false;
  }

  // Currently the kCastStreamingForceEnableHardwareVp8 is ignored, since no
  // platforms have it disabled.
  return IsHardwareEncodingEnabled(profiles, VP8PROFILE_MIN, VP8PROFILE_MAX,
                                   requested_resolution, requested_frame_rate);
}

// Scan profiles for hardware VP9 encoder support.
bool IsHardwareVP9EncodingEnabled(
    const std::vector<VideoEncodeAccelerator::SupportedProfile>& profiles,
    gfx::Size requested_resolution,
    double requested_frame_rate) {
  // Don't offer hardware if VP9 is not enabled at all.
  if (!base::FeatureList::IsEnabled(kCastStreamingVp9)) {
    return false;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCastStreamingForceDisableHardwareVp9)) {
    return false;
  }

  // Currently the kCastStreamingForceEnableHardwareVp9 is ignored, since no
  // platforms have it disabled.
  return IsHardwareEncodingEnabled(profiles, VP9PROFILE_MIN, VP9PROFILE_MAX,
                                   requested_resolution, requested_frame_rate);
}

constexpr int k1080pPixels = 1920 * 1080;

uint8_t GetH264Level(gfx::Size resolution, double frame_rate) {
  constexpr uint8_t kLevel1080p30 =
      40;  // Level 4.0 supports up to 1080p30 (H.264 Table A-1).
  constexpr uint8_t kLevel1080p60 =
      42;  // Level 4.2 supports up to 1080p60 (H.264 Table A-1).

  if (resolution.Area64() > static_cast<uint64_t>(k1080pPixels) ||
      frame_rate > 30.0) {
    return kLevel1080p60;
  }
  return kLevel1080p30;
}

std::string_view GetVP9Level(gfx::Size resolution, double frame_rate) {
  static constexpr char kLevel1080p30[] =
      "40";  // Level 4.0 supports up to 1080p30 (VP9 Spec Annex A).
  static constexpr char kLevel1080p60[] =
      "41";  // Level 4.1 supports up to 1080p60 (VP9 Spec Annex A).

  if (resolution.Area64() > static_cast<uint64_t>(k1080pPixels) ||
      frame_rate > 30.0) {
    return kLevel1080p60;
  }
  return kLevel1080p30;
}

std::string_view GetHEVCLevel(gfx::Size resolution, double frame_rate) {
  static constexpr char kLevel1080p30[] =
      "120";  // Level 4.0 supports up to 1080p30 (H.265 Table A.6).
  static constexpr char kLevel1080p60[] =
      "123";  // Level 4.1 supports up to 1080p60 (H.265 Table A.6).

  if (resolution.Area64() > static_cast<uint64_t>(k1080pPixels) ||
      frame_rate > 30.0) {
    return kLevel1080p60;
  }
  return kLevel1080p30;
}

std::string_view GetAV1Level(gfx::Size resolution, double frame_rate) {
  static constexpr char kLevel1080p30[] =
      "08M";  // Level 4.0 supports up to 1080p30 (AV1 Spec Annex A).
  static constexpr char kLevel1080p60[] =
      "09M";  // Level 4.1 supports up to 1080p60 (AV1 Spec Annex A).

  if (resolution.Area64() > static_cast<uint64_t>(k1080pPixels) ||
      frame_rate > 30.0) {
    return kLevel1080p60;
  }
  return kLevel1080p30;
}

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

  switch (codec) {
    case VideoCodec::kH264:
      return IsHardwareH264EncodingEnabled(profiles, requested_resolution,
                                           requested_frame_rate);

    case VideoCodec::kHEVC:
      return IsHardwareHevcEncodingEnabled(profiles, requested_resolution,
                                           requested_frame_rate);

    case VideoCodec::kVP8:
      return IsHardwareVP8EncodingEnabled(profiles, requested_resolution,
                                          requested_frame_rate);

    case VideoCodec::kVP9:
      return IsHardwareVP9EncodingEnabled(profiles, requested_resolution,
                                          requested_frame_rate);

    default:
      return false;
  }
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
  switch (codec) {
    case VideoCodec::kH264: {
      VideoCodecProfile profile = ToProfile(codec);
      uint8_t level = GetH264Level(resolution, frame_rate);
      std::string suffix = media::BuildH264MimeSuffix(profile, level);
      return "avc1" + suffix;
    }
    case VideoCodec::kVP8:
      return std::string();
    case VideoCodec::kVP9: {
      return "vp09.00." + std::string(GetVP9Level(resolution, frame_rate)) +
             ".08";
    }
    case VideoCodec::kHEVC: {
      return "hev1.1.6.L" + std::string(GetHEVCLevel(resolution, frame_rate)) +
             ".B0";
    }
    case VideoCodec::kAV1: {
      return "av01.0." + std::string(GetAV1Level(resolution, frame_rate)) +
             ".08";
    }
    default:
      return std::string();
  }
}

}  //  namespace media::cast::encoding_support

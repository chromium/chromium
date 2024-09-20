// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/encoding/encoding_support.h"

#include <algorithm>
#include <bitset>

#include "base/command_line.h"
#include "build/build_config.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/cast/encoding/external_video_encoder.h"
#include "third_party/libaom/libaom_buildflags.h"

namespace media::cast::encoding_support {
namespace {

using VideoCodecBitset =
    std::bitset<static_cast<size_t>(VideoCodec::kMaxValue) + 1>;

static VideoCodecBitset& GetHardwareCodecDenyList() {
  static VideoCodecBitset* const kInstance = new VideoCodecBitset();
  return *kInstance;
}

bool IsHardwareDenyListed(VideoCodec codec) {
  return GetHardwareCodecDenyList().test(static_cast<size_t>(codec));
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
    VideoCodecProfile max_profile) {
  for (const auto& vea_profile : profiles) {
    if (vea_profile.profile >= min_profile &&
        vea_profile.profile <= max_profile) {
      return true;
    }
  }
  return false;
}

// Scan profiles for hardware VP8 encoder support.
bool IsHardwareVP8EncodingEnabled(
    const std::vector<VideoEncodeAccelerator::SupportedProfile>& profiles) {
  if (!base::FeatureList::IsEnabled(kCastStreamingVp8)) {
    return false;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kCastStreamingForceDisableHardwareVp8)) {
    return false;
  }

  // Currently the kCastStreamingForceEnableHardwareVp8 is ignored, since no
  // platforms have it disabled.
  return IsHardwareEncodingEnabled(profiles, VP8PROFILE_MIN, VP8PROFILE_MAX);
}

// Scan profiles for hardware VP9 encoder support.
bool IsHardwareVP9EncodingEnabled(
    const std::vector<VideoEncodeAccelerator::SupportedProfile>& profiles) {
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
  return IsHardwareEncodingEnabled(profiles, VP9PROFILE_MIN, VP9PROFILE_MAX);
}

// Scan profiles for hardware H.264 encoder support.
bool IsHardwareH264EncodingEnabled(
    const std::vector<VideoEncodeAccelerator::SupportedProfile>& profiles) {
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

  return IsHardwareEncodingEnabled(profiles, H264PROFILE_MIN, H264PROFILE_MAX);
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
    const std::vector<VideoEncodeAccelerator::SupportedProfile>& profiles) {
  if (IsHardwareDenyListed(codec)) {
    return false;
  }

  switch (codec) {
    case VideoCodec::kVP8:
      return IsHardwareVP8EncodingEnabled(profiles);

    case VideoCodec::kVP9:
      return IsHardwareVP9EncodingEnabled(profiles);

    case VideoCodec::kH264:
      return IsHardwareH264EncodingEnabled(profiles);

    default:
      return false;
  }
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

}  //  namespace media::cast::encoding_support

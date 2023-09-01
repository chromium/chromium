// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/v4l2/stateless/utils.h"

#include "media/gpu/macros.h"

namespace media {

VideoDecodeAccelerator::SupportedProfiles GetSupportedDecodeProfiles(
    Device* device) {
  VideoDecodeAccelerator::SupportedProfiles supported_profiles;

  auto input_codecs = device->EnumerateInputFormats();

  for (VideoCodec codec : input_codecs) {
    VideoDecodeAccelerator::SupportedProfile profile;

    const auto resolution_range = device->GetFrameResolutionRange(codec);
    profile.min_resolution = resolution_range.first;
    profile.max_resolution = resolution_range.second;

    auto video_codec_profiles = device->ProfilesForVideoCodec(codec);
    for (const auto& video_codec_profile : video_codec_profiles) {
      profile.profile = video_codec_profile;
      supported_profiles.push_back(profile);

      DVLOGF(3) << "Found decoder profile " << GetProfileName(profile.profile)
                << ", resolutions: " << profile.min_resolution.ToString() << " "
                << profile.max_resolution.ToString();
    }
  }

  return supported_profiles;
}

}  // namespace media

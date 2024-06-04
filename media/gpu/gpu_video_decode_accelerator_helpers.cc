// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/gpu_video_decode_accelerator_helpers.h"

namespace media {

SupportedVideoDecoderConfigs ConvertFromSupportedProfiles(
    const VideoDecodeAccelerator::SupportedProfiles& profiles,
    bool allow_encrypted) {
  SupportedVideoDecoderConfigs configs;
  for (const auto& profile : profiles) {
    configs.push_back(SupportedVideoDecoderConfig(
        profile.profile,           // profile_min
        profile.profile,           // profile_max
        profile.min_resolution,    // coded_size_min
        profile.max_resolution,    // coded_size_max
        allow_encrypted,           // allow_encrypted
        profile.encrypted_only));  // require_encrypted);
  }
  return configs;
}

}  // namespace media

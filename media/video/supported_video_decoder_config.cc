// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/video/supported_video_decoder_config.h"

namespace media {

SupportedVideoDecoderConfig::SupportedVideoDecoderConfig() = default;

SupportedVideoDecoderConfig::SupportedVideoDecoderConfig(
    VideoCodecProfile profile_min,
    VideoCodecProfile profile_max,
    const gfx::Size& coded_size_min,
    const gfx::Size& coded_size_max,
    bool allow_encrypted,
    bool require_encrypted)
    : profile_min(profile_min),
      profile_max(profile_max),
      coded_size_min(coded_size_min),
      coded_size_max(coded_size_max),
      allow_encrypted(allow_encrypted),
      require_encrypted(require_encrypted) {}

SupportedVideoDecoderConfig::~SupportedVideoDecoderConfig() = default;

bool SupportedVideoDecoderConfig::Matches(
    const VideoDecoderConfig& config) const {
  if (config.profile() < profile_min || config.profile() > profile_max)
    return false;

  if (config.is_encrypted()) {
    if (!allow_encrypted)
      return false;
  } else {
    if (require_encrypted)
      return false;
  }

  if (config.coded_size().width() < coded_size_min.width())
    return false;
  if (config.coded_size().height() < coded_size_min.height())
    return false;

  if (config.coded_size().width() > coded_size_max.width())
    return false;
  if (config.coded_size().height() > coded_size_max.height())
    return false;

  return true;
}

// static
bool IsVideoDecoderConfigSupported(
    const SupportedVideoDecoderConfigs& supported_configs,
    const VideoDecoderConfig& config) {
  for (const auto& c : supported_configs) {
    if (c.Matches(config))
      return true;
  }
  return false;
}

}  // namespace media

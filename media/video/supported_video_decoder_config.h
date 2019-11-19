// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_SUPPORTED_VIDEO_DECODER_CONFIG_H_
#define MEDIA_VIDEO_SUPPORTED_VIDEO_DECODER_CONFIG_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// Specification of a range of configurations that are supported by a video
// decoder.  Also provides the ability to check if a VideoDecoderConfig matches
// the supported range.
struct MEDIA_EXPORT SupportedVideoDecoderConfig {
  SupportedVideoDecoderConfig();
  SupportedVideoDecoderConfig(VideoCodecProfile profile_min,
                              VideoCodecProfile profile_max,
                              const gfx::Size& coded_size_min,
                              const gfx::Size& coded_size_max,
                              bool allow_encrypted,
                              bool require_encrypted);
  ~SupportedVideoDecoderConfig();

  // Returns true if and only if |config| is a supported config.
  bool Matches(const VideoDecoderConfig& config) const;

  // Range of VideoCodecProfiles to match, inclusive.
  VideoCodecProfile profile_min = VIDEO_CODEC_PROFILE_UNKNOWN;
  VideoCodecProfile profile_max = VIDEO_CODEC_PROFILE_UNKNOWN;

  // Coded size range, inclusive.
  gfx::Size coded_size_min;
  gfx::Size coded_size_max;

  // TODO(liberato): consider switching these to "allow_clear" and
  // "allow_encrypted", so that they're orthogonal.

  // If true, then this will match encrypted configs.
  bool allow_encrypted = true;

  // If true, then unencrypted configs will not match.
  bool require_encrypted = false;

  // Allow copy and assignment.
};

// Enumeration of possible implementations for (Mojo)VideoDecoders.
enum class VideoDecoderImplementation {
  kDefault = 0,
  kAlternate = 1,
  kMaxValue = kAlternate
};

using SupportedVideoDecoderConfigs = std::vector<SupportedVideoDecoderConfig>;

// Map of mojo VideoDecoder implementations to the vector of configs that they
// (probably) support.
using SupportedVideoDecoderConfigMap =
    base::flat_map<VideoDecoderImplementation, SupportedVideoDecoderConfigs>;

// Helper method to determine if |config| is supported by |supported_configs|.
MEDIA_EXPORT bool IsVideoDecoderConfigSupported(
    const SupportedVideoDecoderConfigs& supported_configs,
    const VideoDecoderConfig& config);

}  // namespace media

#endif  // MEDIA_VIDEO_SUPPORTED_VIDEO_DECODER_CONFIG_H_

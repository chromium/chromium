// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_SUPPORTED_VIDEO_DECODER_CONFIG_H_
#define MEDIA_BASE_SUPPORTED_VIDEO_DECODER_CONFIG_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// The min and max resolution used by SW decoders (dav1d, libgav1, libvpx and
// ffmpeg for example) when queried about decoding capabilities. For now match
// the supported resolutions of HW decoders.
constexpr gfx::Size kDefaultSwDecodeSizeMin(8, 8);
constexpr gfx::Size kDefaultSwDecodeSizeMax(8192, 8192);

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

  bool operator==(const SupportedVideoDecoderConfig& other) const {
    return profile_min == other.profile_min &&
           profile_max == other.profile_max &&
           coded_size_min == other.coded_size_min &&
           coded_size_max == other.coded_size_max &&
           allow_encrypted == other.allow_encrypted &&
           require_encrypted == other.require_encrypted;
  }
  bool operator!=(const SupportedVideoDecoderConfig& other) const {
    return !(*this == other);
  }

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

using SupportedVideoDecoderConfigs = std::vector<SupportedVideoDecoderConfig>;

// Helper method to determine if |config| is supported by |supported_configs|.
MEDIA_EXPORT bool IsVideoDecoderConfigSupported(
    const SupportedVideoDecoderConfigs& supported_configs,
    const VideoDecoderConfig& config);

}  // namespace media

#endif  // MEDIA_BASE_SUPPORTED_VIDEO_DECODER_CONFIG_H_

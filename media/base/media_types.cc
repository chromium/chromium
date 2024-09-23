// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/media_types.h"

#include <tuple>

namespace media {

// static
AudioType AudioType::FromDecoderConfig(const AudioDecoderConfig& config) {
  return {config.codec(), config.profile(), false};
}

// static
VideoType VideoType::FromDecoderConfig(const VideoDecoderConfig& config) {
  // Level is not part of |config|. Its also not always known in the container
  // metadata (e.g. WebM puts it in CodecPrivate which is often not included).
  // Level is not used by /media to make/break support decisions, but
  // embedders with strict hardware limitations could theoretically check it.
  // The following attempts to make a safe guess by choosing the lowest level
  // for the given codec.

  // Zero is not a valid level for any of the following codecs. It means
  // "unknown" or "no level" (e.g. VP8).
  VideoCodecLevel level = 0;

  switch (config.codec()) {
    // These have no notion of level.
    case VideoCodec::kUnknown:
    case VideoCodec::kTheora:
    case VideoCodec::kVP8:
    // These use non-numeric levels, aren't part of our mime code, and
    // are ancient with very limited support.
    case VideoCodec::kVC1:
    case VideoCodec::kMPEG2:
    case VideoCodec::kMPEG4:
      break;
    case VideoCodec::kH264:
    case VideoCodec::kVP9:
    case VideoCodec::kHEVC:
      // 10 is the level_idc for level 1.0.
      level = 10;
      break;
    case VideoCodec::kDolbyVision:
      // Dolby doesn't do decimals, so 1 is just 1.
      level = 1;
      break;
    case VideoCodec::kAV1:
      // Strangely, AV1 starts at 2.0.
      level = 20;
      break;
  }

  return {config.codec(), config.profile(), level, config.color_space_info()};
}

bool operator==(const AudioType& x, const AudioType& y) {
  return std::tie(x.codec, x.profile, x.spatial_rendering) ==
         std::tie(y.codec, y.profile, y.spatial_rendering);
}

bool operator!=(const AudioType& x, const AudioType& y) {
  return !(x == y);
}

bool operator<(const AudioType& x, const AudioType& y) {
  return std::tie(x.codec, x.profile, x.spatial_rendering) <
         std::tie(y.codec, y.profile, y.spatial_rendering);
}

bool operator==(const VideoType& x, const VideoType& y) {
  return std::tie(x.codec, x.profile, x.level, x.color_space) ==
         std::tie(y.codec, y.profile, y.level, y.color_space);
}

bool operator!=(const VideoType& x, const VideoType& y) {
  return !(x == y);
}

}  // namespace media

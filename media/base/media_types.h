// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_MEDIA_TYPES_H_
#define MEDIA_BASE_MEDIA_TYPES_H_

#include <optional>

#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/base/video_decoder_config.h"

namespace media {

// These structures represent parsed audio/video content types (mime strings).
// These are generally a subset of {Audio|Video}DecoderConfig classes, which can
// only be created after demuxing.

struct MEDIA_EXPORT AudioType {
  static AudioType FromDecoderConfig(const AudioDecoderConfig& config);

  AudioCodec codec = AudioCodec::kUnknown;
  AudioCodecProfile profile = AudioCodecProfile::kUnknown;
  bool spatial_rendering = false;
};

struct MEDIA_EXPORT VideoType {
  static VideoType FromDecoderConfig(const VideoDecoderConfig& config);

  VideoCodec codec = VideoCodec::kUnknown;
  VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;
  VideoCodecLevel level = kNoVideoCodecLevel;
  VideoColorSpace color_space;
  gfx::HdrMetadataType hdr_metadata_type = gfx::HdrMetadataType::kNone;
  std::optional<VideoChromaSampling> subsampling;
  std::optional<uint8_t> bit_depth;
};

MEDIA_EXPORT bool operator==(const AudioType& x, const AudioType& y);
MEDIA_EXPORT bool operator!=(const AudioType& x, const AudioType& y);
MEDIA_EXPORT bool operator<(const AudioType& x, const AudioType& y);
MEDIA_EXPORT bool operator==(const VideoType& x, const VideoType& y);
MEDIA_EXPORT bool operator!=(const VideoType& x, const VideoType& y);

}  // namespace media

#endif  // MEDIA_BASE_MEDIA_TYPES_H_

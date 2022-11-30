// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MPEG_MPEG1_AUDIO_STREAM_PARSER_H_
#define MEDIA_FORMATS_MPEG_MPEG1_AUDIO_STREAM_PARSER_H_

#include <stdint.h>

#include "media/base/media_export.h"
#include "media/formats/mpeg/mpeg_audio_stream_parser_base.h"

namespace media {

// MPEG1AudioStreamParser handles MPEG-1 audio streams (ISO/IEC 11172-3)
// as well as the following extensions:
// - MPEG-2 audio (ISO/IEC 13818-3),
// - and MPEG2.5 (not an ISO standard).
class MEDIA_EXPORT MPEG1AudioStreamParser : public MPEGAudioStreamParserBase {
 public:
  // Size of an MPEG-1 frame header in bytes.
  static constexpr int kHeaderSize = 4;

  // Versions and layers as defined in ISO/IEC 11172-3.
  enum Version {
    kVersion1 = 3,
    kVersion2 = 2,
    kVersionReserved = 1,
    kVersion2_5 = 0,
  };

  enum Layer {
    kLayer1 = 3,
    kLayer2 = 2,
    kLayer3 = 1,
    kLayerReserved = 0,
  };

  struct Header {
    Version version;

    // Layer as defined in ISO/IEC 11172-3 bitstream specification.
    Layer layer;

    // Frame size in bytes.
    int frame_size;

    // Sample frequency.
    int sample_rate;

    // Channel mode as defined in ISO/IEC 11172-3 bitstream specification.
    int channel_mode;

    // Channel layout.
    ChannelLayout channel_layout;

    // Number of samples per frame.
    int sample_count;
  };

  // Parses the header starting at |data|.
  // Assumption: size of array |data| should be at least |kHeaderSize|.
  // Returns false if the header is not valid.
  static bool ParseHeader(MediaLog* media_log,
                          size_t* media_log_limit,
                          const uint8_t* data,
                          Header* header);

  MPEG1AudioStreamParser();

  MPEG1AudioStreamParser(const MPEG1AudioStreamParser&) = delete;
  MPEG1AudioStreamParser& operator=(const MPEG1AudioStreamParser&) = delete;

  ~MPEG1AudioStreamParser() override;

 private:
  // MPEGAudioStreamParserBase overrides.
  int ParseFrameHeader(const uint8_t* data,
                       int size,
                       int* frame_size,
                       int* sample_rate,
                       ChannelLayout* channel_layout,
                       int* sample_count,
                       bool* metadata_frame,
                       std::vector<uint8_t>* extra_data) override;

  size_t mp3_parse_error_limit_ = 0;
};

}  // namespace media

#endif  // MEDIA_FORMATS_MPEG_MPEG1_AUDIO_STREAM_PARSER_H_

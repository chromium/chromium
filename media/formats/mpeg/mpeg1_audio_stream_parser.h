// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MPEG_MPEG1_AUDIO_STREAM_PARSER_H_
#define MEDIA_FORMATS_MPEG_MPEG1_AUDIO_STREAM_PARSER_H_

#include <stdint.h>

#include <optional>

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
  static constexpr size_t kHeaderSize = 4;

  using Header = MPEGAudioStreamParserBase::Header;

  // Parses the header starting at |data|.
  // Assumption: size of array |data| should be at least |kHeaderSize|.
  // Returns false if the header is not valid.
  static std::optional<Header> ParseHeader(base::span<const uint8_t> data);

  MPEG1AudioStreamParser();

  MPEG1AudioStreamParser(const MPEG1AudioStreamParser&) = delete;
  MPEG1AudioStreamParser& operator=(const MPEG1AudioStreamParser&) = delete;

  ~MPEG1AudioStreamParser() override;

 private:
  // MPEGAudioStreamParserBase overrides.
  int ParseFrameHeader(base::span<const uint8_t> data,
                       size_t* frame_size,
                       size_t* sample_rate,
                       ChannelLayout* channel_layout,
                       size_t* sample_count,
                       bool* metadata_frame,
                       std::vector<uint8_t>* extra_data) override;

  size_t mp3_parse_error_limit_ = 0;
};

}  // namespace media

#endif  // MEDIA_FORMATS_MPEG_MPEG1_AUDIO_STREAM_PARSER_H_

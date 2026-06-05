// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MPEG_ADTS_STREAM_PARSER_H_
#define MEDIA_FORMATS_MPEG_ADTS_STREAM_PARSER_H_

#include <stdint.h>

#include <optional>

#include "media/base/media_export.h"
#include "media/formats/mpeg/mpeg_audio_stream_parser_base.h"

namespace media {

class MEDIA_EXPORT ADTSStreamParser : public MPEGAudioStreamParserBase {
 public:
  using Header = MPEGAudioStreamParserBase::Header;

  static std::optional<Header> ParseHeader(base::span<const uint8_t> data);

  ADTSStreamParser();

  ADTSStreamParser(const ADTSStreamParser&) = delete;
  ADTSStreamParser& operator=(const ADTSStreamParser&) = delete;

  ~ADTSStreamParser() override;

 private:
  // MPEGAudioStreamParserBase overrides.
  size_t GetMinHeaderSize() const override;
  std::optional<Header> ParseFrameHeader(
      base::span<const uint8_t> data) override;

  size_t adts_parse_error_limit_ = 0;
};

}  // namespace media

#endif  // MEDIA_FORMATS_MPEG_ADTS_STREAM_PARSER_H_

// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_DTS_DTS_STREAM_PARSER_H_
#define MEDIA_FORMATS_DTS_DTS_STREAM_PARSER_H_

#include <stdint.h>
#include <vector>

#include "media/base/media_export.h"
#include "media/formats/mpeg/mpeg_audio_stream_parser_base.h"

namespace media {

class MEDIA_EXPORT DTSStreamParser : public MPEGAudioStreamParserBase {
 public:
  DTSStreamParser();
  ~DTSStreamParser() override;

  enum {
    kDTSCoreHeaderSizeInBytes = 15,
    kDTSCoreSyncWord = 0x7ffe8001,
  };

  // MPEGAudioStreamParserBase overrides.
  int ParseFrameHeader(const uint8_t* data,
                       int size,
                       int* frame_size,
                       int* sample_rate,
                       ChannelLayout* channel_layout,
                       int* sample_count,
                       bool* metadata_frame,
                       std::vector<uint8_t>* extra_data) override;
};

}  // namespace media

#endif  // MEDIA_FORMATS_DTS_DTS_STREAM_PARSER_H_

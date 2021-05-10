// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_AAC_BITSTREAM_CONVERTER_H_
#define MEDIA_FILTERS_FFMPEG_AAC_BITSTREAM_CONVERTER_H_

#include <stdint.h>

#include "base/macros.h"

#include "media/base/media_export.h"
#include "media/filters/ffmpeg_bitstream_converter.h"

// Forward declarations for FFmpeg datatypes used.
struct AVCodecParameters;
struct AVPacket;

namespace media {

// Bitstream converter that adds ADTS headers to AAC frames.
class MEDIA_EXPORT FFmpegAACBitstreamConverter
    : public FFmpegBitstreamConverter {
 public:
  enum { kAdtsHeaderSize = 7 };

  // The |stream_codec_parameters| will be used during conversion and should be
  // the AVCodecParameters for the stream sourcing these packets. A reference to
  // |stream_codec_parameters| is retained, so it must outlive this class.
  explicit FFmpegAACBitstreamConverter(
      AVCodecParameters* stream_codec_parameters);
  ~FFmpegAACBitstreamConverter() override;

  // FFmpegBitstreamConverter implementation.
  // Uses FFmpeg allocation methods for buffer allocation to ensure
  // compatibility with FFmpeg's memory management.
  bool ConvertPacket(AVPacket* packet) override;

 private:
  // Variable to hold a pointer to memory where we can access the global
  // data from the FFmpeg file format's global headers.
  AVCodecParameters* stream_codec_parameters_;

  bool header_generated_;
  uint8_t hdr_[kAdtsHeaderSize];
  int codec_;
  int audio_profile_;
  int sample_rate_index_;
  int channel_configuration_;
  int frame_length_;

  DISALLOW_COPY_AND_ASSIGN(FFmpegAACBitstreamConverter);
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_AAC_BITSTREAM_CONVERTER_H_

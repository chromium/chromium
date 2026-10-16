// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_AAC_BITSTREAM_CONVERTER_H_
#define MEDIA_FILTERS_FFMPEG_AAC_BITSTREAM_CONVERTER_H_

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "base/memory/raw_ptr.h"
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
  static constexpr size_t kAdtsHeaderSize = 7;

  // The |stream_codec_parameters| will be used during conversion and should be
  // the AVCodecParameters for the stream sourcing these packets. A reference to
  // |stream_codec_parameters| is retained, so it must outlive this class.
  explicit FFmpegAACBitstreamConverter(
      AVCodecParameters* stream_codec_parameters);

  FFmpegAACBitstreamConverter(const FFmpegAACBitstreamConverter&) = delete;
  FFmpegAACBitstreamConverter& operator=(const FFmpegAACBitstreamConverter&) =
      delete;

  ~FFmpegAACBitstreamConverter() override;

  // FFmpegBitstreamConverter implementation.
  // Uses FFmpeg allocation methods for buffer allocation to ensure
  // compatibility with FFmpeg's memory management.
  bool ConvertPacket(AVPacket* packet) override;

 private:
  // Variable to hold a pointer to memory where we can access the global
  // data from the FFmpeg file format's global headers.
  raw_ptr<AVCodecParameters> stream_codec_parameters_;

  bool header_generated_ = false;
  std::array<uint8_t, kAdtsHeaderSize> hdr_{};
  int codec_ = 0;
  int audio_profile_ = 0;
  int sample_rate_index_ = 0;
  int channel_configuration_ = 0;
  int frame_length_ = 0;
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_AAC_BITSTREAM_CONVERTER_H_

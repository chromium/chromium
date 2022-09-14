// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_H265_TO_ANNEX_B_BITSTREAM_CONVERTER_H_
#define MEDIA_FILTERS_FFMPEG_H265_TO_ANNEX_B_BITSTREAM_CONVERTER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "media/base/media_export.h"
#include "media/filters/ffmpeg_bitstream_converter.h"
#include "media/formats/mp4/hevc.h"

// Forward declarations for FFmpeg datatypes used.
struct AVCodecParameters;
struct AVPacket;

namespace media {

// Bitstream converter that converts H.265 bitstream based FFmpeg packets into
// H.265 Annex B bytestream format.
class MEDIA_EXPORT FFmpegH265ToAnnexBBitstreamConverter
    : public FFmpegBitstreamConverter {
 public:
  // The |stream_codec_parameters| will be used during conversion and should be
  // the AVCodecParameters for the stream sourcing these packets. A reference to
  // |stream_codec_parameters| is retained, so it must outlive this class.
  explicit FFmpegH265ToAnnexBBitstreamConverter(
      AVCodecParameters* stream_codec_parameters);

  FFmpegH265ToAnnexBBitstreamConverter(
      const FFmpegH265ToAnnexBBitstreamConverter&) = delete;
  FFmpegH265ToAnnexBBitstreamConverter& operator=(
      const FFmpegH265ToAnnexBBitstreamConverter&) = delete;

  ~FFmpegH265ToAnnexBBitstreamConverter() override;

  // FFmpegBitstreamConverter implementation.
  bool ConvertPacket(AVPacket* packet) override;

 private:
  std::unique_ptr<mp4::HEVCDecoderConfigurationRecord> hevc_config_;

  // Variable to hold a pointer to memory where we can access the global
  // data from the FFmpeg file format's global headers.
  raw_ptr<AVCodecParameters> stream_codec_parameters_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_H265_TO_ANNEX_B_BITSTREAM_CONVERTER_H_

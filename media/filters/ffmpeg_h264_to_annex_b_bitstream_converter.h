// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FILTERS_FFMPEG_H264_TO_ANNEX_B_BITSTREAM_CONVERTER_H_
#define MEDIA_FILTERS_FFMPEG_H264_TO_ANNEX_B_BITSTREAM_CONVERTER_H_

#include "base/memory/raw_ptr.h"
#include "media/base/media_export.h"
#include "media/filters/ffmpeg_bitstream_converter.h"
#include "media/filters/h264_to_annex_b_bitstream_converter.h"

// Forward declarations for FFmpeg datatypes used.
struct AVCodecParameters;
struct AVPacket;

namespace media {

// Bitstream converter that converts H.264 bitstream based FFmpeg packets into
// H.264 Annex B bytestream format.
class MEDIA_EXPORT FFmpegH264ToAnnexBBitstreamConverter
    : public FFmpegBitstreamConverter {
 public:
  // The |stream_codec_parameters| will be used during conversion and should be
  // the AVCodecParameters for the stream sourcing these packets. A reference to
  // |stream_codec_parameters| is retained, so it must outlive this class.
  explicit FFmpegH264ToAnnexBBitstreamConverter(
      AVCodecParameters* stream_codec_parameters);

  FFmpegH264ToAnnexBBitstreamConverter(
      const FFmpegH264ToAnnexBBitstreamConverter&) = delete;
  FFmpegH264ToAnnexBBitstreamConverter& operator=(
      const FFmpegH264ToAnnexBBitstreamConverter&) = delete;

  ~FFmpegH264ToAnnexBBitstreamConverter() override;

  // FFmpegBitstreamConverter implementation.
  // Converts |packet| to H.264 Annex B bytestream format. This conversion is
  // on single NAL unit basis which is contained within the |packet| with the
  // exception of the first packet which is prepended with the AVC decoder
  // configuration record information. For example:
  //
  //    NAL unit #1 ==> bytestream buffer #1 (AVC configuraion + NAL unit #1)
  //    NAL unit #2 ==> bytestream buffer #2 (NAL unit #2)
  //    ...
  //    NAL unit #n ==> bytestream buffer #n (NAL unit #n)
  //
  // Returns true if conversion succeeded. In this case, the output will be
  // stored into the |packet|. But user should be aware that this conversion can
  // free and reallocate the |packet|, if it needs to do so to fit it in.
  // FFmpeg allocation methods will be used for buffer allocation to ensure
  // compatibility with FFmpeg's memory management.
  //
  // Returns false if conversion failed. In this case, the |packet| will not
  // be changed.
  bool ConvertPacket(AVPacket* packet) override;

 private:
  // Actual converter class.
  H264ToAnnexBBitstreamConverter converter_;

  // Flag for indicating whether global parameter sets have been processed.
  bool configuration_processed_;

  // Variable to hold a pointer to memory where we can access the global
  // data from the FFmpeg file format's global headers.
  raw_ptr<AVCodecParameters, AcrossTasksDanglingUntriaged>
      stream_codec_parameters_;
};

}  // namespace media

#endif  // MEDIA_FILTERS_FFMPEG_H264_TO_ANNEX_B_BITSTREAM_CONVERTER_H_

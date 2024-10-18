// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP4_H26X_ANNEX_B_TO_BITSTREAM_CONVERTER_H_
#define MEDIA_FORMATS_MP4_H26X_ANNEX_B_TO_BITSTREAM_CONVERTER_H_

#include <memory>
#include <vector>

#include "media/base/decoder_buffer.h"
#include "media/base/media_export.h"
#include "media/base/video_codecs.h"
#include "media/base/video_encoder.h"
#include "media/formats/mp4/h264_annex_b_to_avc_bitstream_converter.h"
#include "media/formats/mp4/mp4_status.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
#include "media/formats/mp4/h265_annex_b_to_hevc_bitstream_converter.h"
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

namespace media {

// H26xAnnexBToBitstreamConverter is a class to convert H.264 &
// H.265 bitstream from Annex B (ISO/IEC 14496-10) to AVC/HEVC (as
// specified in ISO/IEC 14496-15).
//
// It is a shim to the underlying `H264AnnexBToAvcBitstreamConverter` and
// `H265AnnexBToHevcBitstreamConverter`.
class MEDIA_EXPORT H26xAnnexBToBitstreamConverter {
 public:
  explicit H26xAnnexBToBitstreamConverter(VideoCodec video_codec);
  H26xAnnexBToBitstreamConverter(const H26xAnnexBToBitstreamConverter&) =
      delete;
  H26xAnnexBToBitstreamConverter& operator=(
      const H26xAnnexBToBitstreamConverter&) = delete;

  ~H26xAnnexBToBitstreamConverter();

  // Converts a video chunk from a format with in-place decoder configuration
  // into a format where configuration needs to be sent separately.
  //
  // |input| - where to read the data from
  //
  // NOTE: Caller needs to be careful using this function, and make sure the
  // conversion won't fail, otherwise there will be a `CHECK` failure.
  scoped_refptr<DecoderBuffer> Convert(base::span<const uint8_t> input);

  // Returns the latest version of codec description, found in converted
  // video chunks.
  VideoEncoder::CodecDescription GetCodecDescription();

  // Returns the latest version of codec/profile/level, found in converted
  // video chunks.
  CodecProfileLevel GetCodecProfileLevel();

 private:
  // Converts a video chunk from a format with in-place decoder configuration
  // into a format where configuration needs to be sent separately.
  //
  // |input| - where to read the data from
  // |output| - where to put the converted video data
  // If error kBufferTooSmall is returned, it means that |output| was not
  // big enough to contain a converted video chunk. In this case |size_out|
  // is populated.
  // |config_changed_out| is set to True if the video chunk
  // processed by this call contained decoder configuration information.
  // In this case latest codec description can be obtained
  // from GetCodecDescription().
  // |size_out| - number of bytes written to |output|, or desired size of
  // |output| if it's too small.
  MP4Status ConvertChunk(base::span<const uint8_t> input,
                         base::span<uint8_t> output,
                         bool* config_changed_out,
                         size_t* size_out);

  std::unique_ptr<H264AnnexBToAvcBitstreamConverter> h264_converter_;

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  std::unique_ptr<H265AnnexBToHevcBitstreamConverter> h265_converter_;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
};

}  // namespace media

#endif  // MEDIA_FORMATS_MP4_H26X_ANNEX_B_TO_BITSTREAM_CONVERTER_H_

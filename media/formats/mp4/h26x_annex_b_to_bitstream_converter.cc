// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp4/h26x_annex_b_to_bitstream_converter.h"

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/heap_array.h"
#include "base/notreached.h"

namespace media {

H26xAnnexBToBitstreamConverter::H26xAnnexBToBitstreamConverter(
    VideoCodec video_codec) {
  CHECK(video_codec == VideoCodec::kH264
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
        || video_codec == VideoCodec::kHEVC
#endif
  );
  if (video_codec == VideoCodec::kH264) {
    h264_converter_ = std::make_unique<H264AnnexBToAvcBitstreamConverter>();
  }
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  if (video_codec == VideoCodec::kHEVC) {
    h265_converter_ = std::make_unique<H265AnnexBToHevcBitstreamConverter>();
  }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
}

H26xAnnexBToBitstreamConverter::~H26xAnnexBToBitstreamConverter() = default;

MP4Status H26xAnnexBToBitstreamConverter::ConvertChunk(
    const base::span<const uint8_t> input,
    base::span<uint8_t> output,
    bool* config_changed_out,
    size_t* size_out) {
  if (h264_converter_) {
    return h264_converter_->ConvertChunk(input, output, config_changed_out,
                                         size_out);
  }
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  if (h265_converter_) {
    return h265_converter_->ConvertChunk(input, output, config_changed_out,
                                         size_out);
  }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  NOTREACHED();
}

scoped_refptr<DecoderBuffer> H26xAnnexBToBitstreamConverter::Convert(
    const base::span<const uint8_t> input) {
  bool config_changed = false;
  size_t desired_size = 0;

  auto status = ConvertChunk(input, base::span<uint8_t>(), &config_changed,
                             &desired_size);
  CHECK_EQ(status.code(), MP4Status::Codes::kBufferTooSmall);
  auto output_chunk = base::HeapArray<uint8_t>::WithSize(desired_size);

  status = ConvertChunk(input, output_chunk, &config_changed, &desired_size);
  CHECK(status.is_ok());

  return DecoderBuffer::FromArray(std::move(output_chunk));
}

VideoEncoder::CodecDescription
H26xAnnexBToBitstreamConverter::GetCodecDescription() {
  if (h264_converter_) {
    const auto& config = h264_converter_->GetCurrentConfig();
    VideoEncoder::CodecDescription h264_codec_description;
    if (!config.Serialize(h264_codec_description)) {
      DVLOG(1) << "Failed to get h264 codec description";
    }
    return h264_codec_description;
  }
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  if (h265_converter_) {
    const auto& config = h265_converter_->GetCurrentConfig();
    VideoEncoder::CodecDescription hevc_codec_description;
    if (!config.Serialize(hevc_codec_description)) {
      DVLOG(1) << "Failed to get hevc codec description";
    }
    return hevc_codec_description;
  }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  NOTREACHED();
}

CodecProfileLevel H26xAnnexBToBitstreamConverter::GetCodecProfileLevel() {
  if (h264_converter_) {
    const auto& config = h264_converter_->GetCurrentConfig();
    return {
        VideoCodec::kH264,
        H264Parser::ProfileIDCToVideoCodecProfile(config.profile_indication),
        config.avc_level};
  }
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  if (h265_converter_) {
    const auto& config = h265_converter_->GetCurrentConfig();
    return {
        VideoCodec::kHEVC,
        H265Parser::ProfileIDCToVideoCodecProfile(config.general_profile_idc),
        config.general_level_idc};
  }
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
  NOTREACHED();
}

}  // namespace media

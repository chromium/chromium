// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_decoder_helper.h"

#include "base/containers/span_writer.h"
#include "media/base/media_types.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/filters/h264_to_annex_b_bitstream_converter.h"  // nogncheck
#include "media/formats/mp4/box_definitions.h"                  // nogncheck
#include "media/parsers/h264_parser.h"

#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#include "media/filters/h265_to_annex_b_bitstream_converter.h"  // nogncheck
#include "media/formats/mp4/hevc.h"                             // nogncheck
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)

namespace blink {

// static
std::unique_ptr<VideoDecoderHelper> VideoDecoderHelper::Create(
    media::VideoType video_type,
    base::span<const uint8_t> configuration_record,
    Status* status_out) {
  DCHECK(!configuration_record.empty());
  DCHECK(status_out);
  std::unique_ptr<VideoDecoderHelper> decoder_helper = nullptr;
  if (video_type.codec != media::VideoCodec::kH264 &&
      video_type.codec != media::VideoCodec::kHEVC) {
    *status_out = Status::kUnsupportedCodec;
  } else {
#if !BUILDFLAG(USE_PROPRIETARY_CODECS)
    if (video_type.codec == media::VideoCodec::kH264) {
      *status_out = Status::kUnsupportedCodec;
      return nullptr;
    }
#endif  // !BUILDFLAG(USE_PROPRIETARY_CODECS)
#if !BUILDFLAG(USE_PROPRIETARY_CODECS) || !BUILDFLAG(ENABLE_PLATFORM_HEVC)
    if (video_type.codec == media::VideoCodec::kHEVC) {
      *status_out = Status::kUnsupportedCodec;
      return nullptr;
    }
#endif  // !BUILDFLAG(USE_PROPRIETARY_CODECS) ||
        // !BUILDFLAG(ENABLE_PLATFORM_HEVC)

    decoder_helper = std::make_unique<VideoDecoderHelper>(video_type);
    *status_out = decoder_helper->Initialize(configuration_record);
  }
  if (*status_out != Status::kSucceed) {
    decoder_helper.reset();
    return nullptr;
  } else {
    return decoder_helper;
  }
}

VideoDecoderHelper::VideoDecoderHelper(media::VideoType video_type) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (video_type.codec == media::VideoCodec::kH264) {
    h264_avcc_ = std::make_unique<media::mp4::AVCDecoderConfigurationRecord>();
    h264_converter_ = std::make_unique<media::H264ToAnnexBBitstreamConverter>();
  }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  if (video_type.codec == media::VideoCodec::kHEVC) {
    h265_hvcc_ = std::make_unique<media::mp4::HEVCDecoderConfigurationRecord>();
    h265_converter_ = std::make_unique<media::H265ToAnnexBBitstreamConverter>();
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
}

VideoDecoderHelper::~VideoDecoderHelper() = default;

VideoDecoderHelper::Status VideoDecoderHelper::Initialize(
    base::span<const uint8_t> configuration_record) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  bool initialized = false;
  if (h264_converter_ && h264_avcc_) {
    initialized = h264_converter_->ParseConfiguration(configuration_record,
                                                      h264_avcc_.get());

    // Parsing failures below are non-fatal for historical compliance.
    if (initialized && !h264_avcc_->sps_list.empty()) {
      auto sps_nalu = h264_avcc_->sps_list[0];
      sps_nalu.insert(sps_nalu.begin(), {0u, 0u, 1u});
      media::H264Parser parser;
      parser.SetStream(sps_nalu);
      media::H264NALU nalu;
      if (parser.AdvanceToNextNALU(&nalu) == media::H264Parser::kOk) {
        int sps_id;
        if (parser.ParseSPS(&sps_id) == media::H264Parser::kOk) {
          if (const auto* sps = parser.GetSPS(sps_id)) {
            // Interlaced content isn't supported by any hardware decoders based
            // on media::H264Decoder and is only sometimes supported on Android.
            //
            // Sadly this information is not part of the codec string, nor is it
            // part of the information we get back from the OS support matrix.
            // The best we can do is preemptively detect this type of content
            // for AVC formatted H.264 and route it to FFmpeg. Checking this
            // here also allows us to properly fail isConfigSupported() if the
            // `hardwareAcceleration` preference is `prefer-hardware`.
            //
            // This does not fix the problem for annex-b formatted H.264, which
            // we can't detect until after we've selected the decoder. Given how
            // rare this type of content is, this fix is best effort.
            requires_software_decoder_ = sps->frame_mbs_only_flag == 0;
          }
        }
      }
    }
  }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  else if (h265_converter_ && h265_hvcc_) {
    initialized = h265_converter_->ParseConfiguration(configuration_record,
                                                      h265_hvcc_.get());
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
  if (initialized) {
    return Status::kSucceed;
  } else {
    return Status::kDescriptionParseFailed;
  }
#else
  return Status::kUnsupportedCodec;
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
}

uint32_t VideoDecoderHelper::CalculateNeededOutputBufferSize(
    base::span<const uint8_t> input,
    bool is_first_chunk) const {
  uint32_t output_size = 0;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (h264_converter_ && h264_avcc_) {
    output_size = h264_converter_->CalculateNeededOutputBufferSize(
        input, is_first_chunk ? h264_avcc_.get() : nullptr);
  }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  else if (h265_converter_ && h265_hvcc_) {
    output_size = h265_converter_->CalculateNeededOutputBufferSize(
        input, is_first_chunk ? h265_hvcc_.get() : nullptr);
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  return output_size;
}

VideoDecoderHelper::Status VideoDecoderHelper::ConvertNalUnitStreamToByteStream(
    base::span<const uint8_t> input,
    base::span<uint8_t> output,
    uint32_t* output_size,
    bool is_first_chunk) {
  bool converted = false;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (h264_converter_ && h264_avcc_) {
    converted = h264_converter_->ConvertNalUnitStreamToByteStream(
        input, is_first_chunk ? h264_avcc_.get() : nullptr, output,
        output_size);
  }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  else if (h265_converter_ && h265_hvcc_) {
    base::SpanWriter writer(output);
    converted = h265_converter_->ConvertNalUnitStreamToByteStream(
        input, is_first_chunk ? h265_hvcc_.get() : nullptr, writer);
    *output_size = writer.num_written();
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  return converted ? Status::kSucceed : Status::kBitstreamConvertFailed;
}

}  // namespace blink

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_decoder_helper.h"

#if BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/filters/h264_to_annex_b_bitstream_converter.h"  // nogncheck
#include "media/formats/mp4/box_definitions.h"                  // nogncheck
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
#include "media/filters/h265_to_annex_b_bitstream_converter.h"  // nogncheck
#include "media/formats/mp4/hevc.h"                             // nogncheck
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
#include "media/base/media_types.h"

namespace blink {

// static
std::unique_ptr<VideoDecoderHelper> VideoDecoderHelper::Create(
    media::VideoType video_type,
    const uint8_t* configuration_record,
    int configuration_record_size,
    Status* status_out) {
  DCHECK(configuration_record);
  DCHECK(configuration_record_size);
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
    *status_out = decoder_helper->Initialize(configuration_record,
                                             configuration_record_size);
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
    const uint8_t* configuration_record,
    int configuration_record_size) {
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  bool initialized = false;
  if (h264_converter_ && h264_avcc_) {
    initialized = h264_converter_->ParseConfiguration(
        configuration_record, configuration_record_size, h264_avcc_.get());
  }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  else if (h265_converter_ && h265_hvcc_) {
    initialized = h265_converter_->ParseConfiguration(
        configuration_record, configuration_record_size, h265_hvcc_.get());
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
    const uint8_t* input,
    uint32_t input_size,
    bool is_first_chunk) const {
  uint32_t output_size = 0;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (h264_converter_ && h264_avcc_) {
    output_size = h264_converter_->CalculateNeededOutputBufferSize(
        input, input_size, is_first_chunk ? h264_avcc_.get() : nullptr);
  }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  else if (h265_converter_ && h265_hvcc_) {
    output_size = h265_converter_->CalculateNeededOutputBufferSize(
        input, input_size, is_first_chunk ? h265_hvcc_.get() : nullptr);
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  return output_size;
}

VideoDecoderHelper::Status VideoDecoderHelper::ConvertNalUnitStreamToByteStream(
    const uint8_t* input,
    uint32_t input_size,
    uint8_t* output,
    uint32_t* output_size,
    bool is_first_chunk) {
  bool converted = false;
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  if (h264_converter_ && h264_avcc_) {
    converted = h264_converter_->ConvertNalUnitStreamToByteStream(
        input, input_size, is_first_chunk ? h264_avcc_.get() : nullptr, output,
        output_size);
  }
#if BUILDFLAG(ENABLE_PLATFORM_HEVC)
  else if (h265_converter_ && h265_hvcc_) {
    converted = h265_converter_->ConvertNalUnitStreamToByteStream(
        input, input_size, is_first_chunk ? h265_hvcc_.get() : nullptr, output,
        output_size);
  }
#endif  // BUILDFLAG(ENABLE_PLATFORM_HEVC)
#endif  // BUILDFLAG(USE_PROPRIETARY_CODECS)
  return converted ? Status::kSucceed : Status::kBitstreamConvertFailed;
}

}  // namespace blink

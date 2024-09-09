// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/image_decoder_fuzzer_utils.h"

#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/jpeg/jpeg_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"

namespace blink {

std::unique_ptr<ImageDecoder> CreateImageDecoder(DecoderType decoder_type,
                                                 FuzzedDataProvider& fdp) {
  ImageDecoder::AlphaOption option = fdp.ConsumeBool()
                                         ? ImageDecoder::kAlphaPremultiplied
                                         : ImageDecoder::kAlphaNotPremultiplied;
  int which_color_behavior = fdp.ConsumeIntegralInRange(1, 3);
  ColorBehavior behavior;
  switch (which_color_behavior) {
    case 1:
      behavior = ColorBehavior::kIgnore;
      break;
    case 2:
      behavior = ColorBehavior::kTag;
      break;
    case 3:
      behavior = ColorBehavior::kTransformToSRGB;
      break;
    default:
      behavior = ColorBehavior::kIgnore;
      break;
  }
  wtf_size_t max_decoded_bytes = fdp.ConsumeIntegral<uint32_t>();

  switch (decoder_type) {
    case DecoderType::kBmpDecoder:
      return std::make_unique<BMPImageDecoder>(option, behavior,
                                               max_decoded_bytes);
    case DecoderType::kJpegDecoder: {
      cc::AuxImage aux_image_type =
          fdp.ConsumeBool() ? cc::AuxImage::kDefault : cc::AuxImage::kGainmap;
      wtf_size_t offset = fdp.ConsumeIntegral<uint32_t>();
      return std::make_unique<JPEGImageDecoder>(
          option, behavior, aux_image_type, max_decoded_bytes, offset);
    }
    case DecoderType::kPngDecoder: {
      ImageDecoder::HighBitDepthDecodingOption decoding_option =
          fdp.ConsumeBool() ? ImageDecoder::kDefaultBitDepth
                            : ImageDecoder::kHighBitDepthToHalfFloat;
      wtf_size_t offset = fdp.ConsumeIntegral<uint32_t>();
      return std::make_unique<PNGImageDecoder>(
          option, decoding_option, behavior, max_decoded_bytes, offset);
    }
  }
}

void FuzzDecoder(DecoderType decoder_type, FuzzedDataProvider& fdp) {
  auto decoder = CreateImageDecoder(decoder_type, fdp);
  auto remaining_data = fdp.ConsumeRemainingBytes<char>();
  auto buffer =
      SharedBuffer::Create(remaining_data.data(), remaining_data.size());
  const bool kAllDataReceived = true;
  decoder->SetData(buffer.get(), kAllDataReceived);
  for (wtf_size_t frame = 0; frame < decoder->FrameCount(); ++frame) {
    decoder->DecodeFrameBufferAtIndex(frame);
    if (decoder->Failed()) {
      return;
    }
  }
}

}  // namespace blink

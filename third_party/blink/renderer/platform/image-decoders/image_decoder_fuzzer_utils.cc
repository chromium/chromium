// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/image_decoder_fuzzer_utils.h"

#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/image-decoders/avif/crabbyavif_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/jpeg/jpeg_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"

namespace blink {

ColorBehavior GetColorBehavior(FuzzedDataProvider& fdp) {
  switch (fdp.ConsumeIntegralInRange(1, 3)) {
    case 1:
      return ColorBehavior::kIgnore;
    case 2:
      return ColorBehavior::kTag;
    case 3:
      return ColorBehavior::kTransformToSRGB;
    default:
      return ColorBehavior::kIgnore;
  }
}

ImageDecoder::AnimationOption GetAnimationOption(FuzzedDataProvider& fdp) {
  switch (fdp.ConsumeIntegralInRange(1, 3)) {
    case 1:
      return ImageDecoder::AnimationOption::kUnspecified;
    case 2:
      return ImageDecoder::AnimationOption::kPreferAnimation;
    case 3:
      return ImageDecoder::AnimationOption::kPreferStillImage;
    default:
      return ImageDecoder::AnimationOption::kUnspecified;
  }
}

ImageDecoder::HighBitDepthDecodingOption GetHbdOption(FuzzedDataProvider& fdp) {
  return fdp.ConsumeBool() ? ImageDecoder::kDefaultBitDepth
                           : ImageDecoder::kHighBitDepthToHalfFloat;
}

cc::AuxImage GetAuxImageType(FuzzedDataProvider& fdp) {
  return fdp.ConsumeBool() ? cc::AuxImage::kDefault : cc::AuxImage::kGainmap;
}

ImageDecoder::AlphaOption GetAlphaOption(FuzzedDataProvider& fdp) {
  return fdp.ConsumeBool() ? ImageDecoder::kAlphaPremultiplied
                           : ImageDecoder::kAlphaNotPremultiplied;
}

std::unique_ptr<ImageDecoder> CreateImageDecoder(DecoderType decoder_type,
                                                 FuzzedDataProvider& fdp) {
  switch (decoder_type) {
    case DecoderType::kBmpDecoder:
      return std::make_unique<BMPImageDecoder>(
          GetAlphaOption(fdp), GetColorBehavior(fdp),
          /*max_decoded_bytes=*/fdp.ConsumeIntegral<uint32_t>());
    case DecoderType::kJpegDecoder: {
      return std::make_unique<JPEGImageDecoder>(
          GetAlphaOption(fdp), GetColorBehavior(fdp), GetAuxImageType(fdp),
          /*max_decoded_bytes=*/fdp.ConsumeIntegral<uint32_t>(),
          /*offset=*/fdp.ConsumeIntegral<uint32_t>());
    }
    case DecoderType::kPngDecoder: {
      return std::make_unique<PngImageDecoder>(
          GetAlphaOption(fdp), GetColorBehavior(fdp),
          /*max_decoded_bytes=*/fdp.ConsumeIntegral<uint32_t>(),
          /*offset=*/fdp.ConsumeIntegral<uint32_t>(),
          /*bit_depth_option=*/GetHbdOption(fdp));
    }
    case DecoderType::kCrabbyAvifDecoder: {
      return std::make_unique<CrabbyAVIFImageDecoder>(
          GetAlphaOption(fdp), GetHbdOption(fdp), GetColorBehavior(fdp),
          GetAuxImageType(fdp),
          /*max_decoded_bytes=*/fdp.ConsumeIntegral<uint32_t>(),
          GetAnimationOption(fdp));
    }
  }
}

void FuzzDecoder(DecoderType decoder_type, FuzzedDataProvider& fdp) {
  auto decoder = CreateImageDecoder(decoder_type, fdp);
  auto remaining_data = fdp.ConsumeRemainingBytes<char>();
  auto buffer = SharedBuffer::Create(remaining_data);
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

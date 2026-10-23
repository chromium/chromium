// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/image_decoder_fuzzer_utils.h"

#include <limits>

#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/image-decoders/avif/avif_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/jpeg/jpeg_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"

#if BUILDFLAG(ENABLE_JXL_DECODER)
#include "third_party/blink/renderer/platform/image-decoders/jxl/jxl_image_decoder.h"
#endif

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

// Returns a `uint32_t` value >= kMinMaxDecodedBytes.
uint32_t GetMaxDecodedBytes(FuzzedDataProvider& fdp) {
  // The floor is a heuristic, not a threshold used by ImageDecoder: it is
  // chosen to exceed the decoded frame size of typical seed images so fuzzed
  // input rarely puts the decoder in aggressive purge mode, where cached
  // frames are dropped and later frames are re-decoded from scratch.
  constexpr uint32_t kMinMaxDecodedBytes = 1024 * 1024;
  return fdp.ConsumeIntegralInRange<uint32_t>(
      kMinMaxDecodedBytes, std::numeric_limits<uint32_t>::max());
}

std::unique_ptr<ImageDecoder> CreateImageDecoder(DecoderType decoder_type,
                                                 FuzzedDataProvider& fdp) {
  // Options are read into locals before construction: argument evaluation order
  // is unspecified, so reading them inline would make a repro depend on the
  // compiler's ordering.
  switch (decoder_type) {
    case DecoderType::kBmpDecoder: {
      const ImageDecoder::AlphaOption alpha_option = GetAlphaOption(fdp);
      const ColorBehavior color_behavior = GetColorBehavior(fdp);
      const uint32_t max_decoded_bytes = GetMaxDecodedBytes(fdp);
      return std::make_unique<BMPImageDecoder>(alpha_option, color_behavior,
                                               max_decoded_bytes);
    }
    case DecoderType::kJpegDecoder: {
      const ImageDecoder::AlphaOption alpha_option = GetAlphaOption(fdp);
      const ColorBehavior color_behavior = GetColorBehavior(fdp);
      const cc::AuxImage aux_image = GetAuxImageType(fdp);
      const uint32_t max_decoded_bytes = GetMaxDecodedBytes(fdp);
      return std::make_unique<JPEGImageDecoder>(
          alpha_option, color_behavior, aux_image, max_decoded_bytes);
    }
    case DecoderType::kPngDecoder: {
      const ImageDecoder::AlphaOption alpha_option = GetAlphaOption(fdp);
      const ColorBehavior color_behavior = GetColorBehavior(fdp);
      const uint32_t max_decoded_bytes = GetMaxDecodedBytes(fdp);
      const uint32_t offset = fdp.ConsumeIntegral<uint32_t>();
      const ImageDecoder::HighBitDepthDecodingOption bit_depth_option =
          GetHbdOption(fdp);
      return std::make_unique<PngImageDecoder>(alpha_option, color_behavior,
                                               max_decoded_bytes, offset,
                                               bit_depth_option);
    }
    case DecoderType::kAvifDecoder: {
      const ImageDecoder::AlphaOption alpha_option = GetAlphaOption(fdp);
      const ImageDecoder::HighBitDepthDecodingOption bit_depth_option =
          GetHbdOption(fdp);
      const ColorBehavior color_behavior = GetColorBehavior(fdp);
      const cc::AuxImage aux_image = GetAuxImageType(fdp);
      const uint32_t max_decoded_bytes = GetMaxDecodedBytes(fdp);
      const ImageDecoder::AnimationOption animation_option =
          GetAnimationOption(fdp);
      return std::make_unique<AVIFImageDecoder>(
          alpha_option, bit_depth_option, color_behavior, aux_image,
          max_decoded_bytes, animation_option);
    }
#if BUILDFLAG(ENABLE_JXL_DECODER)
    case DecoderType::kJxlDecoder: {
      const ImageDecoder::AlphaOption alpha_option = GetAlphaOption(fdp);
      const ImageDecoder::HighBitDepthDecodingOption bit_depth_option =
          GetHbdOption(fdp);
      const ColorBehavior color_behavior = GetColorBehavior(fdp);
      const cc::AuxImage aux_image = GetAuxImageType(fdp);
      const uint32_t max_decoded_bytes = GetMaxDecodedBytes(fdp);
      const ImageDecoder::AnimationOption animation_option =
          GetAnimationOption(fdp);
      return std::make_unique<JXLImageDecoder>(
          alpha_option, bit_depth_option, color_behavior, aux_image,
          max_decoded_bytes, animation_option);
    }
#endif
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

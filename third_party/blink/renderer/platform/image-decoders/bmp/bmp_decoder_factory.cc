// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_decoder_factory.h"

#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_features.h"
#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/bmp/bmp_rust_image_decoder.h"

namespace blink {

std::unique_ptr<ImageDecoder> CreateBmpImageDecoder(
    ImageDecoder::AlphaOption alpha_option,
    ImageDecoder::HighBitDepthDecodingOption high_bit_depth_decoding_option,
    ColorBehavior color_behavior,
    wtf_size_t max_decoded_bytes,
    wtf_size_t offset) {
  if (IsRustyBmpEnabled()) {
    return std::make_unique<BmpRustImageDecoder>(
        alpha_option, color_behavior, max_decoded_bytes, offset,
        high_bit_depth_decoding_option);
  }

  return std::make_unique<BMPImageDecoder>(alpha_option, color_behavior,
                                           max_decoded_bytes);
}

}  // namespace blink

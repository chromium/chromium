// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/png/png_decoder_factory.h"

#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"

namespace blink {

std::unique_ptr<ImageDecoder> CreatePngImageDecoder(
    ImageDecoder::AlphaOption alpha_option,
    ImageDecoder::HighBitDepthDecodingOption high_bit_depth_decoding_option,
    ColorBehavior color_behavior,
    wtf_size_t max_decoded_bytes,
    wtf_size_t offset) {
  // TODO(https://crbug.com/356884491): Create Rust-based decoder when the
  // appropriate build flags and runtime features are enabled.
  return std::make_unique<PNGImageDecoder>(
      alpha_option, high_bit_depth_decoding_option, color_behavior,
      max_decoded_bytes, offset);
}

}  // namespace blink

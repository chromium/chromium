// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/png/png_decoder_factory.h"

#include "skia/buildflags.h"
#include "skia/rusty_png_feature.h"
#include "third_party/blink/renderer/platform/image-decoders/png/png_image_decoder.h"

#if BUILDFLAG(SKIA_BUILD_RUST_PNG)
#include "third_party/blink/renderer/platform/image-decoders/png/skia_png_rust_image_decoder.h"
#endif

namespace blink {

std::unique_ptr<ImageDecoder> CreatePngImageDecoder(
    ImageDecoder::AlphaOption alpha_option,
    ImageDecoder::HighBitDepthDecodingOption high_bit_depth_decoding_option,
    ColorBehavior color_behavior,
    wtf_size_t max_decoded_bytes,
    wtf_size_t offset) {
  if (skia::IsRustyPngEnabled()) {
#if BUILDFLAG(SKIA_BUILD_RUST_PNG)
    // TODO(https://crbug.com/359350061): Stop ignoring
    // `high_bit_depth_decoding_option`.
    return std::make_unique<SkiaPngRustImageDecoder>(
        alpha_option, color_behavior, max_decoded_bytes, offset);
#else
    NOTREACHED();  // The `if` condition guarantees `ENABLE_RUST_PNG`.
#endif
  }

  return std::make_unique<PNGImageDecoder>(
      alpha_option, high_bit_depth_decoding_option, color_behavior,
      max_decoded_bytes, offset);
}

}  // namespace blink

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/jpeg/jpeg_decoder_factory.h"

#include "skia/rusty_jpeg_feature.h"
#include "third_party/blink/renderer/platform/image-decoders/jpeg/jpeg_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/jpeg/jpeg_rust_image_decoder.h"

namespace blink {

std::unique_ptr<ImageDecoder> CreateJpegImageDecoder(
    ImageDecoder::AlphaOption alpha_option,
    ColorBehavior color_behavior,
    cc::AuxImage aux_image,
    wtf_size_t max_decoded_bytes,
    const gfx::Size& desired_size) {
  // TODO(crbug.com/493315750): The Skia-backed decoder does not yet support
  // budget-driven downsampling or desired_size.
  if (skia::IsRustyJpegEnabled() && aux_image == cc::AuxImage::kDefault &&
      max_decoded_bytes == ImageDecoder::kNoDecodedImageByteLimit &&
      desired_size.IsEmpty()) {
    return std::make_unique<JpegRustImageDecoder>(alpha_option, color_behavior,
                                                  max_decoded_bytes);
  }

  return std::make_unique<JPEGImageDecoder>(
      alpha_option, color_behavior, aux_image, max_decoded_bytes, desired_size);
}

}  // namespace blink

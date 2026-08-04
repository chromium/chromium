// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/ico/ico_decoder_factory.h"

#include "base/feature_list.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/image-decoders/ico/ico_image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/ico/ico_rust_image_decoder.h"

namespace blink {

std::unique_ptr<ImageDecoder> CreateIcoImageDecoder(
    ImageDecoder::AlphaOption alpha_option,
    ImageDecoder::HighBitDepthDecodingOption high_bit_depth_decoding_option,
    ColorBehavior color_behavior,
    wtf_size_t max_decoded_bytes) {
  if (base::FeatureList::IsEnabled(features::kRustyIcoFeature)) {
    return std::make_unique<IcoRustImageDecoder>(
        alpha_option, color_behavior, max_decoded_bytes,
        IcoRustImageDecoder::kNoReadingOffset, high_bit_depth_decoding_option);
  }

  return std::make_unique<ICOImageDecoder>(alpha_option, color_behavior,
                                           max_decoded_bytes);
}

}  // namespace blink

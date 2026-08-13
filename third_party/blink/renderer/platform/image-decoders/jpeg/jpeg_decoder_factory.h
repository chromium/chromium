// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JPEG_JPEG_DECODER_FACTORY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JPEG_JPEG_DECODER_FACTORY_H_

#include <memory>

#include "cc/paint/paint_image.h"
#include "third_party/blink/renderer/platform/graphics/color_behavior.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

PLATFORM_EXPORT std::unique_ptr<ImageDecoder> CreateJpegImageDecoder(
    ImageDecoder::AlphaOption alpha_option,
    ColorBehavior color_behavior,
    cc::AuxImage aux_image,
    wtf_size_t max_decoded_bytes);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JPEG_JPEG_DECODER_FACTORY_H_

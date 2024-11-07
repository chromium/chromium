// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/image-decoders/png/skia_png_rust_image_decoder.h"

#include "third_party/skia/experimental/rust_png/SkPngRustDecoder.h"
#include "third_party/skia/include/core/SkStream.h"

namespace blink {

SkiaPngRustImageDecoder::SkiaPngRustImageDecoder(AlphaOption alpha_option,
                                                 ColorBehavior color_behavior,
                                                 wtf_size_t max_decoded_bytes,
                                                 wtf_size_t offset)
    : SkiaImageDecoderBase(alpha_option,
                           color_behavior,
                           max_decoded_bytes,
                           offset) {}

SkiaPngRustImageDecoder::~SkiaPngRustImageDecoder() = default;

String SkiaPngRustImageDecoder::FilenameExtension() const {
  return "png";
}

const AtomicString& SkiaPngRustImageDecoder::MimeType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, png_mime_type, ("image/png"));
  return png_mime_type;
}

std::unique_ptr<SkCodec> SkiaPngRustImageDecoder::OnCreateSkCodec(
    std::unique_ptr<SkStream> stream,
    SkCodec::Result* result) {
  std::unique_ptr<SkCodec> codec =
      SkPngRustDecoder::Decode(std::move(stream), result);
  return codec;
}

}  // namespace blink

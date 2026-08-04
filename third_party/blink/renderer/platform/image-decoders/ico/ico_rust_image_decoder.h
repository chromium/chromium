// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_ICO_ICO_RUST_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_ICO_ICO_RUST_IMAGE_DECODER_H_

#include "third_party/blink/renderer/platform/image-decoders/skia/skia_image_decoder_base.h"

namespace blink {

// This class decodes the ICO and CUR image formats using `SkIcoRustCodec`.
class PLATFORM_EXPORT IcoRustImageDecoder final : public SkiaImageDecoderBase {
 public:
  // Exposing the same constructor as the base class:
  using SkiaImageDecoderBase::SkiaImageDecoderBase;

  IcoRustImageDecoder(const IcoRustImageDecoder&) = delete;
  IcoRustImageDecoder& operator=(const IcoRustImageDecoder&) = delete;
  ~IcoRustImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override;
  const AtomicString& MimeType() const override;

 protected:
  // SkiaImageDecoderBase:
  std::unique_ptr<SkCodec> OnCreateSkCodec(std::unique_ptr<SkStream>,
                                           SkCodec::Result* result) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_ICO_ICO_RUST_IMAGE_DECODER_H_

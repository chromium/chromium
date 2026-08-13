// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JPEG_JPEG_RUST_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JPEG_JPEG_RUST_IMAGE_DECODER_H_

#include "third_party/blink/renderer/platform/image-decoders/skia/skia_image_decoder_base.h"

namespace blink {

// Decodes JPEG images using `SkJpegRustCodec` (backed by zune-jpeg).
class PLATFORM_EXPORT JpegRustImageDecoder final : public SkiaImageDecoderBase {
 public:
  // Exposing the same constructor as the base class:
  using SkiaImageDecoderBase::SkiaImageDecoderBase;

  JpegRustImageDecoder(const JpegRustImageDecoder&) = delete;
  JpegRustImageDecoder& operator=(const JpegRustImageDecoder&) = delete;
  ~JpegRustImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override;
  const AtomicString& MimeType() const override;
  bool GetGainmapInfoAndData(
      SkGainmapInfo& out_gainmap_info,
      scoped_refptr<SegmentReader>& out_gainmap_data) const override;

 protected:
  // SkiaImageDecoderBase:
  std::unique_ptr<SkCodec> OnCreateSkCodec(std::unique_ptr<SkStream>,
                                           SkCodec::Result* result) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JPEG_JPEG_RUST_IMAGE_DECODER_H_

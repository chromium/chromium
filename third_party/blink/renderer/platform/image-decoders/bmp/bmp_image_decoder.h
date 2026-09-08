// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_BMP_BMP_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_BMP_BMP_IMAGE_DECODER_H_

#include <memory>

#include "third_party/blink/renderer/platform/image-decoders/skia/skia_image_decoder_base.h"
#include "third_party/skia/include/core/SkStream.h"

namespace blink {

// This class decodes the BMP image format using `SkBmpRustCodec`.
class PLATFORM_EXPORT BMPImageDecoder final : public SkiaImageDecoderBase {
 public:
  using SkiaImageDecoderBase::SkiaImageDecoderBase;

  BMPImageDecoder(const BMPImageDecoder&) = delete;
  BMPImageDecoder& operator=(const BMPImageDecoder&) = delete;
  ~BMPImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override;
  const AtomicString& MimeType() const override;

 protected:
  // SkiaImageDecoderBase:
  std::unique_ptr<SkCodec> OnCreateSkCodec(std::unique_ptr<SkStream>,
                                           SkCodec::Result* result) override;

 private:
  // In the complete-data path, Skia decodes from SkData instead of the
  // SegmentStream created by SkiaImageDecoderBase. Keep that stream alive
  // because the base class stores a raw pointer to it for future SetData calls.
  std::unique_ptr<SkStream> complete_data_stream_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_BMP_BMP_IMAGE_DECODER_H_

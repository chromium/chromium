// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JPEG_JPEG_RUST_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JPEG_JPEG_RUST_IMAGE_DECODER_H_

#include "third_party/blink/renderer/platform/image-decoders/skia/skia_image_decoder_base.h"
#include "third_party/skia/include/core/SkYUVAPixmaps.h"

namespace blink {

// Decodes main and gainmap JPEG images using `SkJpegRustCodec` (zune-jpeg).
class PLATFORM_EXPORT JpegRustImageDecoder final : public SkiaImageDecoderBase {
 public:
  JpegRustImageDecoder(AlphaOption,
                       ColorBehavior,
                       cc::AuxImage,
                       wtf_size_t max_decoded_bytes,
                       const gfx::Size& desired_size = gfx::Size());

  JpegRustImageDecoder(const JpegRustImageDecoder&) = delete;
  JpegRustImageDecoder& operator=(const JpegRustImageDecoder&) = delete;
  ~JpegRustImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override;
  const AtomicString& MimeType() const override;
  void OnSetData(scoped_refptr<SegmentReader> data) override;
  cc::YUVSubsampling GetYUVSubsampling() const override;
  gfx::Size DecodedYUVSize(cc::YUVIndex) const override;
  wtf_size_t DecodedYUVWidthBytes(cc::YUVIndex) const override;
  void DecodeToYUV() override;
  SkYUVColorSpace GetYUVColorSpace() const override;
  gfx::Size DecodedSize() const override;
  Vector<SkISize> GetSupportedDecodeSizes() const override;
  bool GetGainmapInfoAndData(
      SkGainmapInfo& out_gainmap_info,
      scoped_refptr<SegmentReader>& out_gainmap_data) const override;

 protected:
  // SkiaImageDecoderBase:
  std::unique_ptr<SkCodec> OnCreateSkCodec(std::unique_ptr<SkStream>,
                                           SkCodec::Result* result) override;
  void OnSkCodecCreated() override;
  SkCodec::Result OnIncrementalDecode(wtf_size_t) override;

 private:
  SkYUVAPixmapInfo yuva_pixmap_info_;
  scoped_refptr<SegmentReader> gainmap_data_;
  Vector<SkISize> supported_decode_sizes_;
  gfx::Size decoded_size_;
  gfx::Size source_mcu_size_;
  // Input state at codec creation; not updated by later SetData() calls.
  bool codec_input_complete_ = false;
  bool retried_complete_input_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JPEG_JPEG_RUST_IMAGE_DECODER_H_

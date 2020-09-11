/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JPEG_JPEG_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_JPEG_JPEG_IMAGE_DECODER_H_

#include <memory>

#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"

namespace blink {

class JPEGImageReader;

class PLATFORM_EXPORT JPEGImageDecoder final : public ImageDecoder {
 public:
  JPEGImageDecoder(AlphaOption,
                   const ColorBehavior&,
                   size_t max_decoded_bytes,
                   const OverrideAllowDecodeToYuv allow_decode_to_yuv,
                   size_t offset = 0);
  ~JPEGImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override { return "jpg"; }
  void OnSetData(SegmentReader* data) override;
  IntSize DecodedSize() const override { return decoded_size_; }
  bool SetSize(unsigned width, unsigned height) override;
  cc::YUVSubsampling GetYUVSubsampling() const override;
  IntSize DecodedYUVSize(cc::YUVIndex) const override;
  size_t DecodedYUVWidthBytes(cc::YUVIndex) const override;
  void DecodeToYUV() override;
  SkYUVColorSpace GetYUVColorSpace() const override;
  Vector<SkISize> GetSupportedDecodeSizes() const override;

  bool HasImagePlanes() const { return image_planes_.get(); }

  bool OutputScanlines();
  unsigned DesiredScaleNumerator() const;
  bool ShouldGenerateAllSizes() const;
  void Complete();

  void SetOrientation(ImageOrientation orientation) {
    orientation_ = orientation;
  }

  void SetDensityCorrectedSize(const IntSize& size) {
    density_corrected_size_ = size;
  }
  void SetDecodedSize(unsigned width, unsigned height);

  void SetSupportedDecodeSizes(Vector<SkISize> sizes);

  // TODO(crbug.com/919627): |allow_decode_to_yuv_| is false by
  // default and is only set true for unit tests. Remove it once
  // JPEG YUV decoding is finished and YUV decoding is enabled by default.
  void SetDecodeToYuvForTesting(bool decode_to_yuv) {
    allow_decode_to_yuv_ = decode_to_yuv;
  }

 private:
  // ImageDecoder:
  void DecodeSize() override { Decode(true); }
  void Decode(size_t) override { Decode(false); }
  cc::ImageHeaderMetadata MakeMetadataForDecodeAcceleration() const override;

  // Attempts to calculate the coded size of the JPEG image. Returns a zero
  // initialized gfx::Size upon failure.
  gfx::Size GetImageCodedSize() const;

  // Decodes the image.  If |only_size| is true, stops decoding after
  // calculating the image size.  If decoding fails but there is no more
  // data coming, sets the "decode failure" flag.
  void Decode(bool only_size);

  std::unique_ptr<JPEGImageReader> reader_;
  const size_t offset_;
  IntSize decoded_size_;
  Vector<SkISize> supported_decode_sizes_;

  DISALLOW_COPY_AND_ASSIGN(JPEGImageDecoder);
};

}  // namespace blink

#endif

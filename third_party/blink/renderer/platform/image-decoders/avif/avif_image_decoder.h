// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_AVIF_AVIF_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_AVIF_AVIF_IMAGE_DECODER_H_

#include <memory>

#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkData.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/color_transform.h"

struct avifDecoder;
struct avifImage;

namespace blink {

class FastSharedBufferReader;

class PLATFORM_EXPORT AVIFImageDecoder final : public ImageDecoder {
 public:
  AVIFImageDecoder(AlphaOption,
                   HighBitDepthDecodingOption,
                   const ColorBehavior&,
                   size_t max_decoded_bytes,
                   AnimationOption);
  AVIFImageDecoder(const AVIFImageDecoder&) = delete;
  AVIFImageDecoder& operator=(const AVIFImageDecoder&) = delete;
  ~AVIFImageDecoder() override;

  // ImageDecoder:
  String FilenameExtension() const override { return "avif"; }
  bool ImageIsHighBitDepth() override;
  void OnSetData(SegmentReader* data) override;
  cc::YUVSubsampling GetYUVSubsampling() const override;
  IntSize DecodedYUVSize(cc::YUVIndex) const override;
  size_t DecodedYUVWidthBytes(cc::YUVIndex) const override;
  SkYUVColorSpace GetYUVColorSpace() const override;
  uint8_t GetYUVBitDepth() const override;
  void DecodeToYUV() override;
  int RepetitionCount() const override;
  base::TimeDelta FrameDurationAtIndex(size_t) const override;
  bool ImageHasBothStillAndAnimatedSubImages() const override;

  // Returns true if the data in fast_reader begins with a valid FileTypeBox
  // (ftyp) that supports the brand 'avif' or 'avis'.
  static bool MatchesAVIFSignature(const FastSharedBufferReader& fast_reader);

  gfx::ColorTransform* GetColorTransformForTesting();

 private:
  // ImageDecoder:
  void DecodeSize() override;
  size_t DecodeFrameCount() override;
  void InitializeNewFrame(size_t) override;
  void Decode(size_t) override;
  bool CanReusePreviousFrameBuffer(size_t) const override;

  // Creates |decoder_| and decodes the size and frame count.
  bool MaybeCreateDemuxer();

  // Decodes the frame at index |index|. The decoded frame is available in
  // decoder_->image. Returns whether decoding completed successfully.
  bool DecodeImage(size_t index);

  // Updates or creates |color_transform_| for YUV-to-RGB conversion.
  void UpdateColorTransform(const gfx::ColorSpace& frame_cs, int bit_depth);

  // Renders |image| in |buffer|. Returns whether |image| was rendered
  // successfully.
  bool RenderImage(const avifImage* image, ImageFrame* buffer);

  // Applies color profile correction to the pixel data for |buffer|, if
  // desired.
  void ColorCorrectImage(ImageFrame* buffer);

  // The bit depth from the container.
  uint8_t bit_depth_ = 0;
  bool decode_to_half_float_ = false;
  // The YUV format from the container. Stores an avifPixelFormat enum value.
  // Declared as uint8_t because we can't forward-declare an enum type in C++.
  uint8_t avif_yuv_format_ = 0;  // AVIF_PIXEL_FORMAT_NONE
  uint8_t chroma_shift_x_ = 0;
  uint8_t chroma_shift_y_ = 0;
  size_t decoded_frame_count_ = 0;
  SkYUVColorSpace yuv_color_space_ = SkYUVColorSpace::kIdentity_SkYUVColorSpace;
  std::unique_ptr<avifDecoder, void (*)(avifDecoder*)> decoder_{nullptr,
                                                                nullptr};

  std::unique_ptr<gfx::ColorTransform> color_transform_;

  const AnimationOption animation_option_;

  sk_sp<SkData> image_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_IMAGE_DECODERS_AVIF_AVIF_IMAGE_DECODER_H_

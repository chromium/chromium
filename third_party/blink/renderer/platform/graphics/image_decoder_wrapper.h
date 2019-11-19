// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_IMAGE_DECODER_WRAPPER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_IMAGE_DECODER_WRAPPER_H_

#include "cc/paint/paint_image.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/skia/include/core/SkSize.h"

namespace blink {
class ImageDecoderFactory;
class ImageFrameGenerator;
class SegmentReader;

class ImageDecoderWrapper {
  STACK_ALLOCATED();

 public:
  ImageDecoderWrapper(ImageFrameGenerator* generator,
                      SegmentReader* data,
                      const SkISize& scaled_size,
                      ImageDecoder::AlphaOption alpha_option,
                      ColorBehavior decoder_color_behavior,
                      ImageDecoder::HighBitDepthDecodingOption decoding_option,
                      size_t index,
                      const SkImageInfo& info,
                      void* pixels,
                      size_t row_bytes,
                      bool all_data_received,
                      cc::PaintImage::GeneratorClientId client_id);
  ~ImageDecoderWrapper();

  // Returns true if the decode succeeded.
  bool Decode(ImageDecoderFactory* factory,
              size_t* frame_count,
              bool* has_alpha);

  // Indicates that the decode failed due to a corrupt image.
  bool decode_failed() const { return decode_failed_; }

 private:
  bool ShouldDecodeToExternalMemory(size_t frame_count,
                                    bool has_cached_decoder) const;
  bool ShouldRemoveDecoder(bool frame_was_completely_decoded,
                           bool decoded_to_external_memory) const;
  void PurgeAllFramesIfNecessary(ImageDecoder* decoder,
                                 bool frame_was_completely_decoded,
                                 size_t frame_count) const;
  std::unique_ptr<ImageDecoder> CreateDecoderWithData(
      ImageDecoderFactory* factory) const;

  const ImageFrameGenerator* const generator_;
  SegmentReader* data_;
  const SkISize scaled_size_;
  const ImageDecoder::AlphaOption alpha_option_;
  const ColorBehavior decoder_color_behavior_;
  const ImageDecoder::HighBitDepthDecodingOption decoding_option_;
  const size_t frame_index_;
  const SkImageInfo info_;
  void* pixels_;
  const size_t row_bytes_;
  const bool all_data_received_;
  const cc::PaintImage::GeneratorClientId client_id_;

  bool decode_failed_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_IMAGE_DECODER_WRAPPER_H_

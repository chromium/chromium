/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_IMAGE_FRAME_GENERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_IMAGE_FRAME_GENERATOR_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "cc/paint/paint_image.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/image-decoders/segment_reader.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/threading_primitives.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkSize.h"
#include "third_party/skia/include/core/SkTypes.h"
#include "third_party/skia/include/core/SkYUVASizeInfo.h"

namespace blink {

class ImageDecoder;

class PLATFORM_EXPORT ImageDecoderFactory {
  USING_FAST_MALLOC(ImageDecoderFactory);

 public:
  ImageDecoderFactory() = default;
  virtual ~ImageDecoderFactory() = default;
  virtual std::unique_ptr<ImageDecoder> Create() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageDecoderFactory);
};

class PLATFORM_EXPORT ImageFrameGenerator final
    : public ThreadSafeRefCounted<ImageFrameGenerator> {
 public:
  static scoped_refptr<ImageFrameGenerator> Create(
      const SkISize& full_size,
      bool is_multi_frame,
      const ColorBehavior& color_behavior,
      Vector<SkISize> supported_sizes) {
    return base::AdoptRef(new ImageFrameGenerator(
        full_size, is_multi_frame, color_behavior, std::move(supported_sizes)));
  }

  ~ImageFrameGenerator();

  // Decodes and scales the specified frame at |index|. The dimensions and
  // output format are given in SkImageInfo. Decoded pixels are written into
  // |pixels| with a stride of |rowBytes|. Returns true if decoding was
  // successful.
  bool DecodeAndScale(SegmentReader*,
                      bool all_data_received,
                      size_t index,
                      const SkImageInfo&,
                      void* pixels,
                      size_t row_bytes,
                      ImageDecoder::AlphaOption,
                      cc::PaintImage::GeneratorClientId);

  // Decodes YUV components directly into the provided memory planes. Must not
  // be called unless GetYUVComponentSizes has been called and returned true.
  // TODO(crbug.com/943519): In order to support incremental YUV decoding,
  // ImageDecoder needs something analogous to its ImageFrame cache to hold
  // partial planes, and the GPU code needs to handle them.
  bool DecodeToYUV(SegmentReader*,
                   size_t index,
                   const SkISize component_sizes[3],
                   void* planes[3],
                   const size_t row_bytes[3]);

  const SkISize& GetFullSize() const { return full_size_; }

  SkISize GetSupportedDecodeSize(const SkISize& requested_size) const;

  bool IsMultiFrame() const { return is_multi_frame_; }
  bool DecodeFailed() const {
    MutexLocker lock(generator_mutex_);
    return decode_failed_;
  }

  bool HasAlpha(size_t index);

  // TODO(crbug.com/943519): Do not call unless the SkROBuffer has all the data.
  bool GetYUVComponentSizes(SegmentReader*, SkYUVASizeInfo*, SkYUVColorSpace*);

 private:
  class ClientMutexLocker {
    STACK_ALLOCATED();

   public:
    ClientMutexLocker(ImageFrameGenerator* generator,
                      cc::PaintImage::GeneratorClientId client_id);
    ~ClientMutexLocker();

   private:
    ImageFrameGenerator* generator_;
    cc::PaintImage::GeneratorClientId client_id_;
    Mutex* mutex_;
  };

  ImageFrameGenerator(const SkISize& full_size,
                      bool is_multi_frame,
                      const ColorBehavior&,
                      Vector<SkISize> supported_sizes);

  friend class ImageFrameGeneratorTest;
  friend class DeferredImageDecoderTest;
  // For testing. |factory| will overwrite the default ImageDecoder creation
  // logic if |factory->create()| returns non-zero.
  void SetImageDecoderFactory(std::unique_ptr<ImageDecoderFactory> factory) {
    image_decoder_factory_ = std::move(factory);
  }

  void SetHasAlpha(size_t index, bool has_alpha);

  const SkISize full_size_;
  // Parameters used to create internal ImageDecoder objects.
  const ColorBehavior decoder_color_behavior_;
  const bool is_multi_frame_;
  const Vector<SkISize> supported_sizes_;

  mutable Mutex generator_mutex_;
  bool decode_failed_ GUARDED_BY(generator_mutex_) = false;
  bool yuv_decoding_failed_ GUARDED_BY(generator_mutex_) = false;
  size_t frame_count_ GUARDED_BY(generator_mutex_) = 0u;
  Vector<bool> has_alpha_ GUARDED_BY(generator_mutex_);

  struct ClientMutex {
    int ref_count = 0;
    Mutex mutex;
  };

  // Note that it is necessary to use HashMap here to ensure that references
  // to entries in the map, stored in ClientMutexLocker, remain valid across
  // insertions into the map.
  HashMap<cc::PaintImage::GeneratorClientId,
          std::unique_ptr<ClientMutex>,
          WTF::IntHash<cc::PaintImage::GeneratorClientId>,
          WTF::UnsignedWithZeroKeyHashTraits<cc::PaintImage::GeneratorClientId>>
      mutex_map_ GUARDED_BY(generator_mutex_);

  std::unique_ptr<ImageDecoderFactory> image_decoder_factory_;

  DISALLOW_COPY_AND_ASSIGN(ImageFrameGenerator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_IMAGE_FRAME_GENERATOR_H_

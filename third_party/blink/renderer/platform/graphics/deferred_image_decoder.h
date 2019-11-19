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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DEFERRED_IMAGE_DECODER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_DEFERRED_IMAGE_DECODER_H_

#include <memory>

#include "base/macros.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/image-decoders/image_decoder.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/skia/include/core/SkRWBuffer.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class ImageFrameGenerator;
struct DeferredFrameData;

class PLATFORM_EXPORT DeferredImageDecoder final {
  USING_FAST_MALLOC(DeferredImageDecoder);

 public:
  static std::unique_ptr<DeferredImageDecoder> Create(
      scoped_refptr<SharedBuffer> data,
      bool data_complete,
      ImageDecoder::AlphaOption,
      const ColorBehavior&);

  static std::unique_ptr<DeferredImageDecoder> CreateForTesting(
      std::unique_ptr<ImageDecoder>);

  ~DeferredImageDecoder();

  String FilenameExtension() const;

  sk_sp<PaintImageGenerator> CreateGenerator(size_t index);

  scoped_refptr<SharedBuffer> Data();
  void SetData(scoped_refptr<SharedBuffer> data, bool all_data_received);

  bool IsSizeAvailable();
  bool HasEmbeddedColorProfile() const;
  IntSize Size() const;
  IntSize FrameSizeAtIndex(size_t index) const;
  size_t FrameCount();
  bool ImageIsHighBitDepth() const { return image_is_high_bit_depth_; }
  int RepetitionCount() const;
  bool FrameHasAlphaAtIndex(size_t index) const;
  bool FrameIsReceivedAtIndex(size_t index) const;
  base::TimeDelta FrameDurationAtIndex(size_t index) const;
  ImageOrientation OrientationAtIndex(size_t index) const;
  bool HotSpot(IntPoint&) const;

  // A less expensive method for getting the number of bytes thus far received
  // for the image. Checking Data()->size() involves copying bytes to a
  // SharedBuffer.
  //
  // Returns 0 if the read-write buffer has not been initialized or received
  // data.
  size_t ByteSize() const;

 private:
  explicit DeferredImageDecoder(std::unique_ptr<ImageDecoder> metadata_decoder);

  friend class DeferredImageDecoderTest;
  ImageFrameGenerator* FrameGenerator() { return frame_generator_.get(); }

  void ActivateLazyDecoding();
  void PrepareLazyDecodedFrames();

  void SetDataInternal(scoped_refptr<SharedBuffer> data,
                       bool all_data_received,
                       bool push_data_to_decoder);

  // Copy of the data that is passed in, used by deferred decoding.
  // Allows creating readonly snapshots that may be read in another thread.
  std::unique_ptr<SkRWBuffer> rw_buffer_;
  std::unique_ptr<ImageDecoder> metadata_decoder_;

  String filename_extension_;
  IntSize size_;
  int repetition_count_;
  bool has_embedded_color_profile_ = false;
  bool all_data_received_;
  bool first_decoding_generator_created_;
  bool can_yuv_decode_;
  bool has_hot_spot_;
  bool image_is_high_bit_depth_;
  sk_sp<SkColorSpace> color_space_for_sk_images_;
  IntPoint hot_spot_;
  const PaintImage::ContentId complete_frame_content_id_;
  base::Optional<bool> incremental_decode_needed_;

  // Caches an image's metadata so it can outlive |metadata_decoder_| after all
  // data is received in cases where multiple generators are created.
  base::Optional<cc::ImageHeaderMetadata> image_metadata_;

  // Caches frame state information.
  Vector<DeferredFrameData> frame_data_;
  scoped_refptr<ImageFrameGenerator> frame_generator_;

  DISALLOW_COPY_AND_ASSIGN(DeferredImageDecoder);
};

}  // namespace blink

#endif

/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CROSSFADE_GENERATED_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_CROSSFADE_GENERATED_IMAGE_H_

#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/generated_image.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/image_observer.h"

namespace blink {

class PLATFORM_EXPORT CrossfadeGeneratedImage final : public GeneratedImage {
 public:
  static scoped_refptr<CrossfadeGeneratedImage> Create(
      scoped_refptr<Image> from_image,
      scoped_refptr<Image> to_image,
      float percentage,
      FloatSize crossfade_size,
      const FloatSize& size) {
    return base::AdoptRef(
        new CrossfadeGeneratedImage(std::move(from_image), std::move(to_image),
                                    percentage, crossfade_size, size));
  }

  bool HasIntrinsicSize() const override { return true; }

  IntSize Size() const override { return FlooredIntSize(crossfade_size_); }

 protected:
  void Draw(cc::PaintCanvas*,
            const cc::PaintFlags&,
            const FloatRect&,
            const FloatRect&,
            const SkSamplingOptions&,
            RespectImageOrientationEnum,
            ImageClampingMode,
            ImageDecodingMode) override;
  void DrawTile(GraphicsContext&,
                const FloatRect&,
                RespectImageOrientationEnum) final;

  CrossfadeGeneratedImage(scoped_refptr<Image> from_image,
                          scoped_refptr<Image> to_image,
                          float percentage,
                          FloatSize crossfade_size,
                          const FloatSize&);

 private:
  void DrawCrossfade(cc::PaintCanvas*,
                     const SkSamplingOptions&,
                     const cc::PaintFlags&,
                     RespectImageOrientationEnum,
                     ImageClampingMode,
                     ImageDecodingMode);

  scoped_refptr<Image> from_image_;
  scoped_refptr<Image> to_image_;

  float percentage_;
  FloatSize crossfade_size_;
};

}  // namespace blink

#endif

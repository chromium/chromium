// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_GENERATED_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_GENERATED_IMAGE_H_

#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/generated_image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class PLATFORM_EXPORT PaintGeneratedImage : public GeneratedImage {
 public:
  static scoped_refptr<PaintGeneratedImage> Create(sk_sp<PaintRecord> record,
                                                   const FloatSize& size) {
    return base::AdoptRef(new PaintGeneratedImage(std::move(record), size));
  }
  ~PaintGeneratedImage() override = default;

 protected:
  void Draw(cc::PaintCanvas*,
            const cc::PaintFlags&,
            const FloatRect&,
            const FloatRect&,
            const ImageDrawOptions& draw_options,
            ImageClampingMode,
            ImageDecodingMode) override;
  void DrawTile(GraphicsContext&,
                const FloatRect&,
                RespectImageOrientationEnum) final;

  PaintGeneratedImage(sk_sp<PaintRecord> record, const FloatSize& size)
      : GeneratedImage(size), record_(std::move(record)) {}

  sk_sp<PaintRecord> record_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_GENERATED_IMAGE_H_

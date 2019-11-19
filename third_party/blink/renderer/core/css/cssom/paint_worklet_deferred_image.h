// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_PAINT_WORKLET_DEFERRED_IMAGE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_PAINT_WORKLET_DEFERRED_IMAGE_H_

#include "third_party/blink/renderer/core/css/cssom/paint_worklet_input.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/graphics/generated_image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

// Stores the data necessary to draw a CSS Paint[0] specified image, when using
// Off-Thread Paint Worklet.
//
// With Off-Thread PaintWorklet, the actual creation of the PaintRecord is
// deferred until cc-Raster time. This class just holds the input arguments for
// the PaintWorklet, which are then stored in the cc::PaintCanvas when 'drawn'.
//
// https://drafts.css-houdini.org/css-paint-api-1/
class CORE_EXPORT PaintWorkletDeferredImage : public GeneratedImage {
 public:
  static scoped_refptr<PaintWorkletDeferredImage> Create(
      scoped_refptr<PaintWorkletInput> input,
      const FloatSize& size) {
    return base::AdoptRef(new PaintWorkletDeferredImage(input, size));
  }
  ~PaintWorkletDeferredImage() override = default;

 protected:
  void Draw(cc::PaintCanvas*,
            const cc::PaintFlags&,
            const FloatRect&,
            const FloatRect&,
            RespectImageOrientationEnum,
            ImageClampingMode,
            ImageDecodingMode) override;
  void DrawTile(GraphicsContext&, const FloatRect&) override;
  sk_sp<cc::PaintShader> CreateShader(const FloatRect& tile_rect,
                                      const SkMatrix* pattern_matrix,
                                      const FloatRect& src_rect) final;

 private:
  PaintWorkletDeferredImage(scoped_refptr<PaintWorkletInput> input,
                            const FloatSize& size)
      : GeneratedImage(size) {
    image_ = PaintImageBuilder::WithDefault()
                 .set_paint_worklet_input(std::move(input))
                 .set_id(PaintImage::GetNextId())
                 .TakePaintImage();
  }

  PaintImage image_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_CSSOM_PAINT_WORKLET_DEFERRED_IMAGE_H_

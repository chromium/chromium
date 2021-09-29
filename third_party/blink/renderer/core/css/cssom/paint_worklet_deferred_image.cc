// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/paint_worklet_deferred_image.h"

#include <utility>

#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"

namespace blink {

namespace {
void DrawInternal(cc::PaintCanvas* canvas,
                  const PaintFlags& flags,
                  const FloatRect& dest_rect,
                  const FloatRect& src_rect,
                  const ImageDrawOptions& draw_options,
                  const PaintImage& image) {
  canvas->drawImageRect(
      image, src_rect, dest_rect, draw_options.sampling_options, &flags,
      WebCoreClampingModeToSkiaRectConstraint(draw_options.clamping_mode));
}
}  // namespace

void PaintWorkletDeferredImage::Draw(cc::PaintCanvas* canvas,
                                     const PaintFlags& flags,
                                     const FloatRect& dest_rect,
                                     const FloatRect& src_rect,
                                     const ImageDrawOptions& draw_options) {
  DrawInternal(canvas, flags, dest_rect, src_rect, draw_options, image_);
}

void PaintWorkletDeferredImage::DrawTile(GraphicsContext& context,
                                         const FloatRect& src_rect,
                                         const ImageDrawOptions& draw_options) {
  DrawInternal(context.Canvas(), context.FillFlags(), FloatRect(), src_rect,
               draw_options, image_);
}

sk_sp<PaintShader> PaintWorkletDeferredImage::CreateShader(
    const FloatRect& tile_rect,
    const SkMatrix* pattern_matrix,
    const FloatRect& src_rect,
    const ImageDrawOptions&) {
  SkRect tile = SkRect::MakeXYWH(tile_rect.X(), tile_rect.Y(),
                                 tile_rect.Width(), tile_rect.Height());
  sk_sp<PaintShader> shader = PaintShader::MakeImage(
      image_, SkTileMode::kRepeat, SkTileMode::kRepeat, pattern_matrix, &tile);

  return shader;
}

}  // namespace blink

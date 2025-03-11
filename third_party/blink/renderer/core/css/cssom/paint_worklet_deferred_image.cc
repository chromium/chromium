// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/cssom/paint_worklet_deferred_image.h"

#include "base/notreached.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

void PaintWorkletDeferredImage::Draw(cc::PaintCanvas* canvas,
                                     const cc::PaintFlags& flags,
                                     const gfx::RectF& dest_rect,
                                     const gfx::RectF& src_rect,
                                     const ImageDrawOptions& draw_options) {
  canvas->drawImageRect(image_, gfx::RectFToSkRect(src_rect),
                        gfx::RectFToSkRect(dest_rect),
                        draw_options.sampling_options, &flags,
                        ToSkiaRectConstraint(draw_options.clamping_mode));
}

void PaintWorkletDeferredImage::DrawTile(cc::PaintCanvas*,
                                         const gfx::RectF&,
                                         const ImageDrawOptions&) {
  // Because `CreateShader()` is overridden, this hook won't be used.
  // See `GeneratedImage::CreateShader()`.
  NOTREACHED();
}

sk_sp<PaintShader> PaintWorkletDeferredImage::CreateShader(
    const gfx::RectF& tile_rect,
    const SkMatrix* pattern_matrix,
    const gfx::RectF& src_rect,
    const ImageDrawOptions&) {
  SkRect tile = gfx::RectFToSkRect(tile_rect);
  return PaintShader::MakeImage(image_, SkTileMode::kRepeat,
                                SkTileMode::kRepeat, pattern_matrix, &tile);
}

}  // namespace blink

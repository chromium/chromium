// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/paint_generated_image.h"

#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

void PaintGeneratedImage::Draw(cc::PaintCanvas* canvas,
                               const cc::PaintFlags& flags,
                               const gfx::RectF& dest_rect,
                               const gfx::RectF& src_rect,
                               const ImageDrawOptions&) {
  PaintCanvasAutoRestore ar(canvas, true);
  SkRect sk_dest_rect = gfx::RectFToSkRect(dest_rect);
  SkRect sk_src_rect = gfx::RectFToSkRect(src_rect);
  canvas->clipRect(sk_dest_rect);
  canvas->concat(SkM44::RectToRect(sk_src_rect, sk_dest_rect));
  canvas->saveLayer(sk_src_rect, flags);
  canvas->drawPicture(record_);
}

void PaintGeneratedImage::DrawTile(cc::PaintCanvas* canvas,
                                   const gfx::RectF& src_rect,
                                   const ImageDrawOptions&) {
  canvas->drawPicture(record_);
}

}  // namespace blink

/*
 * Copyright (C) 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/gradient_generated_image.h"

#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

void GradientGeneratedImage::Draw(cc::PaintCanvas* canvas,
                                  const cc::PaintFlags& flags,
                                  const gfx::RectF& dest_rect,
                                  const gfx::RectF& src_rect,
                                  const ImageDrawOptions& draw_options) {
  SkRect visible_src_rect = gfx::RectFToSkRect(src_rect);
  if (!visible_src_rect.intersect(
          SkRect::MakeWH(size_.width(), size_.height())))
    return;

  const SkMatrix transform = SkMatrix::RectToRect(
      gfx::RectFToSkRect(src_rect), gfx::RectFToSkRect(dest_rect));
  SkRect visible_dest_rect;
  transform.mapRect(&visible_dest_rect, visible_src_rect);

  cc::PaintFlags gradient_flags(flags);
  gradient_->ApplyToFlags(gradient_flags, transform, draw_options);
  canvas->drawRect(visible_dest_rect, gradient_flags);
}

void GradientGeneratedImage::DrawTile(cc::PaintCanvas* canvas,
                                      const gfx::RectF& src_rect,
                                      const ImageDrawOptions& draw_options) {
  cc::PaintFlags gradient_flags;
  gradient_flags.setAntiAlias(true);
  gradient_->ApplyToFlags(gradient_flags, SkMatrix::I(), draw_options);
  canvas->drawRect(gfx::RectFToSkRect(src_rect), gradient_flags);
}

bool GradientGeneratedImage::ApplyShader(cc::PaintFlags& flags,
                                         const SkMatrix& local_matrix,
                                         const gfx::RectF& src_rect,
                                         const ImageDrawOptions& draw_options) {
  DCHECK(gradient_);
  gradient_->ApplyToFlags(flags, local_matrix, draw_options);
  return true;
}

}  // namespace blink

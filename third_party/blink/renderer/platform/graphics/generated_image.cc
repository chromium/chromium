/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/generated_image.h"

#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_controller.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"

namespace blink {

void GeneratedImage::DrawPattern(GraphicsContext& dest_context,
                                 const FloatRect& src_rect,
                                 const FloatSize& scale,
                                 const FloatPoint& phase,
                                 SkBlendMode composite_op,
                                 const FloatRect& dest_rect,
                                 const FloatSize& repeat_spacing) {
  FloatRect tile_rect = src_rect;
  tile_rect.Expand(repeat_spacing);

  SkMatrix pattern_matrix = SkMatrix::MakeTrans(phase.X(), phase.Y());
  pattern_matrix.preScale(scale.Width(), scale.Height());
  pattern_matrix.preTranslate(tile_rect.X(), tile_rect.Y());

  sk_sp<PaintShader> tile_shader =
      CreateShader(tile_rect, &pattern_matrix, src_rect);

  PaintFlags fill_flags = dest_context.FillFlags();
  fill_flags.setShader(std::move(tile_shader));
  fill_flags.setColor(SK_ColorBLACK);
  fill_flags.setBlendMode(composite_op);

  dest_context.DrawRect(dest_rect, fill_flags);
}

sk_sp<PaintShader> GeneratedImage::CreateShader(const FloatRect& tile_rect,
                                                const SkMatrix* pattern_matrix,
                                                const FloatRect& src_rect) {
  auto paint_controller = std::make_unique<PaintController>();
  GraphicsContext context(*paint_controller);
  context.BeginRecording(tile_rect);
  DrawTile(context, src_rect);
  sk_sp<PaintRecord> record = context.EndRecording();

  return PaintShader::MakePaintRecord(std::move(record), tile_rect,
                                      SkTileMode::kRepeat, SkTileMode::kRepeat,
                                      pattern_matrix);
}

PaintImage GeneratedImage::PaintImageForCurrentFrame() {
  return PaintImage();
}

}  // namespace blink

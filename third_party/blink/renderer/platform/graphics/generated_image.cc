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

#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_recorder.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_shader.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

void GeneratedImage::DrawPattern(GraphicsContext& dest_context,
                                 const cc::PaintFlags& base_flags,
                                 const gfx::RectF& dest_rect,
                                 const ImageTilingInfo& tiling_info,
                                 const ImageDrawOptions& options) {
  gfx::RectF tile_rect = tiling_info.image_rect;
  tile_rect.set_size(tile_rect.size() + tiling_info.spacing);

  SkMatrix pattern_matrix =
      SkMatrix::Translate(tiling_info.phase.x(), tiling_info.phase.y());
  pattern_matrix.preScale(tiling_info.scale.x(), tiling_info.scale.y());
  pattern_matrix.preTranslate(tile_rect.x(), tile_rect.y());

  ImageDrawOptions draw_options(options);
  // TODO(fs): Computing sampling options using `size_` and the tile source
  // rect doesn't seem all too useful since they should be in the same space.
  // Should probably be using the tile source mapped to destination space
  // (instead of `size_`).
  draw_options.sampling_options = dest_context.ComputeSamplingOptions(
      *this, gfx::RectF(size_), tiling_info.image_rect);
  sk_sp<PaintShader> tile_shader = CreateShader(
      tile_rect, &pattern_matrix, tiling_info.image_rect, draw_options);

  cc::PaintFlags fill_flags(base_flags);
  fill_flags.setShader(std::move(tile_shader));
  fill_flags.setColor(SK_ColorBLACK);

  dest_context.DrawRect(gfx::RectFToSkRect(dest_rect), fill_flags,
                        AutoDarkMode(draw_options));
}

sk_sp<PaintShader> GeneratedImage::CreateShader(
    const gfx::RectF& tile_rect,
    const SkMatrix* pattern_matrix,
    const gfx::RectF& src_rect,
    const ImageDrawOptions& draw_options) {
  PaintRecorder recorder;
  DrawTile(recorder.beginRecording(), src_rect, draw_options);
  return PaintShader::MakePaintRecord(
      recorder.finishRecordingAsPicture(), gfx::RectFToSkRect(tile_rect),
      SkTileMode::kRepeat, SkTileMode::kRepeat, pattern_matrix);
}

PaintImage GeneratedImage::PaintImageForCurrentFrame() {
  return PaintImage();
}

}  // namespace blink

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

#include "third_party/blink/renderer/platform/graphics/crossfade_generated_image.h"

#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

CrossfadeGeneratedImage::CrossfadeGeneratedImage(Vector<WeightedImage> images,
                                                 const gfx::SizeF& size)
    : GeneratedImage(size), images_(std::move(images)) {}

void CrossfadeGeneratedImage::DrawCrossfade(
    cc::PaintCanvas* canvas,
    const cc::PaintFlags& flags,
    const ImageDrawOptions& draw_options) {
  gfx::RectF dest_rect(size_);

  // TODO(junov): The various effects encoded into paint should probably be
  // applied here instead of inside the layer.  This probably faulty behavior
  // was maintained in order to preserve pre-existing behavior while refactoring
  // this code.  This should be investigated further. crbug.com/472634
  cc::PaintFlags layer_flags;
  layer_flags.setBlendMode(flags.getBlendMode());
  PaintCanvasAutoRestore ar(canvas, false);
  canvas->saveLayer(layer_flags);

  cc::PaintFlags image_flags(flags);

  for (unsigned image_idx = 0; image_idx < images_.size(); ++image_idx) {
    ImageDrawOptions image_draw_options(draw_options);
    if (image_idx == 0) {
      // TODO(junov): This code should probably be propagating the
      // RespectImageOrientationEnum from CrossfadeGeneratedImage::draw(). Code
      // was written this way during refactoring to avoid modifying existing
      // behavior, but this warrants further investigation. crbug.com/472634
      image_draw_options.respect_orientation = kDoNotRespectImageOrientation;
      image_flags.setBlendMode(SkBlendMode::kSrcOver);
    } else {
      image_flags.setBlendMode(SkBlendMode::kPlus);
    }
    const WeightedImage& image = images_[image_idx];
    image_flags.setColor(ScaleAlpha(flags.getColor(), image.weight));
    image.image->Draw(canvas, image_flags, dest_rect,
                      gfx::RectF(gfx::SizeF(image.image->Size())),
                      image_draw_options);
  }
}

void CrossfadeGeneratedImage::Draw(cc::PaintCanvas* canvas,
                                   const cc::PaintFlags& flags,
                                   const gfx::RectF& dst_rect,
                                   const gfx::RectF& src_rect,
                                   const ImageDrawOptions& draw_options) {
  // Draw nothing if any of the images have not loaded yet.
  for (const WeightedImage& image : images_) {
    if (image.image == Image::NullImage()) {
      return;
    }
  }

  PaintCanvasAutoRestore ar(canvas, true);
  SkRect src_sk_rect = gfx::RectFToSkRect(src_rect);
  SkRect dst_sk_rect = gfx::RectFToSkRect(dst_rect);
  canvas->clipRect(dst_sk_rect);
  canvas->concat(SkM44::RectToRect(src_sk_rect, dst_sk_rect));
  DrawCrossfade(canvas, flags, draw_options);
}

void CrossfadeGeneratedImage::DrawTile(cc::PaintCanvas* canvas,
                                       const gfx::RectF& src_rect,
                                       const ImageDrawOptions& options) {
  // Draw nothing if either of the images hasn't loaded yet.
  for (const WeightedImage& image : images_) {
    if (image.image == Image::NullImage()) {
      return;
    }
  }
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  DrawCrossfade(canvas, flags, options);
}

}  // namespace blink

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

#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"

namespace blink {

CrossfadeGeneratedImage::CrossfadeGeneratedImage(
    scoped_refptr<Image> from_image,
    scoped_refptr<Image> to_image,
    float percentage,
    const FloatSize& size)
    : GeneratedImage(size),
      from_image_(std::move(from_image)),
      to_image_(std::move(to_image)),
      percentage_(percentage) {}

void CrossfadeGeneratedImage::DrawCrossfade(
    cc::PaintCanvas* canvas,
    const SkSamplingOptions& sampling,
    const PaintFlags& flags,
    RespectImageOrientationEnum respect_orientation,
    ImageClampingMode clamp_mode,
    ImageDecodingMode decode_mode) {
  FloatRect from_image_rect(FloatPoint(), FloatSize(from_image_->Size()));
  FloatRect to_image_rect(FloatPoint(), FloatSize(to_image_->Size()));
  FloatRect dest_rect((FloatPoint()), size_);

  // TODO(junov): The various effects encoded into paint should probably be
  // applied here instead of inside the layer.  This probably faulty behavior
  // was maintained in order to preserve pre-existing behavior while refactoring
  // this code.  This should be investigated further. crbug.com/472634
  PaintFlags layer_flags;
  layer_flags.setBlendMode(flags.getBlendMode());
  PaintCanvasAutoRestore ar(canvas, false);
  canvas->saveLayer(nullptr, &layer_flags);

  PaintFlags image_flags(flags);
  image_flags.setBlendMode(SkBlendMode::kSrcOver);
  image_flags.setColor(ScaleAlpha(flags.getColor(), 1 - percentage_));
  // TODO(junov): This code should probably be propagating the
  // RespectImageOrientationEnum from CrossfadeGeneratedImage::draw(). Code was
  // written this way during refactoring to avoid modifying existing behavior,
  // but this warrants further investigation. crbug.com/472634
  from_image_->Draw(canvas, image_flags, dest_rect, from_image_rect, sampling,
                    kDoNotRespectImageOrientation, clamp_mode, decode_mode);
  image_flags.setBlendMode(SkBlendMode::kPlus);
  image_flags.setColor(ScaleAlpha(flags.getColor(), percentage_));
  to_image_->Draw(canvas, image_flags, dest_rect, to_image_rect, sampling,
                  respect_orientation, clamp_mode, decode_mode);
}

void CrossfadeGeneratedImage::Draw(
    cc::PaintCanvas* canvas,
    const PaintFlags& flags,
    const FloatRect& dst_rect,
    const FloatRect& src_rect,
    const SkSamplingOptions& sampling,
    RespectImageOrientationEnum respect_orientation,
    ImageClampingMode clamp_mode,
    ImageDecodingMode decode_mode) {
  // Draw nothing if either of the images hasn't loaded yet.
  if (from_image_ == Image::NullImage() || to_image_ == Image::NullImage())
    return;

  PaintCanvasAutoRestore ar(canvas, true);
  canvas->clipRect(dst_rect);
  canvas->concat(SkMatrix::RectToRect(src_rect, dst_rect));
  DrawCrossfade(canvas, sampling, flags, respect_orientation, clamp_mode,
                decode_mode);
}

void CrossfadeGeneratedImage::DrawTile(
    GraphicsContext& context,
    const FloatRect& src_rect,
    RespectImageOrientationEnum respect_orientation) {
  // Draw nothing if either of the images hasn't loaded yet.
  if (from_image_ == Image::NullImage() || to_image_ == Image::NullImage())
    return;

  PaintFlags flags = context.FillFlags();
  flags.setBlendMode(SkBlendMode::kSrcOver);
  FloatRect dest_rect((FloatPoint()), size_);
  DrawCrossfade(context.Canvas(),
                context.ComputeSamplingOptions(this, dest_rect, src_rect),
                flags, respect_orientation, kClampImageToSourceRect,
                kSyncDecode);
}

}  // namespace blink

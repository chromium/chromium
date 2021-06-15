// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/static_bitmap_image.h"

#include "base/numerics/checked_math.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image_observer.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "v8/include/v8.h"

namespace blink {

scoped_refptr<StaticBitmapImage> StaticBitmapImage::Create(
    PaintImage image,
    ImageOrientation orientation) {
  DCHECK(!image.IsTextureBacked());
  return UnacceleratedStaticBitmapImage::Create(std::move(image), orientation);
}

scoped_refptr<StaticBitmapImage> StaticBitmapImage::Create(
    sk_sp<SkData> data,
    const SkImageInfo& info,
    ImageOrientation orientation) {
  return UnacceleratedStaticBitmapImage::Create(
      SkImage::MakeRasterData(info, std::move(data), info.minRowBytes()),
      orientation);
}

IntSize StaticBitmapImage::SizeWithConfig(SizeConfig config) const {
  IntSize size = SizeInternal();
  if (config.apply_orientation && orientation_.UsesWidthAsHeight())
    size = size.TransposedSize();
  return size;
}

void StaticBitmapImage::DrawHelper(
    cc::PaintCanvas* canvas,
    const PaintFlags& flags,
    const FloatRect& dst_rect,
    const FloatRect& src_rect,
    const SkSamplingOptions& sampling,
    ImageClampingMode clamp_mode,
    RespectImageOrientationEnum respect_orientation,
    const PaintImage& image) {
  FloatRect adjusted_src_rect = src_rect;
  adjusted_src_rect.Intersect(SkRect::MakeWH(image.width(), image.height()));

  if (dst_rect.IsEmpty() || adjusted_src_rect.IsEmpty())
    return;  // Nothing to draw.

  cc::PaintCanvasAutoRestore auto_restore(canvas, false);
  FloatRect adjusted_dst_rect = dst_rect;
  if (respect_orientation && orientation_ != ImageOrientationEnum::kDefault) {
    canvas->save();

    // ImageOrientation expects the origin to be at (0, 0)
    canvas->translate(adjusted_dst_rect.X(), adjusted_dst_rect.Y());
    adjusted_dst_rect.SetLocation(FloatPoint());

    canvas->concat(AffineTransformToSkMatrix(
        orientation_.TransformFromDefault(adjusted_dst_rect.Size())));

    if (orientation_.UsesWidthAsHeight()) {
      adjusted_dst_rect =
          FloatRect(adjusted_dst_rect.X(), adjusted_dst_rect.Y(),
                    adjusted_dst_rect.Height(), adjusted_dst_rect.Width());
    }
  }

  canvas->drawImageRect(image, adjusted_src_rect, adjusted_dst_rect, sampling,
                        &flags,
                        WebCoreClampingModeToSkiaRectConstraint(clamp_mode));
}

}  // namespace blink

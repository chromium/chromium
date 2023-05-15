// Copyright 2014 The Chromium Authors
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
#include "ui/gfx/geometry/skia_conversions.h"
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
      SkImages::RasterFromData(info, std::move(data), info.minRowBytes()),
      orientation);
}

gfx::Size StaticBitmapImage::SizeWithConfig(SizeConfig config) const {
  auto info = GetSkImageInfo();
  gfx::Size size(info.width(), info.height());
  if (config.apply_orientation && orientation_.UsesWidthAsHeight())
    size.Transpose();
  return size;
}

Vector<uint8_t> StaticBitmapImage::CopyImageData(const SkImageInfo& info,
                                                 bool apply_orientation) {
  if (info.isEmpty())
    return {};
  PaintImage paint_image = PaintImageForCurrentFrame();
  if (paint_image.GetSkImageInfo().isEmpty())
    return {};

  wtf_size_t byte_length =
      base::checked_cast<wtf_size_t>(info.computeMinByteSize());
  if (byte_length > partition_alloc::MaxDirectMapped())
    return {};
  Vector<uint8_t> dst_buffer(byte_length);

  bool read_pixels_successful =
      paint_image.readPixels(info, dst_buffer.data(), info.minRowBytes(), 0, 0);
  DCHECK(read_pixels_successful);
  if (!read_pixels_successful)
    return {};

  // Orient the data, and re-read the pixels.
  if (apply_orientation && !HasDefaultOrientation()) {
    paint_image = Image::ResizeAndOrientImage(
        paint_image, CurrentFrameOrientation(), gfx::Vector2dF(1, 1), 1,
        kInterpolationNone);
    read_pixels_successful = paint_image.readPixels(info, dst_buffer.data(),
                                                    info.minRowBytes(), 0, 0);
    DCHECK(read_pixels_successful);
    if (!read_pixels_successful)
      return {};
  }

  return dst_buffer;
}

void StaticBitmapImage::DrawHelper(cc::PaintCanvas* canvas,
                                   const cc::PaintFlags& flags,
                                   const gfx::RectF& dst_rect,
                                   const gfx::RectF& src_rect,
                                   const ImageDrawOptions& draw_options,
                                   const PaintImage& image) {
  gfx::RectF adjusted_src_rect = src_rect;
  adjusted_src_rect.Intersect(gfx::RectF(image.width(), image.height()));

  if (dst_rect.IsEmpty() || adjusted_src_rect.IsEmpty())
    return;  // Nothing to draw.

  cc::PaintCanvasAutoRestore auto_restore(canvas, false);
  gfx::RectF adjusted_dst_rect = dst_rect;
  if (draw_options.respect_orientation &&
      orientation_ != ImageOrientationEnum::kDefault) {
    canvas->save();

    // ImageOrientation expects the origin to be at (0, 0)
    canvas->translate(adjusted_dst_rect.x(), adjusted_dst_rect.y());
    adjusted_dst_rect.set_origin(gfx::PointF());

    canvas->concat(AffineTransformToSkM44(
        orientation_.TransformFromDefault(adjusted_dst_rect.size())));

    if (orientation_.UsesWidthAsHeight())
      adjusted_dst_rect.set_size(gfx::TransposeSize(adjusted_dst_rect.size()));
  }

  canvas->drawImageRect(
      image, gfx::RectFToSkRect(adjusted_src_rect),
      gfx::RectFToSkRect(adjusted_dst_rect), draw_options.sampling_options,
      &flags,
      WebCoreClampingModeToSkiaRectConstraint(draw_options.clamping_mode));
}

}  // namespace blink

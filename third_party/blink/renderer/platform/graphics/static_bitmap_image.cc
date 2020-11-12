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

IntSize StaticBitmapImage::PreferredDisplaySize() const {
  if (orientation_.UsesWidthAsHeight())
    return Size().TransposedSize();
  else
    return Size();
}

void StaticBitmapImage::DrawHelper(
    cc::PaintCanvas* canvas,
    const PaintFlags& flags,
    const FloatRect& dst_rect,
    const FloatRect& src_rect,
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

  canvas->drawImageRect(image, adjusted_src_rect, adjusted_dst_rect, &flags,
                        WebCoreClampingModeToSkiaRectConstraint(clamp_mode));
}

base::CheckedNumeric<size_t> StaticBitmapImage::GetSizeInBytes(
    const IntRect& rect,
    const CanvasColorParams& color_params) {
  uint8_t bytes_per_pixel = color_params.BytesPerPixel();
  base::CheckedNumeric<size_t> data_size = bytes_per_pixel;
  data_size *= rect.Size().Area();
  return data_size;
}

bool StaticBitmapImage::MayHaveStrayArea(
    scoped_refptr<StaticBitmapImage> src_image,
    const IntRect& rect) {
  if (!src_image)
    return false;

  return rect.X() < 0 || rect.Y() < 0 ||
         rect.MaxX() > src_image->Size().Width() ||
         rect.MaxY() > src_image->Size().Height();
}

bool StaticBitmapImage::CopyToByteArray(
    scoped_refptr<StaticBitmapImage> src_image,
    base::span<uint8_t> dst,
    const IntRect& rect,
    const CanvasColorParams& color_params) {
  DCHECK_EQ(dst.size(), GetSizeInBytes(rect, color_params).ValueOrDie());

  if (!src_image)
    return true;

  if (dst.size() == 0)
    return true;

  SkColorType color_type =
      (color_params.GetSkColorType() == kRGBA_F16_SkColorType)
          ? kRGBA_F16_SkColorType
          : kRGBA_8888_SkColorType;
  SkImageInfo info =
      SkImageInfo::Make(rect.Width(), rect.Height(), color_type,
                        kUnpremul_SkAlphaType, color_params.GetSkColorSpace());
  bool read_pixels_successful =
      src_image->PaintImageForCurrentFrame().readPixels(
          info, dst.data(), info.minRowBytes(), rect.X(), rect.Y());
  DCHECK(read_pixels_successful ||
         !src_image->PaintImageForCurrentFrame()
              .GetSkImageInfo()
              .bounds()
              .intersect(SkIRect::MakeXYWH(rect.X(), rect.Y(), info.width(),
                                           info.height())));
  return true;
}

}  // namespace blink

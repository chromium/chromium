// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_non2d_snapshot_provider_bitmap.h"

#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/renderer/platform/graphics/canvas_deferred_paint_record.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"

namespace blink {

CanvasNon2DSnapshotProviderBitmap::ImageProviderImpl::ImageProviderImpl(
    bool is_f16,
    const gfx::ColorSpace& color_space)
    : is_f16_(is_f16), color_space_(color_space) {}

cc::ImageProvider::ScopedResult
CanvasNon2DSnapshotProviderBitmap::ImageProviderImpl::GetRasterContent(
    const cc::DrawImage& draw_image) {
  cc::PaintImage paint_image = draw_image.paint_image();
  if (paint_image.IsDeferredPaintRecord()) {
    CHECK(!paint_image.IsPaintWorklet());
    scoped_refptr<CanvasDeferredPaintRecord> canvas_deferred_paint_record(
        static_cast<CanvasDeferredPaintRecord*>(
            paint_image.deferred_paint_record().get()));
    return cc::ImageProvider::ScopedResult(
        canvas_deferred_paint_record->GetPaintRecord());
  }

  // To decode high bit depth image source to half float backed image, we need
  // to sniff the image bit depth here to avoid double decoding.
  auto target_color_type =
      (is_f16_ && draw_image.paint_image().is_high_bit_depth())
          ? kRGBA_F16_SkColorType
          : kN32_SkColorType;
  cc::TargetColorParams target_color_params;
  target_color_params.color_space = color_space_;

  return cc::PlaybackImageProvider(
             &Image::SharedCCDecodeCache(target_color_type),
             target_color_params, cc::PlaybackImageProvider::Settings())
      .GetRasterContent(draw_image);
}

std::unique_ptr<CanvasNon2DSnapshotProviderBitmap>
CanvasNon2DSnapshotProviderBitmap::Create(
    const CanvasSnapshotProvider::Info& info) {
  return base::WrapUnique<CanvasNon2DSnapshotProviderBitmap>(
      new CanvasNon2DSnapshotProviderBitmap(info));
}

CanvasNon2DSnapshotProviderBitmap::CanvasNon2DSnapshotProviderBitmap(
    const CanvasSnapshotProvider::Info& info)
    : info_(info) {}

CanvasNon2DSnapshotProviderBitmap::~CanvasNon2DSnapshotProviderBitmap() =
    default;

bool CanvasNon2DSnapshotProviderBitmap::IsGpuContextLost() const {
  return true;
}

bool CanvasNon2DSnapshotProviderBitmap::IsValid() const {
  // This class doesn't attempt to create an SkSurface until
  // DoExternalDrawAndSnapshot() is invoked; it will detect failure to create
  // the surface at that point and return nullptr.
  return true;
}

scoped_refptr<StaticBitmapImage>
CanvasNon2DSnapshotProviderBitmap::DoExternalDrawAndSnapshot(
    base::FunctionRef<void(cc::PaintCanvas&)> draw_callback,
    ImageOrientation orientation /*= ImageOrientationEnum::kDefault*/) {
  const bool can_use_lcd_text = info_.alpha_type == kOpaque_SkAlphaType;
  const auto props =
      skia::LegacyDisplayGlobals::ComputeSurfaceProps(can_use_lcd_text);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(
      SkImageInfo::Make(info_.size.width(), info_.size.height(),
                        viz::ToClosestSkColorType(info_.format),
                        kPremul_SkAlphaType,
                        info_.color_space.ToSkColorSpace()),
      &props);
  if (!surface) {
    return nullptr;
  }

  ImageProviderImpl image_provider(
      GetSharedImageFormat() == viz::SinglePlaneFormat::kRGBA_F16,
      GetColorSpace());
  cc::SkiaPaintCanvas canvas(surface->getCanvas(), &image_provider);
  draw_callback(canvas);

  cc::PaintImage paint_image;

  auto sk_image = surface->makeImageSnapshot();
  if (sk_image) {
    paint_image =
        PaintImageBuilder::WithDefault()
            .set_id(cc::PaintImage::GetNextId())
            .set_image(std::move(sk_image), PaintImage::GetNextContentId())
            .TakePaintImage();
  }

  DCHECK(!paint_image.IsTextureBacked());
  return UnacceleratedStaticBitmapImage::Create(std::move(paint_image),
                                                orientation);
}

}  // namespace blink

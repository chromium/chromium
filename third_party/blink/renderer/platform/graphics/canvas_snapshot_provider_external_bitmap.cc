// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_snapshot_provider_external_bitmap.h"

#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/renderer/platform/graphics/canvas_deferred_paint_record.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"

namespace blink {

cc::ImageProvider::ScopedResult
CanvasSnapshotProviderExternalBitmap::GetRasterContent(
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

  // TODO(xidachen): Ensure this function works for paint worklet generated
  // images.
  // If we like to decode high bit depth image source to half float backed
  // image, we need to sniff the image bit depth here to avoid double
  // decoding.
  ImageProvider::ScopedResult scoped_decoded_image;
  if (playback_image_provider_f16_ &&
      draw_image.paint_image().is_high_bit_depth()) {
    scoped_decoded_image =
        playback_image_provider_f16_->GetRasterContent(draw_image);
  } else {
    scoped_decoded_image =
        playback_image_provider_n32_->GetRasterContent(draw_image);
  }
  return scoped_decoded_image;
}

std::unique_ptr<CanvasSnapshotProviderExternalBitmap>
CanvasSnapshotProviderExternalBitmap::Create(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space) {
  auto provider = base::WrapUnique<CanvasSnapshotProviderExternalBitmap>(
      new CanvasSnapshotProviderExternalBitmap(size, format, alpha_type,
                                               color_space));
  if (provider->IsValid()) {
    return provider;
  }
  return nullptr;
}

CanvasSnapshotProviderExternalBitmap::CanvasSnapshotProviderExternalBitmap(
    gfx::Size size,
    viz::SharedImageFormat format,
    SkAlphaType alpha_type,
    const gfx::ColorSpace& color_space)
    : size_(size),
      format_(format),
      alpha_type_(alpha_type),
      color_space_(color_space),
      snapshot_paint_image_id_(cc::PaintImage::GetNextId()),
      recorder_(
          std::make_unique<MemoryManagedPaintRecorder>(Size(),
                                                       /*client=*/nullptr)) {
  const bool can_use_lcd_text = alpha_type_ == kOpaque_SkAlphaType;
  const auto props =
      skia::LegacyDisplayGlobals::ComputeSurfaceProps(can_use_lcd_text);
  surface_ = SkSurfaces::Raster(
      SkImageInfo::Make(size_.width(), size_.height(),
                        viz::ToClosestSkColorType(format_), kPremul_SkAlphaType,
                        color_space_.ToSkColorSpace()),
      &props);
}

CanvasSnapshotProviderExternalBitmap::~CanvasSnapshotProviderExternalBitmap() =
    default;

bool CanvasSnapshotProviderExternalBitmap::IsGpuContextLost() const {
  return true;
}

bool CanvasSnapshotProviderExternalBitmap::IsValid() const {
  return surface_.get();
}

scoped_refptr<StaticBitmapImage>
CanvasSnapshotProviderExternalBitmap::DoExternalDrawAndSnapshot(
    base::FunctionRef<void(MemoryManagedPaintCanvas&)> draw_callback,
    ImageOrientation orientation /*= ImageOrientationEnum::kDefault*/) {
  // The static creation method returns nullptr if `IsValid()` is false on the
  // created instance, and once `surface_` is created, it is never destroyed
  // until the instance itself is destroyed.
  CHECK(surface_);

  draw_callback(recorder_->getRecordingCanvas());

  if (recorder_->HasReleasableDrawOps()) {
    if (!skia_canvas_) {
      if (!playback_image_provider_n32_) {
        // Create an ImageDecodeCache for half float images only if the canvas
        // is using half float back storage.
        cc::ImageDecodeCache* cache_f16 = nullptr;
        if (GetSharedImageFormat() == viz::SinglePlaneFormat::kRGBA_F16) {
          cache_f16 = &Image::SharedCCDecodeCache(kRGBA_F16_SkColorType);
        }

        cc::ImageDecodeCache* cache_rgba8 =
            &Image::SharedCCDecodeCache(kN32_SkColorType);

        cc::TargetColorParams target_color_params;
        target_color_params.color_space = color_space_;
        playback_image_provider_n32_.emplace(
            cache_rgba8, target_color_params,
            cc::PlaybackImageProvider::Settings());

        // If the image provider may require to decode to half float instead of
        // uint8, create a f16 PlaybackImageProvider with the passed cache.
        if (format_ == viz::SinglePlaneFormat::kRGBA_F16) {
          DCHECK(cache_f16);
          playback_image_provider_f16_.emplace(
              cache_f16, target_color_params,
              cc::PlaybackImageProvider::Settings());
        }
      }

      skia_canvas_ =
          std::make_unique<cc::SkiaPaintCanvas>(surface_->getCanvas(), this);
    }

    skia_canvas_->drawPicture(recorder_->ReleaseMainRecording());
  }

  cc::PaintImage paint_image;

  auto sk_image = surface_->makeImageSnapshot();
  if (sk_image) {
    auto last_snapshot_sk_image_id = snapshot_sk_image_id_;
    snapshot_sk_image_id_ = sk_image->uniqueID();

    // Ensure that a new PaintImage::ContentId is used only when the underlying
    // SkImage changes. This is necessary to ensure that the same image results
    // in a cache hit in cc's ImageDecodeCache.
    if (snapshot_paint_image_content_id_ == PaintImage::kInvalidContentId ||
        last_snapshot_sk_image_id != snapshot_sk_image_id_) {
      snapshot_paint_image_content_id_ = PaintImage::GetNextContentId();
    }

    paint_image =
        PaintImageBuilder::WithDefault()
            .set_id(snapshot_paint_image_id_)
            .set_image(std::move(sk_image), snapshot_paint_image_content_id_)
            .TakePaintImage();
  }

  DCHECK(!paint_image.IsTextureBacked());
  return UnacceleratedStaticBitmapImage::Create(std::move(paint_image),
                                                orientation);
}

}  // namespace blink

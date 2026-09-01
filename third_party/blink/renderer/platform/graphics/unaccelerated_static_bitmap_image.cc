// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"

#include "base/process/memory.h"
#include "cc/raster/playback_image_provider.h"
#include "components/viz/common/gpu/context_provider.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_2d_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/image.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_image.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_skia.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {

scoped_refptr<UnacceleratedStaticBitmapImage>
UnacceleratedStaticBitmapImage::Create(sk_sp<SkImage> image,
                                       ImageOrientation orientation,
                                       const gfx::HDRMetadata& hdr_metadata) {
  if (!image) {
    return nullptr;
  }
  DCHECK(!image->isTextureBacked());
  return base::AdoptRef(new UnacceleratedStaticBitmapImage(
      std::move(image), orientation, hdr_metadata));
}

UnacceleratedStaticBitmapImage::UnacceleratedStaticBitmapImage(
    sk_sp<SkImage> image,
    ImageOrientation orientation,
    const gfx::HDRMetadata& hdr_metadata)
    : StaticBitmapImage(orientation) {
  CHECK(image);
  DCHECK(!image->isLazyGenerated());
  paint_image_ =
      CreatePaintImageBuilder()
          .set_image(std::move(image), cc::PaintImage::GetNextContentId())
          .set_hdr_metadata(hdr_metadata)
          .TakePaintImage();
}

scoped_refptr<UnacceleratedStaticBitmapImage>
UnacceleratedStaticBitmapImage::Create(PaintImage image,
                                       ImageOrientation orientation) {
  return base::AdoptRef(
      new UnacceleratedStaticBitmapImage(std::move(image), orientation));
}

scoped_refptr<StaticBitmapImage>
UnacceleratedStaticBitmapImage::CreateFromRaster(
    const gfx::Size& size,
    base::FunctionRef<void(cc::PaintCanvas&)> draw_callback,
    scoped_refptr<const cc::AnimatedImageFrameIndexMap>
        animated_image_frame_index_map) {
  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height(),
                                                SkColorSpace::MakeSRGB());
  SkSurfaceProps props = skia::LegacyDisplayGlobals::ComputeSurfaceProps(
      /*can_use_lcd_text=*/false);
  sk_sp<SkSurface> surface = SkSurfaces::Raster(info, &props);
  if (!surface) {
    return nullptr;
  }

  cc::TargetColorParams target_color_params;
  target_color_params.color_space = gfx::ColorSpace::CreateSRGB();
  cc::PlaybackImageProvider::Settings settings;
  settings.raster_mode = cc::PlaybackImageProvider::RasterMode::kSoftware;

  cc::PlaybackImageProvider image_provider(
      &Image::SharedCCDecodeCache(kN32_SkColorType), target_color_params,
      std::move(settings));
  image_provider.SetAnimatedImageFrameIndexes(animated_image_frame_index_map);

  cc::SkiaPaintCanvas canvas(surface->getCanvas(), &image_provider);
  draw_callback(canvas);
  sk_sp<SkImage> sk_image = surface->makeImageSnapshot();
  cc::PaintImage paint_image;
  if (sk_image) {
    paint_image =
        PaintImageBuilder::WithDefault()
            .set_id(cc::PaintImage::GetNextId())
            .set_image(std::move(sk_image), cc::PaintImage::GetNextContentId())
            .TakePaintImage();
  }
  return UnacceleratedStaticBitmapImage::Create(std::move(paint_image));
}

UnacceleratedStaticBitmapImage::UnacceleratedStaticBitmapImage(
    PaintImage image,
    ImageOrientation orientation)
    : StaticBitmapImage(orientation), paint_image_(std::move(image)) {
  DCHECK(paint_image_);
}

UnacceleratedStaticBitmapImage::~UnacceleratedStaticBitmapImage() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!original_skia_image_)
    return;

  if (!original_skia_image_task_runner_->BelongsToCurrentThread()) {
    PostCrossThreadTask(
        *original_skia_image_task_runner_, FROM_HERE,
        CrossThreadBindOnce([](sk_sp<SkImage> image) { image.reset(); },
                            std::move(original_skia_image_)));
  } else {
    original_skia_image_.reset();
  }
}

bool UnacceleratedStaticBitmapImage::IsOpaque() {
  return paint_image_.IsOpaque();
}

void UnacceleratedStaticBitmapImage::Draw(
    cc::PaintCanvas* canvas,
    const cc::PaintFlags& flags,
    const gfx::RectF& dst_rect,
    const gfx::RectF& src_rect,
    const ImageDrawOptions& draw_options) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  StaticBitmapImage::DrawHelper(canvas, flags, dst_rect, src_rect, draw_options,
                                PaintImageForCurrentFrame());
}

PaintImage UnacceleratedStaticBitmapImage::PaintImageForCurrentFrame() {
  return paint_image_;
}

void UnacceleratedStaticBitmapImage::Transfer() {
  DETACH_FROM_THREAD(thread_checker_);

  original_skia_image_ = paint_image_.GetSwSkImage();
  original_skia_image_task_runner_ =
      ThreadScheduler::Current()->CleanupTaskRunner();
}

SkImageInfo UnacceleratedStaticBitmapImage::GetSkImageInfo() const {
  return paint_image_.GetSkImageInfo().makeWH(paint_image_.width(),
                                              paint_image_.height());
}

}  // namespace blink

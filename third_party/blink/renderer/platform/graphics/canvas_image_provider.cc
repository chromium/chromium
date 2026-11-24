// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/canvas_image_provider.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/viz/common/gpu/raster_context_provider.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"

namespace blink {

CanvasImageProvider::CanvasImageProvider(
    cc::ImageDecodeCache* cache,
    const gfx::ColorSpace& target_color_space,
    viz::SharedImageFormat canvas_format,
    cc::PlaybackImageProvider::RasterMode raster_mode,
    base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider_wrapper)
    : raster_mode_(raster_mode),
      context_provider_wrapper_(std::move(context_provider_wrapper)) {
  std::optional<cc::PlaybackImageProvider::Settings> settings =
      cc::PlaybackImageProvider::Settings();
  settings->raster_mode = raster_mode_;

  cc::TargetColorParams target_color_params;
  target_color_params.color_space = target_color_space;
  // If the canvas is half float, then have the cache store high bit depth
  // images as half float, instead of reducing them to uint8.
  if (canvas_format == viz::SinglePlaneFormat::kRGBA_F16) {
    target_color_params.preserve_image_precision = true;
  }
  playback_image_provider_.emplace(cache, target_color_params,
                                   std::move(settings));
}

CanvasImageProvider::~CanvasImageProvider() = default;

void CanvasImageProvider::UnbindTextureBackedImages() {
  for (auto& image : bound_texture_backed_images_) {
    DCHECK(image.IsTextureBacked());
    image.UnbindTextureBacking();
  }
  bound_texture_backed_images_.clear();
}

void CanvasImageProvider::SetAnimatedImageFrameIndexes(
    scoped_refptr<const cc::AnimatedImageFrameIndexMap> indexes) {
  playback_image_provider_->SetAnimatedImageFrameIndexes(indexes);
}

cc::ImageProvider::ScopedResult CanvasImageProvider::GetRasterContent(
    const cc::DrawImage& draw_image) {
  cc::PaintImage paint_image = draw_image.paint_image();
  CHECK(!paint_image.IsDeferredPaintRecord());

  // Bind texture backing to RasterContextProvider if necessary
  if (paint_image.IsTextureBacked()) {
    if (context_provider_wrapper_) {
      paint_image.BindTextureBacking(
          base::MakeRefCounted<viz::RasterContextProviderWrapper>(
              context_provider_wrapper_->ContextProvider()
                  .RasterContextProvider()));
      bound_texture_backed_images_.emplace_back(paint_image);
    }
  }

  ImageProvider::ScopedResult scoped_decoded_image =
      playback_image_provider_->GetRasterContent(draw_image);

  if (!scoped_decoded_image.needs_unlock() || !IsHardwareDecodeCache()) {
    return scoped_decoded_image;
  }

  constexpr int kMaxLockedImagesCount = 500;
  if (!scoped_decoded_image.decoded_image().is_budgeted() ||
      locked_images_.size() > kMaxLockedImagesCount) {
    ReleaseLockedImages();
  }

  auto decoded_draw_image = scoped_decoded_image.decoded_image();
  return ScopedResult(decoded_draw_image,
                      base::BindOnce(&CanvasImageProvider::CanUnlockImage,
                                     weak_factory_.GetWeakPtr(),
                                     std::move(scoped_decoded_image)));
}

void CanvasImageProvider::CanUnlockImage(ScopedResult image) {
  DCHECK(IsHardwareDecodeCache());

  if (!cleanup_task_pending_) {
    cleanup_task_pending_ = true;
    ThreadScheduler::Current()->CleanupTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&CanvasImageProvider::CleanupLockedImages,
                                  weak_factory_.GetWeakPtr()));
  }

  locked_images_.push_back(std::move(image));
}

void CanvasImageProvider::CleanupLockedImages() {
  cleanup_task_pending_ = false;
  ReleaseLockedImages();
}

bool CanvasImageProvider::IsHardwareDecodeCache() const {
  return raster_mode_ != cc::PlaybackImageProvider::RasterMode::kSoftware;
}

}  // namespace blink

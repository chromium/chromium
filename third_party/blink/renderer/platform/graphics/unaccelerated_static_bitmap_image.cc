// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"

#include "base/process/memory.h"
#include "components/viz/common/gpu/context_provider.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_graphics_context_3d_provider.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/web_graphics_context_3d_provider_wrapper.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_skia.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

scoped_refptr<UnacceleratedStaticBitmapImage>
UnacceleratedStaticBitmapImage::Create(sk_sp<SkImage> image,
                                       ImageOrientation orientation) {
  if (!image)
    return nullptr;
  DCHECK(!image->isTextureBacked());
  return base::AdoptRef(
      new UnacceleratedStaticBitmapImage(std::move(image), orientation));
}

UnacceleratedStaticBitmapImage::UnacceleratedStaticBitmapImage(
    sk_sp<SkImage> image,
    ImageOrientation orientation)
    : StaticBitmapImage(orientation) {
  CHECK(image);
  DCHECK(!image->isLazyGenerated());
  paint_image_ =
      CreatePaintImageBuilder()
          .set_image(std::move(image), cc::PaintImage::GetNextContentId())
          .TakePaintImage();
}

scoped_refptr<UnacceleratedStaticBitmapImage>
UnacceleratedStaticBitmapImage::Create(PaintImage image,
                                       ImageOrientation orientation) {
  return base::AdoptRef(
      new UnacceleratedStaticBitmapImage(std::move(image), orientation));
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
  auto image = PaintImageForCurrentFrame();
  if (image.may_be_lcp_candidate() != draw_options.may_be_lcp_candidate) {
    image = PaintImageBuilder::WithCopy(std::move(image))
                .set_may_be_lcp_candidate(draw_options.may_be_lcp_candidate)
                .TakePaintImage();
  }
  StaticBitmapImage::DrawHelper(canvas, flags, dst_rect, src_rect, draw_options,
                                image);
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

bool UnacceleratedStaticBitmapImage::CopyToResourceProvider(
    CanvasNon2DResourceProviderSharedImage* resource_provider,
    uint32_t src_x,
    uint32_t src_y) {
  DCHECK(resource_provider);

  // Extract content to SkPixmap. Pixels are CPU backed resource and this
  // should be freed.
  sk_sp<SkImage> image = paint_image_.GetSwSkImage();
  if (!image) {
    return false;
  }

  SkPixmap pixmap;
  if (!image->peekPixels(&pixmap)) {
    return false;
  }

  return resource_provider->WritePixels(pixmap, paint_image_.GetSkImageInfo(),
                                        src_x, src_y);
}

SkImageInfo UnacceleratedStaticBitmapImage::GetSkImageInfo() const {
  return paint_image_.GetSkImageInfo().makeWH(paint_image_.width(),
                                              paint_image_.height());
}

}  // namespace blink

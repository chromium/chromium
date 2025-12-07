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
#include "ui/gfx/skia_span_util.h"

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
    CanvasResourceProviderSharedImage* resource_provider,
    const gfx::Rect& copy_rect) {
  DCHECK(resource_provider);

  // Extract content to SkPixmap. Pixels are CPU backed resource and this
  // should be freed.
  sk_sp<SkImage> image = paint_image_.GetSwSkImage();
  if (!image)
    return false;

  SkPixmap pixmap;
  if (!image->peekPixels(&pixmap))
    return false;

  base::span<const uint8_t> pixels = gfx::SkPixmapToSpan(pixmap);
  const size_t source_row_bytes = pixmap.rowBytes();
  const size_t source_height = pixmap.height();

  SkImageInfo copy_rect_info = paint_image_.GetSkImageInfo().makeWH(
      copy_rect.width(), copy_rect.height());
  const size_t dest_row_bytes =
      copy_rect_info.bytesPerPixel() * static_cast<size_t>(copy_rect.width());
  const size_t dest_height = static_cast<size_t>(copy_rect.height());

  std::vector<uint8_t> dest_pixels;
  if (source_row_bytes != dest_row_bytes || source_height != dest_height) {
    dest_pixels.resize(dest_row_bytes * dest_height);

    const size_t x_offset_bytes =
        copy_rect_info.bytesPerPixel() * static_cast<size_t>(copy_rect.x());
    size_t src_offset = copy_rect.y() * source_row_bytes + x_offset_bytes;

    base::span<uint8_t> dest_data(dest_pixels);
    for (size_t dst_y = 0; dst_y < dest_height;
         ++dst_y, src_offset += source_row_bytes) {
      dest_data.take_first(dest_row_bytes)
          .copy_from(pixels.subspan(src_offset, dest_row_bytes));
    }
    pixels = dest_pixels;
  }

  return resource_provider->WritePixels(copy_rect_info, pixels.data(),
                                        dest_row_bytes,
                                        /*x=*/0, /*y=*/0);
}

SkImageInfo UnacceleratedStaticBitmapImage::GetSkImageInfo() const {
  return paint_image_.GetSkImageInfo().makeWH(paint_image_.width(),
                                              paint_image_.height());
}

}  // namespace blink

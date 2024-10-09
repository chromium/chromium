// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

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

bool UnacceleratedStaticBitmapImage::CurrentFrameKnownToBeOpaque() {
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

scoped_refptr<StaticBitmapImage>
UnacceleratedStaticBitmapImage::ConvertToColorSpace(
    sk_sp<SkColorSpace> color_space,
    SkColorType color_type) {
  DCHECK(color_space);

  sk_sp<SkImage> skia_image = PaintImageForCurrentFrame().GetSwSkImage();
  // If we don't need to change the color type, use SkImage::makeColorSpace()
  if (skia_image->colorType() == color_type) {
    skia_image = skia_image->makeColorSpace(
        static_cast<GrDirectContext*>(nullptr), color_space);
  } else {
    skia_image = skia_image->makeColorTypeAndColorSpace(
        static_cast<GrDirectContext*>(nullptr), color_type, color_space);
  }
  if (!skia_image) [[unlikely]] {
    // Null value indicates that skia failed to allocate the destination
    // bitmap.
    base::TerminateBecauseOutOfMemory(
        skia_image->imageInfo().makeColorType(color_type).computeMinByteSize());
  }
  return UnacceleratedStaticBitmapImage::Create(skia_image, orientation_);
}

bool UnacceleratedStaticBitmapImage::CopyToResourceProvider(
    CanvasResourceProvider* resource_provider,
    const gfx::Rect& copy_rect) {
  DCHECK(resource_provider);
  DCHECK(IsOriginTopLeft());

  // Extract content to SkPixmap. Pixels are CPU backed resource and this
  // should be freed.
  sk_sp<SkImage> image = paint_image_.GetSwSkImage();
  if (!image)
    return false;

  SkPixmap pixmap;
  if (!image->peekPixels(&pixmap))
    return false;

  const void* pixels = pixmap.addr();
  const size_t source_row_bytes = pixmap.rowBytes();
  const size_t source_height = pixmap.height();

  SkImageInfo copy_rect_info = paint_image_.GetSkImageInfo().makeWH(
      copy_rect.width(), copy_rect.height());
  const size_t dest_row_bytes =
      copy_rect_info.bytesPerPixel() * static_cast<size_t>(copy_rect.width());
  const size_t dest_height = static_cast<size_t>(copy_rect.height());

  // Source image has top left origin but destination resource provider doesn't.
  // Usually it means resource provider has bottom left origin. Apply flip op
  // on copy result to fix it.
  bool dest_flipped = !resource_provider->IsOriginTopLeft();

  std::vector<uint8_t> dest_pixels;
  if (dest_flipped || source_row_bytes != dest_row_bytes ||
      source_height != dest_height) {
    dest_pixels.resize(dest_row_bytes * dest_height);

    const size_t x_offset_bytes =
        copy_rect_info.bytesPerPixel() * static_cast<size_t>(copy_rect.x());
    const size_t y_offset = copy_rect.y();

    for (size_t dst_y = 0; dst_y < dest_height; ++dst_y) {
      size_t src_y = dest_flipped ? dest_height - dst_y - 1 : dst_y;
      memcpy(dest_pixels.data() + dst_y * dest_row_bytes,
             static_cast<const uint8_t*>(pixels) +
                 (y_offset + src_y) * source_row_bytes + x_offset_bytes,
             dest_row_bytes);
    }
    pixels = dest_pixels.data();
  }

  return resource_provider->WritePixels(copy_rect_info, pixels, dest_row_bytes,
                                        /*x=*/0, /*y=*/0);
}

SkImageInfo UnacceleratedStaticBitmapImage::GetSkImageInfo() const {
  return paint_image_.GetSkImageInfo().makeWH(paint_image_.width(),
                                              paint_image_.height());
}

}  // namespace blink

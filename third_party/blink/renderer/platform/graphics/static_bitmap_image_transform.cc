// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(https://crbug.com/40773069): The function FlipSkPixmapInPlace triggers
// unsafe buffer access warnings that were suppressed in the path it was moved
// from. Update the function to fix this issue.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/static_bitmap_image_transform.h"

#include <utility>

#include "gpu/command_buffer/client/shared_image_interface.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/canvas_resource_provider.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

namespace {

// Transformations of StaticBitmapImages have historically also converted them
// to kN32_SkColorType. This function very cautiously only lifts this
// restriction for StaticBitmapImages that are already kRGBA_F16_SkColorType.
// This caution is a response to issues such as the one described in
// https://crrev.com/1364046.
SkColorType GetDestColorType(SkColorType source_color_type) {
  if (source_color_type == kRGBA_F16_SkColorType) {
    return kRGBA_F16_SkColorType;
  }
  return kN32_SkColorType;
}

void FlipSkPixmapInPlace(SkPixmap& pm, bool horizontal) {
  uint8_t* data = reinterpret_cast<uint8_t*>(pm.writable_addr());
  const size_t row_bytes = pm.rowBytes();
  const size_t pixel_bytes = pm.info().bytesPerPixel();
  if (horizontal) {
    for (int i = 0; i < pm.height() - 1; i++) {
      for (int j = 0; j < pm.width() / 2; j++) {
        size_t first_element = i * row_bytes + j * pixel_bytes;
        size_t last_element = i * row_bytes + (j + 1) * pixel_bytes;
        size_t bottom_element = (i + 1) * row_bytes - (j + 1) * pixel_bytes;
        std::swap_ranges(&data[first_element], &data[last_element],
                         &data[bottom_element]);
      }
    }
  } else {
    for (int i = 0; i < pm.height() / 2; i++) {
      size_t top_first_element = i * row_bytes;
      size_t top_last_element = (i + 1) * row_bytes;
      size_t bottom_first_element = (pm.height() - 1 - i) * row_bytes;
      std::swap_ranges(&data[top_first_element], &data[top_last_element],
                       &data[bottom_first_element]);
    }
  }
}

// Return the effective orientation of `source`, which may have been
// overridden by `params`.
ImageOrientation GetSourceOrientation(
    scoped_refptr<StaticBitmapImage> source,
    const StaticBitmapImageTransform::Params& params) {
  if (!params.orientation_from_image) {
    return ImageOrientationEnum::kOriginTopLeft;
  }
  return source->CurrentFrameOrientation();
}

// Return the oriented size of `source`.
gfx::Size GetSourceSize(scoped_refptr<StaticBitmapImage> source,
                        const StaticBitmapImageTransform::Params& params) {
  const auto source_info = source->GetSkImageInfo();
  const auto source_orientation = GetSourceOrientation(source, params);

  return source_orientation.UsesWidthAsHeight()
             ? gfx::Size(source_info.height(), source_info.width())
             : gfx::Size(source_info.width(), source_info.height());
}

void ComputeSubsetParameters(scoped_refptr<StaticBitmapImage> source,
                             const StaticBitmapImageTransform::Params& params,
                             SkIRect& source_skrect,
                             SkIRect& source_skrect_valid,
                             SkISize& dest_sksize) {
  const gfx::Size source_size = GetSourceSize(source, params);
  const ImageOrientation source_orientation =
      GetSourceOrientation(source, params);
  gfx::Size unoriented_source_size = source_size;
  gfx::Size unoriented_dest_size = params.dest_size;
  if (source_orientation.UsesWidthAsHeight()) {
    unoriented_source_size = gfx::TransposeSize(unoriented_source_size);
    unoriented_dest_size = gfx::TransposeSize(unoriented_dest_size);
  }
  auto t = source_orientation.TransformFromDefault(
      gfx::SizeF(unoriented_source_size));
  const gfx::Rect source_rect_valid =
      gfx::IntersectRects(params.source_rect, gfx::Rect(source_size));

  const gfx::Rect unoriented_source_rect = t.MapRect(params.source_rect);
  const gfx::Rect unoriented_source_rect_valid = t.MapRect(source_rect_valid);

  source_skrect = gfx::RectToSkIRect(unoriented_source_rect);
  source_skrect_valid = gfx::RectToSkIRect(unoriented_source_rect_valid);
  dest_sksize = gfx::SizeToSkISize(unoriented_dest_size);
}

}  // namespace

// Perform the requested transformations on the CPU.
scoped_refptr<StaticBitmapImage> StaticBitmapImageTransform::ApplyUsingPixmap(
    scoped_refptr<StaticBitmapImage> source,
    const StaticBitmapImageTransform::Params& options) {
  auto source_paint_image = source->PaintImageForCurrentFrame();
  auto source_info = source->GetSkImageInfo();
  const auto source_orientation = GetSourceOrientation(source, options);

  // Compute the unoriented source and dest rects and sizes.
  SkIRect source_rect;
  SkIRect source_rect_valid;
  SkISize dest_size;
  ComputeSubsetParameters(source, options, source_rect, source_rect_valid,
                          dest_size);

  // Let `bm` be the image that we're manipulating step-by-step.
  SkBitmap bm;

  // Allocate the cropped source image.
  {
    SkAlphaType bm_alpha_type = source_info.alphaType();
    if (bm_alpha_type != kOpaque_SkAlphaType) {
      if (options.premultiply_alpha) {
        bm_alpha_type = kPremul_SkAlphaType;
      } else {
        bm_alpha_type = kUnpremul_SkAlphaType;
      }
    }
    const auto bm_color_space = options.dest_color_space
                                    ? options.dest_color_space
                                    : source_info.refColorSpace();
    const auto bm_info =
        source_info.makeDimensions(source_rect.size())
            .makeAlphaType(bm_alpha_type)
            .makeColorType(GetDestColorType(source_info.colorType()))
            .makeColorSpace(bm_color_space);
    if (!bm.tryAllocPixels(bm_info)) {
      return nullptr;
    }
  }

  // Populate the cropped image by calling `readPixels`. This can also do alpha
  // conversion.
  {
    // Let `pm_valid_rect` be the intersection of `source_rect` with
    // `source_size`. It will be a subset of `bm`, and we wil read into it.
    SkIRect pm_valid_rect = SkIRect::MakeXYWH(
        source_rect_valid.x() - source_rect.x(),
        source_rect_valid.y() - source_rect.y(), source_rect_valid.width(),
        source_rect_valid.height());
    SkPixmap pm_valid;
    if (!source_rect_valid.isEmpty() &&
        !bm.pixmap().extractSubset(&pm_valid, pm_valid_rect)) {
      NOTREACHED();
    }
    if (!source_rect_valid.isEmpty()) {
      if (!source_paint_image.readPixels(
              pm_valid.info(), pm_valid.writable_addr(), pm_valid.rowBytes(),
              source_rect_valid.x(), source_rect_valid.y())) {
        return nullptr;
      }
    }
  }

  // Apply scaling.
  if (bm.dimensions() != dest_size) {
    SkBitmap bm_scaled;
    if (!bm_scaled.tryAllocPixels(bm.info().makeDimensions(dest_size))) {
      return nullptr;
    }
    bm.pixmap().scalePixels(bm_scaled.pixmap(), options.sampling);
    bm = bm_scaled;
  }

  // Apply vertical flip by using a different ImageOrientation.
  if (options.flip_y) {
    SkPixmap pm = bm.pixmap();
    FlipSkPixmapInPlace(pm, source_orientation.UsesWidthAsHeight());
  }

  // Create the resulting SkImage.
  bm.setImmutable();
  auto dest_image = bm.asImage();

  // Strip the color space if requested.
  if (options.reinterpret_as_srgb) {
    dest_image = dest_image->reinterpretColorSpace(SkColorSpace::MakeSRGB());
  }

  // Return the result.
  auto dest_paint_image =
      PaintImageBuilder::WithDefault()
          .set_id(cc::PaintImage::GetNextId())
          .set_image(std::move(dest_image), cc::PaintImage::GetNextContentId())
          .TakePaintImage();
  return UnacceleratedStaticBitmapImage::Create(std::move(dest_paint_image),
                                                source_orientation);
}

// Perform all transformations using a blit, which will result in a new
// premultiplied-alpha result.
scoped_refptr<StaticBitmapImage> StaticBitmapImageTransform::ApplyWithBlit(
    FlushReason flush_reason,
    scoped_refptr<StaticBitmapImage> source,
    const StaticBitmapImageTransform::Params& options) {
  // This path will necessarily premultiply alpha.
  CHECK(options.premultiply_alpha);

  auto source_paint_image = source->PaintImageForCurrentFrame();
  const auto source_info = source_paint_image.GetSkImageInfo();
  const auto source_orientation = GetSourceOrientation(source, options);

  // Compute the parameters for the blit.
  const SkColorType dest_color_type = GetDestColorType(source_info.colorType());
  const SkAlphaType dest_alpha_type =
      source_info.alphaType() == kOpaque_SkAlphaType ? kOpaque_SkAlphaType
                                                     : kPremul_SkAlphaType;
  const auto dest_color_space = options.dest_color_space
                                    ? options.dest_color_space
                                    : source_info.refColorSpace();
  SkIRect source_rect;
  SkIRect source_rect_valid;
  SkISize dest_size;
  ComputeSubsetParameters(source, options, source_rect, source_rect_valid,
                          dest_size);

  // Create the resource provider for the target for the blit.
  std::unique_ptr<CanvasResourceProvider> resource_provider;
  {
    SkImageInfo dest_info = SkImageInfo::Make(
        dest_size, dest_color_type, dest_alpha_type, dest_color_space);
    constexpr auto kFilterQuality = cc::PaintFlags::FilterQuality::kLow;
    constexpr auto kShouldInitialize =
        CanvasResourceProvider::ShouldInitialize::kNo;
    // If `source` is accelerated, then use a SharedImage provider.
    if (source_paint_image.IsTextureBacked()) {
      base::WeakPtr<WebGraphicsContext3DProviderWrapper> context_provider =
          source->ContextProviderWrapper();
      if (context_provider) {
        const gpu::SharedImageUsageSet shared_image_usage_flags =
            context_provider->ContextProvider()
                ->SharedImageInterface()
                ->UsageForMailbox(source->GetMailboxHolder().mailbox);
        resource_provider = CanvasResourceProvider::CreateSharedImageProvider(
            dest_info, kFilterQuality, kShouldInitialize, context_provider,
            RasterMode::kGPU, shared_image_usage_flags);
      }
    }
    // If not (or if the SharedImage provider fails), fall back to software.
    if (!resource_provider) {
      resource_provider = CanvasResourceProvider::CreateBitmapProvider(
          dest_info, kFilterQuality, kShouldInitialize);
    }
  }

  // Perform the blit and return the drawn resource.
  cc::PaintFlags paint;
  paint.setBlendMode(SkBlendMode::kSrc);
  cc::PaintCanvas& canvas = resource_provider->Canvas();
  if (options.flip_y) {
    if (source_orientation.UsesWidthAsHeight()) {
      canvas.translate(dest_size.width(), 0);
      canvas.scale(-1, 1);
    } else {
      canvas.translate(0, dest_size.height());
      canvas.scale(1, -1);
    }
  }
  canvas.drawImageRect(source_paint_image, SkRect::Make(source_rect),
                       SkRect::Make(dest_size), options.sampling, &paint,
                       SkCanvas::kStrict_SrcRectConstraint);
  return resource_provider->Snapshot(flush_reason, source_orientation);
}

// Apply the transformations indicated in `options` on `source`, and return the
// result. When possible, this will avoid creating a new object and backing,
// unless `force_copy` is specified, in which case it will always create a new
// object and backing.
scoped_refptr<StaticBitmapImage> StaticBitmapImageTransform::Apply(
    FlushReason flush_reason,
    scoped_refptr<StaticBitmapImage> source,
    const StaticBitmapImageTransform::Params& options) {
  // It's not obvious what `reinterpret_as_srgb` should mean if we also specify
  // `dest_color_space`. Don't try to give an answer.
  if (options.dest_color_space) {
    CHECK(!options.reinterpret_as_srgb);
  }

  // Early-out for empty transformations.
  if (!source || options.source_rect.IsEmpty() || options.dest_size.IsEmpty()) {
    return nullptr;
  }

  const auto source_info = source->GetSkImageInfo();
  const bool needs_flip = options.flip_y;
  const bool needs_crop =
      options.source_rect != gfx::Rect(GetSourceSize(source, options));
  const bool needs_resize = options.source_rect.size() != options.dest_size;
  const bool needs_strip_orientation = !options.orientation_from_image;
  const bool needs_strip_color_space = options.reinterpret_as_srgb;
  const bool needs_convert_color_space =
      options.dest_color_space &&
      !SkColorSpace::Equals(options.dest_color_space.get(),
                            source_info.colorSpace()
                                ? source_info.colorSpace()
                                : SkColorSpace::MakeSRGB().get());
  const bool needs_alpha_change =
      (source->GetSkImageInfo().alphaType() == kUnpremul_SkAlphaType) !=
      (!options.premultiply_alpha);

  // If we aren't doing anything (and this wasn't a forced copy), just return
  // the original.
  if (!options.force_copy && !needs_flip && !needs_crop && !needs_resize &&
      !needs_strip_orientation && !needs_strip_color_space &&
      !needs_convert_color_space && !needs_alpha_change) {
    return source;
  }

  // Using a blit will premultiply content, so if unpremultiplied results are
  // requested, fall back to software. The test ImageBitmapTest.AvoidGPUReadback
  // expects this, even if the source had premultiplied alpha (in which case we
  // are falling back to the CPU for no increased precision).
  scoped_refptr<StaticBitmapImage> result;
  if (!options.premultiply_alpha) {
    return ApplyUsingPixmap(source, options);
  }
  return ApplyWithBlit(flush_reason, source, options);
}

scoped_refptr<StaticBitmapImage> StaticBitmapImageTransform::Clone(
    FlushReason flush_reason,
    scoped_refptr<StaticBitmapImage> source) {
  if (!source) {
    return nullptr;
  }
  const auto info = source->GetSkImageInfo();
  StaticBitmapImageTransform::Params options;
  options.source_rect = gfx::Rect(GetSourceSize(source, options));
  options.dest_size = GetSourceSize(source, options);
  options.premultiply_alpha = info.alphaType() != kUnpremul_SkAlphaType;
  options.force_copy = true;
  return Apply(flush_reason, source, options);
}

scoped_refptr<StaticBitmapImage>
StaticBitmapImageTransform::GetWithAlphaDisposition(
    FlushReason flush_reason,
    scoped_refptr<StaticBitmapImage> source,
    AlphaDisposition alpha_disposition) {
  if (!source) {
    return nullptr;
  }
  switch (alpha_disposition) {
    case kPremultiplyAlpha:
      break;
    case kDontChangeAlpha:
      return source;
  }
  const auto info = source->GetSkImageInfo();
  StaticBitmapImageTransform::Params options;
  options.source_rect = gfx::Rect(GetSourceSize(source, options));
  options.dest_size = GetSourceSize(source, options);
  options.premultiply_alpha = true;
  return Apply(flush_reason, source, options);
}

scoped_refptr<StaticBitmapImage>
StaticBitmapImageTransform::ConvertToColorSpace(
    FlushReason flush_reason,
    scoped_refptr<StaticBitmapImage> source,
    sk_sp<SkColorSpace> color_space) {
  const auto info = source->GetSkImageInfo();
  StaticBitmapImageTransform::Params options;
  options.source_rect = gfx::Rect(GetSourceSize(source, options));
  options.dest_size = GetSourceSize(source, options);
  options.premultiply_alpha = info.alphaType() != kUnpremul_SkAlphaType;
  options.force_copy = true;
  options.dest_color_space = color_space;
  return Apply(flush_reason, source, options);
}

}  // namespace blink

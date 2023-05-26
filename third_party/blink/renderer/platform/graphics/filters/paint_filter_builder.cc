/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"

#include "third_party/blink/renderer/platform/graphics/box_reflection.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_record.h"
#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {
namespace paint_filter_builder {

void PopulateSourceGraphicImageFilters(
    FilterEffect* source_graphic,
    InterpolationSpace input_interpolation_space) {
  // Prepopulate SourceGraphic with two image filters: one with a null image
  // filter, and the other with a colorspace conversion filter.
  // We don't know what color space the interior nodes will request, so we
  // have to initialize SourceGraphic with both options.
  // Since we know SourceGraphic is always PM-valid, we also use these for
  // the PM-validated options.
  sk_sp<PaintFilter> device_filter = TransformInterpolationSpace(
      nullptr, input_interpolation_space, kInterpolationSpaceSRGB);
  sk_sp<PaintFilter> linear_filter = TransformInterpolationSpace(
      nullptr, input_interpolation_space, kInterpolationSpaceLinear);
  source_graphic->SetImageFilter(kInterpolationSpaceSRGB, false, device_filter);
  source_graphic->SetImageFilter(kInterpolationSpaceLinear, false,
                                 linear_filter);
  source_graphic->SetImageFilter(kInterpolationSpaceSRGB, true, device_filter);
  source_graphic->SetImageFilter(kInterpolationSpaceLinear, true,
                                 linear_filter);
}

sk_sp<PaintFilter> Build(
    FilterEffect* effect,
    InterpolationSpace interpolation_space,
    bool destination_requires_valid_pre_multiplied_pixels) {
  if (!effect)
    return nullptr;

  bool requires_pm_color_validation =
      effect->MayProduceInvalidPreMultipliedPixels() &&
      destination_requires_valid_pre_multiplied_pixels;

  if (PaintFilter* filter = effect->GetImageFilter(
          interpolation_space, requires_pm_color_validation))
    return sk_ref_sp(filter);

  // Note that we may still need the color transform even if the filter is null
  sk_sp<PaintFilter> orig_filter =
      requires_pm_color_validation
          ? effect->CreateImageFilter()
          : effect->CreateImageFilterWithoutValidation();

  sk_sp<PaintFilter> filter = TransformInterpolationSpace(
      orig_filter, effect->OperatingInterpolationSpace(), interpolation_space);
  effect->SetImageFilter(interpolation_space, requires_pm_color_validation,
                         filter);
  if (filter.get() != orig_filter.get()) {
    effect->SetImageFilter(effect->OperatingInterpolationSpace(),
                           requires_pm_color_validation,
                           std::move(orig_filter));
  }
  return filter;
}

sk_sp<PaintFilter> TransformInterpolationSpace(
    sk_sp<PaintFilter> input,
    InterpolationSpace src_interpolation_space,
    InterpolationSpace dst_interpolation_space) {
  sk_sp<cc::ColorFilter> color_filter =
      interpolation_space_utilities::CreateInterpolationSpaceFilter(
          src_interpolation_space, dst_interpolation_space);
  if (!color_filter)
    return input;

  return sk_make_sp<ColorFilterPaintFilter>(std::move(color_filter),
                                            std::move(input));
}

static const float kMaxMaskBufferSize =
    50.f * 1024.f * 1024.f / 4.f;  // 50MB / 4 bytes per pixel

sk_sp<PaintFilter> BuildBoxReflectFilter(const BoxReflection& reflection,
                                         sk_sp<PaintFilter> input) {
  sk_sp<PaintFilter> masked_input;
  PaintRecord mask_record = reflection.Mask();
  if (!mask_record.empty()) {
    // Since PaintRecords can't be serialized to the browser process, first
    // raster the mask to a bitmap, then encode it in an SkImageSource, which
    // can be serialized.
    SkBitmap bitmap;
    const SkRect mask_record_bounds =
        gfx::RectFToSkRect(reflection.MaskBounds());
    SkRect mask_bounds_rounded;
    mask_record_bounds.roundOut(&mask_bounds_rounded);
    SkScalar mask_buffer_size =
        mask_bounds_rounded.width() * mask_bounds_rounded.height();
    if (mask_buffer_size < kMaxMaskBufferSize && mask_buffer_size > 0.0f) {
      bitmap.allocPixels(SkImageInfo::MakeN32Premul(
          mask_bounds_rounded.width(), mask_bounds_rounded.height()));
      SkiaPaintCanvas canvas(bitmap);
      canvas.clear(SkColors::kTransparent);
      canvas.translate(-mask_record_bounds.x(), -mask_record_bounds.y());
      canvas.drawPicture(std::move(mask_record));
      PaintImage image = PaintImageBuilder::WithDefault()
                             .set_id(PaintImage::GetNextId())
                             .set_image(SkImages::RasterFromBitmap(bitmap),
                                        PaintImage::GetNextContentId())
                             .TakePaintImage();

      // SkXfermodeImageFilter can choose an excessively large size if the
      // mask is smaller than the filtered contents (due to overflow).
      // http://skbug.com/5210
      PaintFilter::CropRect crop_rect(mask_record_bounds);
      SkRect image_rect = SkRect::MakeWH(image.width(), image.height());
      masked_input = sk_make_sp<XfermodePaintFilter>(
          SkBlendMode::kSrcIn,
          sk_make_sp<OffsetPaintFilter>(
              mask_record_bounds.x(), mask_record_bounds.y(),
              sk_make_sp<ImagePaintFilter>(
                  std::move(image), image_rect, image_rect,
                  cc::PaintFlags::FilterQuality::kHigh)),
          input, &crop_rect);
    } else {
      // If the buffer is excessively big, give up and make an
      // SkPictureImageFilter anyway, even if it might not render.
      PaintFilter::CropRect crop_rect(mask_record_bounds);
      masked_input = sk_make_sp<XfermodePaintFilter>(
          SkBlendMode::kSrcOver,
          sk_make_sp<RecordPaintFilter>(std::move(mask_record),
                                        mask_record_bounds),
          input, &crop_rect);
    }
  } else {
    masked_input = input;
  }
  sk_sp<PaintFilter> flip_image_filter = sk_make_sp<MatrixPaintFilter>(
      reflection.ReflectionMatrix(), cc::PaintFlags::FilterQuality::kLow,
      std::move(masked_input));
  return sk_make_sp<XfermodePaintFilter>(
      SkBlendMode::kSrcOver, std::move(flip_image_filter), std::move(input));
}

}  // namespace paint_filter_builder
}  // namespace blink

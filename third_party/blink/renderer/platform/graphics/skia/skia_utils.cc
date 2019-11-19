/*
 * Copyright (c) 2006,2007,2008, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/skia/skia_utils.h"

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "build/build_config.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_flags.h"
#include "third_party/blink/renderer/platform/wtf/allocator/partitions.h"
#include "third_party/skia/include/effects/SkCornerPathEffect.h"
#include "third_party/skia/include/third_party/skcms/skcms.h"

#include <algorithm>
#include <cmath>

namespace blink {

SkBlendMode WebCoreCompositeToSkiaComposite(CompositeOperator op,
                                            BlendMode blend_mode) {
  if (blend_mode != BlendMode::kNormal) {
    DCHECK(op == kCompositeSourceOver);
    return WebCoreBlendModeToSkBlendMode(blend_mode);
  }

  switch (op) {
    case kCompositeClear:
      return SkBlendMode::kClear;
    case kCompositeCopy:
      return SkBlendMode::kSrc;
    case kCompositeSourceOver:
      return SkBlendMode::kSrcOver;
    case kCompositeSourceIn:
      return SkBlendMode::kSrcIn;
    case kCompositeSourceOut:
      return SkBlendMode::kSrcOut;
    case kCompositeSourceAtop:
      return SkBlendMode::kSrcATop;
    case kCompositeDestinationOver:
      return SkBlendMode::kDstOver;
    case kCompositeDestinationIn:
      return SkBlendMode::kDstIn;
    case kCompositeDestinationOut:
      return SkBlendMode::kDstOut;
    case kCompositeDestinationAtop:
      return SkBlendMode::kDstATop;
    case kCompositeXOR:
      return SkBlendMode::kXor;
    case kCompositePlusLighter:
      return SkBlendMode::kPlus;
  }

  NOTREACHED();
  return SkBlendMode::kSrcOver;
}

SkBlendMode WebCoreBlendModeToSkBlendMode(BlendMode blend_mode) {
  switch (blend_mode) {
    case BlendMode::kNormal:
      return SkBlendMode::kSrcOver;
    case BlendMode::kMultiply:
      return SkBlendMode::kMultiply;
    case BlendMode::kScreen:
      return SkBlendMode::kScreen;
    case BlendMode::kOverlay:
      return SkBlendMode::kOverlay;
    case BlendMode::kDarken:
      return SkBlendMode::kDarken;
    case BlendMode::kLighten:
      return SkBlendMode::kLighten;
    case BlendMode::kColorDodge:
      return SkBlendMode::kColorDodge;
    case BlendMode::kColorBurn:
      return SkBlendMode::kColorBurn;
    case BlendMode::kHardLight:
      return SkBlendMode::kHardLight;
    case BlendMode::kSoftLight:
      return SkBlendMode::kSoftLight;
    case BlendMode::kDifference:
      return SkBlendMode::kDifference;
    case BlendMode::kExclusion:
      return SkBlendMode::kExclusion;
    case BlendMode::kHue:
      return SkBlendMode::kHue;
    case BlendMode::kSaturation:
      return SkBlendMode::kSaturation;
    case BlendMode::kColor:
      return SkBlendMode::kColor;
    case BlendMode::kLuminosity:
      return SkBlendMode::kLuminosity;
  }

  NOTREACHED();
  return SkBlendMode::kSrcOver;
}

CompositeOperator CompositeOperatorFromSkBlendMode(SkBlendMode blend_mode) {
  switch (blend_mode) {
    case SkBlendMode::kClear:
      return kCompositeClear;
    case SkBlendMode::kSrc:
      return kCompositeCopy;
    case SkBlendMode::kSrcOver:
      return kCompositeSourceOver;
    case SkBlendMode::kSrcIn:
      return kCompositeSourceIn;
    case SkBlendMode::kSrcOut:
      return kCompositeSourceOut;
    case SkBlendMode::kSrcATop:
      return kCompositeSourceAtop;
    case SkBlendMode::kDstOver:
      return kCompositeDestinationOver;
    case SkBlendMode::kDstIn:
      return kCompositeDestinationIn;
    case SkBlendMode::kDstOut:
      return kCompositeDestinationOut;
    case SkBlendMode::kDstATop:
      return kCompositeDestinationAtop;
    case SkBlendMode::kXor:
      return kCompositeXOR;
    case SkBlendMode::kPlus:
      return kCompositePlusLighter;
    default:
      break;
  }
  return kCompositeSourceOver;
}

BlendMode BlendModeFromSkBlendMode(SkBlendMode blend_mode) {
  switch (blend_mode) {
    case SkBlendMode::kSrcOver:
      return BlendMode::kNormal;
    case SkBlendMode::kMultiply:
      return BlendMode::kMultiply;
    case SkBlendMode::kScreen:
      return BlendMode::kScreen;
    case SkBlendMode::kOverlay:
      return BlendMode::kOverlay;
    case SkBlendMode::kDarken:
      return BlendMode::kDarken;
    case SkBlendMode::kLighten:
      return BlendMode::kLighten;
    case SkBlendMode::kColorDodge:
      return BlendMode::kColorDodge;
    case SkBlendMode::kColorBurn:
      return BlendMode::kColorBurn;
    case SkBlendMode::kHardLight:
      return BlendMode::kHardLight;
    case SkBlendMode::kSoftLight:
      return BlendMode::kSoftLight;
    case SkBlendMode::kDifference:
      return BlendMode::kDifference;
    case SkBlendMode::kExclusion:
      return BlendMode::kExclusion;
    case SkBlendMode::kHue:
      return BlendMode::kHue;
    case SkBlendMode::kSaturation:
      return BlendMode::kSaturation;
    case SkBlendMode::kColor:
      return BlendMode::kColor;
    case SkBlendMode::kLuminosity:
      return BlendMode::kLuminosity;
    default:
      break;
  }
  return BlendMode::kNormal;
}

SkMatrix AffineTransformToSkMatrix(const AffineTransform& source) {
  SkMatrix result;

  result.setScaleX(WebCoreDoubleToSkScalar(source.A()));
  result.setSkewX(WebCoreDoubleToSkScalar(source.C()));
  result.setTranslateX(WebCoreDoubleToSkScalar(source.E()));

  result.setScaleY(WebCoreDoubleToSkScalar(source.D()));
  result.setSkewY(WebCoreDoubleToSkScalar(source.B()));
  result.setTranslateY(WebCoreDoubleToSkScalar(source.F()));

  // FIXME: Set perspective properly.
  result.setPerspX(0);
  result.setPerspY(0);
  result.set(SkMatrix::kMPersp2, SK_Scalar1);

  return result;
}

bool NearlyIntegral(float value) {
  return fabs(value - floorf(value)) < std::numeric_limits<float>::epsilon();
}

bool IsValidImageSize(const IntSize& size) {
  if (size.IsEmpty())
    return false;
  base::CheckedNumeric<int> area = size.Width();
  area *= size.Height();
  if (!area.IsValid() || area.ValueOrDie() > kMaxCanvasArea)
    return false;
  if (size.Width() > kMaxSkiaDim || size.Height() > kMaxSkiaDim)
    return false;
  return true;
}

InterpolationQuality ComputeInterpolationQuality(float src_width,
                                                 float src_height,
                                                 float dest_width,
                                                 float dest_height,
                                                 bool is_data_complete) {
  // The percent change below which we will not resample. This usually means
  // an off-by-one error on the web page, and just doing nearest neighbor
  // sampling is usually good enough.
  const float kFractionalChangeThreshold = 0.025f;

  // Images smaller than this in either direction are considered "small" and
  // are not resampled ever (see below).
  const int kSmallImageSizeThreshold = 8;

  // The amount an image can be stretched in a single direction before we
  // say that it is being stretched so much that it must be a line or
  // background that doesn't need resampling.
  const float kLargeStretch = 3.0f;

  // Figure out if we should resample this image. We try to prune out some
  // common cases where resampling won't give us anything, since it is much
  // slower than drawing stretched.
  float diff_width = fabs(dest_width - src_width);
  float diff_height = fabs(dest_height - src_height);
  bool width_nearly_equal = diff_width < std::numeric_limits<float>::epsilon();
  bool height_nearly_equal =
      diff_height < std::numeric_limits<float>::epsilon();
  // We don't need to resample if the source and destination are the same.
  if (width_nearly_equal && height_nearly_equal)
    return kInterpolationNone;

  if (src_width <= kSmallImageSizeThreshold ||
      src_height <= kSmallImageSizeThreshold ||
      dest_width <= kSmallImageSizeThreshold ||
      dest_height <= kSmallImageSizeThreshold) {
    // Small image detected.

    // Resample in the case where the new size would be non-integral.
    // This can cause noticeable breaks in repeating patterns, except
    // when the source image is only one pixel wide in that dimension.
    if ((!NearlyIntegral(dest_width) &&
         src_width > 1 + std::numeric_limits<float>::epsilon()) ||
        (!NearlyIntegral(dest_height) &&
         src_height > 1 + std::numeric_limits<float>::epsilon()))
      return kInterpolationLow;

    // Otherwise, don't resample small images. These are often used for
    // borders and rules (think 1x1 images used to make lines).
    return kInterpolationNone;
  }

  if (src_height * kLargeStretch <= dest_height ||
      src_width * kLargeStretch <= dest_width) {
    // Large image detected.

    // Don't resample if it is being stretched a lot in only one direction.
    // This is trying to catch cases where somebody has created a border
    // (which might be large) and then is stretching it to fill some part
    // of the page.
    if (width_nearly_equal || height_nearly_equal)
      return kInterpolationNone;

    // The image is growing a lot and in more than one direction. Resampling
    // is slow and doesn't give us very much when growing a lot.
    return kInterpolationLow;
  }

  if ((diff_width / src_width < kFractionalChangeThreshold) &&
      (diff_height / src_height < kFractionalChangeThreshold)) {
    // It is disappointingly common on the web for image sizes to be off by
    // one or two pixels. We don't bother resampling if the size difference
    // is a small fraction of the original size.
    return kInterpolationNone;
  }

  // When the image is not yet done loading, use linear. We don't cache the
  // partially resampled images, and as they come in incrementally, it causes
  // us to have to resample the whole thing every time.
  if (!is_data_complete)
    return kInterpolationLow;

  // Everything else gets resampled at default quality.
  return kInterpolationDefault;
}

SkColor ScaleAlpha(SkColor color, float alpha) {
  const auto clamped_alpha = std::max(0.0f, std::min(1.0f, alpha));
  const auto rounded_alpha = std::lround(SkColorGetA(color) * clamped_alpha);

  return SkColorSetA(color, rounded_alpha);
}

bool ApproximatelyEqualSkColorSpaces(sk_sp<SkColorSpace> src_color_space,
                                     sk_sp<SkColorSpace> dst_color_space) {
  if ((!src_color_space && dst_color_space) ||
      (src_color_space && !dst_color_space))
    return false;
  if (!src_color_space && !dst_color_space)
    return true;
  skcms_ICCProfile src_profile, dst_profile;
  src_color_space->toProfile(&src_profile);
  dst_color_space->toProfile(&dst_profile);
  return skcms_ApproximatelyEqualProfiles(&src_profile, &dst_profile);
}

SkRect LayoutRectToSkRect(const blink::LayoutRect& rect) {
  return SkRect::MakeXYWH(SkFloatToScalar(rect.X()), SkFloatToScalar(rect.Y()),
                          SkFloatToScalar(rect.Width()),
                          SkFloatToScalar(rect.Height()));
}

template <typename PrimitiveType>
void DrawFocusRingPrimitive(const PrimitiveType&,
                            cc::PaintCanvas*,
                            const PaintFlags&,
                            float corner_radius) {
  NOTREACHED();  // Missing an explicit specialization?
}

template <>
void DrawFocusRingPrimitive<SkRect>(const SkRect& rect,
                                    cc::PaintCanvas* canvas,
                                    const PaintFlags& flags,
                                    float corner_radius) {
  SkRRect rrect;
  rrect.setRectXY(rect, SkFloatToScalar(corner_radius),
                  SkFloatToScalar(corner_radius));
  canvas->drawRRect(rrect, flags);
}

template <>
void DrawFocusRingPrimitive<SkPath>(const SkPath& path,
                                    cc::PaintCanvas* canvas,
                                    const PaintFlags& flags,
                                    float corner_radius) {
  PaintFlags path_flags = flags;
  path_flags.setPathEffect(
      SkCornerPathEffect::Make(SkFloatToScalar(corner_radius)));
  canvas->drawPath(path, path_flags);
}

template <typename PrimitiveType>
void DrawPlatformFocusRing(const PrimitiveType& primitive,
                           cc::PaintCanvas* canvas,
                           SkColor color,
                           float width) {
  PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(PaintFlags::kStroke_Style);
  flags.setColor(color);
  flags.setStrokeWidth(width);

#if defined(OS_MACOSX)
  flags.setAlpha(64);
  const float corner_radius = (width - 1) * 0.5f;
#else
  const float corner_radius = width;
#endif

  DrawFocusRingPrimitive(primitive, canvas, flags, corner_radius);

#if defined(OS_MACOSX)
  // Inner part
  flags.setAlpha(128);
  flags.setStrokeWidth(flags.getStrokeWidth() * 0.5f);
  DrawFocusRingPrimitive(primitive, canvas, flags, corner_radius);
#endif
}

template void PLATFORM_EXPORT DrawPlatformFocusRing<SkRect>(const SkRect&,
                                                            cc::PaintCanvas*,
                                                            SkColor,
                                                            float width);
template void PLATFORM_EXPORT DrawPlatformFocusRing<SkPath>(const SkPath&,
                                                            cc::PaintCanvas*,
                                                            SkColor,
                                                            float width);

sk_sp<SkData> TryAllocateSkData(size_t size) {
  void* buffer = WTF::Partitions::BufferPartition()->AllocFlags(
      base::PartitionAllocReturnNull | base::PartitionAllocZeroFill, size,
      "SkData");
  if (!buffer)
    return nullptr;
  return SkData::MakeWithProc(
      buffer, size,
      [](const void* buffer, void* context) {
        WTF::Partitions::BufferPartition()->Free(const_cast<void*>(buffer));
      },
      /*context=*/nullptr);
}

}  // namespace blink

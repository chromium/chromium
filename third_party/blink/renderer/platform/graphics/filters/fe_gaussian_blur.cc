/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Igalia, S.L.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/platform/graphics/filters/fe_gaussian_blur.h"

#include "base/types/optional_util.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"

namespace blink {

namespace {

inline unsigned ApproximateBoxWidth(float s) {
  return static_cast<unsigned>(
      floorf(s * (3 / 4.f * sqrtf(kTwoPiFloat)) + 0.5f));
}

gfx::Size CalculateKernelSize(const gfx::SizeF& std) {
  DCHECK(std.width() >= 0 && std.height() >= 0);
  gfx::Size kernel_size;
  if (std.width()) {
    int size = std::max<unsigned>(2, ApproximateBoxWidth(std.width()));
    kernel_size.set_width(size);
  }
  if (std.height()) {
    int size = std::max<unsigned>(2, ApproximateBoxWidth(std.height()));
    kernel_size.set_height(size);
  }
  return kernel_size;
}
}

FEGaussianBlur::FEGaussianBlur(Filter* filter, float x, float y)
    : FilterEffect(filter), std_x_(x), std_y_(y) {}

gfx::RectF FEGaussianBlur::MapEffect(const gfx::SizeF& std_deviation,
                                     const gfx::RectF& rect) {
  gfx::Size kernel_size = CalculateKernelSize(std_deviation);
  // We take the half kernel size and multiply it by three, because we run box
  // blur three times.
  gfx::RectF result = rect;
  result.Outset(gfx::OutsetsF::VH(3.0f * kernel_size.height() * 0.5f,
                                  3.0f * kernel_size.width() * 0.5f));

  return result;
}

gfx::RectF FEGaussianBlur::MapEffect(const gfx::RectF& rect) const {
  gfx::SizeF std_error(GetFilter()->ApplyHorizontalScale(std_x_),
                       GetFilter()->ApplyVerticalScale(std_y_));
  return MapEffect(std_error, rect);
}

sk_sp<PaintFilter> FEGaussianBlur::CreateImageFilter() {
  sk_sp<PaintFilter> input(paint_filter_builder::Build(
      InputEffect(0), OperatingInterpolationSpace()));
  float std_x = GetFilter()->ApplyHorizontalScale(std_x_);
  float std_y = GetFilter()->ApplyVerticalScale(std_y_);
  std::optional<PaintFilter::CropRect> crop_rect = GetCropRect();
  return sk_make_sp<BlurPaintFilter>(
      SkFloatToScalar(std_x), SkFloatToScalar(std_y), SkTileMode::kDecal,
      std::move(input), base::OptionalToPtr(crop_rect));
}

WTF::TextStream& FEGaussianBlur::ExternalRepresentation(WTF::TextStream& ts,
                                                        int indent) const {
  WriteIndent(ts, indent);
  ts << "[feGaussianBlur";
  FilterEffect::ExternalRepresentation(ts);
  ts << " stdDeviation=\"" << std_x_ << ", " << std_y_ << "\"]\n";
  InputEffect(0)->ExternalRepresentation(ts, indent + 1);
  return ts;
}

}  // namespace blink

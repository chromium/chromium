/*
 * Copyright (C) Research In Motion Limited 2011. All rights reserved.
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

#include "third_party/blink/renderer/platform/graphics/filters/fe_drop_shadow.h"

#include "base/types/optional_util.h"
#include "third_party/blink/renderer/platform/graphics/filters/fe_gaussian_blur.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/blink/renderer/platform/graphics/filters/paint_filter_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/text_stream.h"

namespace blink {

FEDropShadow::FEDropShadow(Filter* filter,
                           float std_x,
                           float std_y,
                           float dx,
                           float dy,
                           const Color& shadow_color,
                           float shadow_opacity)
    : FilterEffect(filter),
      std_x_(std_x),
      std_y_(std_y),
      dx_(dx),
      dy_(dy),
      shadow_color_(shadow_color),
      shadow_opacity_(shadow_opacity) {}

gfx::RectF FEDropShadow::MapEffect(const gfx::SizeF& std_deviation,
                                   const gfx::Vector2dF& offset,
                                   const gfx::RectF& rect) {
  gfx::RectF offset_rect = rect;
  offset_rect.Offset(offset);
  gfx::RectF blurred_rect =
      FEGaussianBlur::MapEffect(std_deviation, offset_rect);
  return gfx::UnionRects(blurred_rect, rect);
}

gfx::RectF FEDropShadow::MapEffect(const gfx::RectF& rect) const {
  const Filter* filter = GetFilter();
  DCHECK(filter);
  gfx::Vector2dF offset(filter->ApplyHorizontalScale(dx_),
                        filter->ApplyVerticalScale(dy_));
  gfx::SizeF std_error(filter->ApplyHorizontalScale(std_x_),
                       filter->ApplyVerticalScale(std_y_));
  return MapEffect(std_error, offset, rect);
}

sk_sp<PaintFilter> FEDropShadow::CreateImageFilter() {
  sk_sp<PaintFilter> input(paint_filter_builder::Build(
      InputEffect(0), OperatingInterpolationSpace()));
  float dx = GetFilter()->ApplyHorizontalScale(dx_);
  float dy = GetFilter()->ApplyVerticalScale(dy_);
  float std_x = GetFilter()->ApplyHorizontalScale(std_x_);
  float std_y = GetFilter()->ApplyVerticalScale(std_y_);
  Color drop_shadow_color = shadow_color_;
  drop_shadow_color.SetAlpha(shadow_opacity_ * drop_shadow_color.Alpha());
  drop_shadow_color =
      AdaptColorToOperatingInterpolationSpace(drop_shadow_color);
  std::optional<PaintFilter::CropRect> crop_rect = GetCropRect();
  return sk_make_sp<DropShadowPaintFilter>(
      SkFloatToScalar(dx), SkFloatToScalar(dy), SkFloatToScalar(std_x),
      SkFloatToScalar(std_y), drop_shadow_color.toSkColor4f(),
      DropShadowPaintFilter::ShadowMode::kDrawShadowAndForeground,
      std::move(input), base::OptionalToPtr(crop_rect));
}

WTF::TextStream& FEDropShadow::ExternalRepresentation(WTF::TextStream& ts,
                                                      int indent) const {
  WriteIndent(ts, indent);
  ts << "[feDropShadow";
  FilterEffect::ExternalRepresentation(ts);
  ts << " stdDeviation=\"" << std_x_ << ", " << std_y_ << "\" dx=\"" << dx_
     << "\" dy=\"" << dy_ << "\" flood-color=\""
     << shadow_color_.NameForLayoutTreeAsText() << "\" flood-opacity=\""
     << shadow_opacity_ << "]\n";
  InputEffect(0)->ExternalRepresentation(ts, indent + 1);
  return ts;
}

}  // namespace blink

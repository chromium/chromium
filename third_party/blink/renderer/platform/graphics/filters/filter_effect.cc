/*
 * Copyright (C) 2008 Alex Mathews <possessedpenguinbob@gmail.com>
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2012 University of Szeged
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

#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"

#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkColorFilterImageFilter.h"

namespace blink {

FilterEffect::FilterEffect(Filter* filter)
    : filter_(filter),
      clips_to_bounds_(true),
      origin_tainted_(false),
      operating_interpolation_space_(kInterpolationSpaceLinear) {
  DCHECK(filter_);
}

FilterEffect::~FilterEffect() = default;

void FilterEffect::Trace(blink::Visitor* visitor) {
  visitor->Trace(input_effects_);
  visitor->Trace(filter_);
}

FloatRect FilterEffect::AbsoluteBounds() const {
  FloatRect computed_bounds = GetFilter()->FilterRegion();
  if (!FilterPrimitiveSubregion().IsEmpty())
    computed_bounds.Intersect(FilterPrimitiveSubregion());
  return GetFilter()->MapLocalRectToAbsoluteRect(computed_bounds);
}

FloatRect FilterEffect::MapInputs(const FloatRect& rect) const {
  if (!input_effects_.size()) {
    if (ClipsToBounds())
      return AbsoluteBounds();
    return rect;
  }
  FloatRect input_union;
  for (const auto& effect : input_effects_)
    input_union.Unite(effect->MapRect(rect));
  return input_union;
}

FloatRect FilterEffect::MapEffect(const FloatRect& rect) const {
  return rect;
}

FloatRect FilterEffect::ApplyBounds(const FloatRect& rect) const {
  // Filters in SVG clip to primitive subregion, while CSS doesn't.
  if (!ClipsToBounds())
    return rect;
  FloatRect bounds = AbsoluteBounds();
  if (AffectsTransparentPixels())
    return bounds;
  return Intersection(rect, bounds);
}

FloatRect FilterEffect::MapRect(const FloatRect& rect) const {
  FloatRect result = MapInputs(rect);
  result = MapEffect(result);
  return ApplyBounds(result);
}

FilterEffect* FilterEffect::InputEffect(unsigned number) const {
  SECURITY_DCHECK(number < input_effects_.size());
  return input_effects_.at(number).Get();
}

void FilterEffect::DisposeImageFilters() {
  for (int i = 0; i < 4; i++)
    image_filters_[i] = nullptr;
}

void FilterEffect::DisposeImageFiltersRecursive() {
  if (!HasImageFilter())
    return;
  DisposeImageFilters();
  for (auto& effect : input_effects_)
    effect->DisposeImageFiltersRecursive();
}

Color FilterEffect::AdaptColorToOperatingInterpolationSpace(
    const Color& device_color) {
  // |deviceColor| is assumed to be DeviceRGB.
  return interpolation_space_utilities::ConvertColor(
      device_color, OperatingInterpolationSpace());
}

WTF::TextStream& FilterEffect::ExternalRepresentation(WTF::TextStream& ts,
                                                      int) const {
  // FIXME: We should dump the subRegions of the filter primitives here later.
  // This isn't possible at the moment, because we need more detailed
  // information from the target object.
  return ts;
}

sk_sp<PaintFilter> FilterEffect::CreateImageFilter() {
  return nullptr;
}

sk_sp<PaintFilter> FilterEffect::CreateImageFilterWithoutValidation() {
  return CreateImageFilter();
}

bool FilterEffect::InputsTaintOrigin() const {
  for (const Member<FilterEffect>& effect : input_effects_) {
    if (effect->OriginTainted())
      return true;
  }
  return false;
}

sk_sp<PaintFilter> FilterEffect::CreateTransparentBlack() const {
  PaintFilter::CropRect rect = GetCropRect();
  sk_sp<SkColorFilter> color_filter =
      SkColorFilters::Blend(0, SkBlendMode::kClear);
  return sk_make_sp<ColorFilterPaintFilter>(std::move(color_filter), nullptr,
                                            &rect);
}

PaintFilter::CropRect FilterEffect::GetCropRect() const {
  if (!FilterPrimitiveSubregion().IsEmpty()) {
    FloatRect rect =
        GetFilter()->MapLocalRectToAbsoluteRect(FilterPrimitiveSubregion());
    return PaintFilter::CropRect(rect);
  } else {
    return PaintFilter::CropRect(SkRect::MakeEmpty(), 0);
  }
}

static int GetImageFilterIndex(InterpolationSpace interpolation_space,
                               bool requires_pm_color_validation) {
  // Map the (colorspace, bool) tuple to an integer index as follows:
  // 0 == linear colorspace, no PM validation
  // 1 == device colorspace, no PM validation
  // 2 == linear colorspace, PM validation
  // 3 == device colorspace, PM validation
  return (interpolation_space == kInterpolationSpaceLinear ? 0x1 : 0x0) |
         (requires_pm_color_validation ? 0x2 : 0x0);
}

PaintFilter* FilterEffect::GetImageFilter(
    InterpolationSpace interpolation_space,
    bool requires_pm_color_validation) const {
  int index =
      GetImageFilterIndex(interpolation_space, requires_pm_color_validation);
  return image_filters_[index].get();
}

void FilterEffect::SetImageFilter(InterpolationSpace interpolation_space,
                                  bool requires_pm_color_validation,
                                  sk_sp<PaintFilter> image_filter) {
  int index =
      GetImageFilterIndex(interpolation_space, requires_pm_color_validation);
  image_filters_[index] = std::move(image_filter);
}

}  // namespace blink

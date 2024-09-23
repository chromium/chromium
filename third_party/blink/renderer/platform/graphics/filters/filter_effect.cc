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

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"

#include "base/types/optional_util.h"
#include "third_party/blink/renderer/platform/graphics/filters/filter.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace blink {

FilterEffect::FilterEffect(Filter* filter)
    : filter_(filter),
      clips_to_bounds_(true),
      origin_tainted_(false),
      operating_interpolation_space_(kInterpolationSpaceLinear) {
  DCHECK(filter_);
}

FilterEffect::~FilterEffect() = default;

void FilterEffect::Trace(Visitor* visitor) const {
  visitor->Trace(input_effects_);
  visitor->Trace(filter_);
}

gfx::RectF FilterEffect::AbsoluteBounds() const {
  gfx::RectF computed_bounds = GetFilter()->FilterRegion();
  if (!FilterPrimitiveSubregion().IsEmpty())
    computed_bounds.Intersect(FilterPrimitiveSubregion());
  return GetFilter()->MapLocalRectToAbsoluteRect(computed_bounds);
}

gfx::RectF FilterEffect::MapInputs(const gfx::RectF& rect) const {
  if (!input_effects_.size()) {
    if (ClipsToBounds())
      return AbsoluteBounds();
    return rect;
  }
  gfx::RectF input_union;
  for (const auto& effect : input_effects_)
    input_union.Union(effect->MapRect(rect));
  return input_union;
}

gfx::RectF FilterEffect::MapEffect(const gfx::RectF& rect) const {
  return rect;
}

gfx::RectF FilterEffect::ApplyBounds(const gfx::RectF& rect) const {
  // Filters in SVG clip to primitive subregion, while CSS doesn't.
  if (!ClipsToBounds())
    return rect;
  gfx::RectF bounds = AbsoluteBounds();
  if (AffectsTransparentPixels())
    return bounds;
  return IntersectRects(rect, bounds);
}

gfx::RectF FilterEffect::MapRect(const gfx::RectF& rect) const {
  gfx::RectF result = MapInputs(rect);
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
  sk_sp<cc::ColorFilter> color_filter =
      cc::ColorFilter::MakeBlend(SkColors::kBlack, SkBlendMode::kClear);
  return sk_make_sp<ColorFilterPaintFilter>(std::move(color_filter), nullptr,
                                            base::OptionalToPtr(GetCropRect()));
}

std::optional<PaintFilter::CropRect> FilterEffect::GetCropRect() const {
  if (!ClipsToBounds()) {
    return {};
  }
  gfx::RectF computed_bounds = FilterPrimitiveSubregion();
  // This and the filter region check is a workaround for crbug.com/512453.
  if (computed_bounds.IsEmpty()) {
    return {};
  }
  gfx::RectF filter_region = GetFilter()->FilterRegion();
  if (!filter_region.IsEmpty()) {
    computed_bounds.Intersect(filter_region);
  }
  return gfx::RectFToSkRect(
      GetFilter()->MapLocalRectToAbsoluteRect(computed_bounds));
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

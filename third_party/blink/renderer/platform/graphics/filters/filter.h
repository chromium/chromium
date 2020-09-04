/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FILTERS_FILTER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FILTERS_FILTER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/geometry/float_point_3d.h"
#include "third_party/blink/renderer/platform/geometry/float_rect.h"
#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace blink {

class SourceGraphic;
class FilterEffect;

class PLATFORM_EXPORT Filter final : public GarbageCollected<Filter> {

 public:
  enum UnitScaling { kUserSpace, kBoundingBox };

  Filter(float scale);
  Filter(const FloatRect& reference_box,
         const FloatRect& filter_region,
         float scale,
         UnitScaling);

  void Trace(Visitor*) const;

  float Scale() const { return scale_; }
  FloatRect MapLocalRectToAbsoluteRect(const FloatRect&) const;
  FloatRect MapAbsoluteRectToLocalRect(const FloatRect&) const;

  float ApplyHorizontalScale(float value) const;
  float ApplyVerticalScale(float value) const;

  FloatPoint3D Resolve3dPoint(const FloatPoint3D&) const;

  const FloatRect& FilterRegion() const { return filter_region_; }
  const FloatRect& ReferenceBox() const { return reference_box_; }

  void SetLastEffect(FilterEffect*);
  FilterEffect* LastEffect() const { return last_effect_.Get(); }

  SourceGraphic* GetSourceGraphic() const { return source_graphic_.Get(); }

 private:
  FloatRect reference_box_;
  FloatRect filter_region_;
  float scale_;
  UnitScaling unit_scaling_;

  Member<SourceGraphic> source_graphic_;
  Member<FilterEffect> last_effect_;

  DISALLOW_COPY_AND_ASSIGN(Filter);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_FILTERS_FILTER_H_

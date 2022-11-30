/*
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/graphics/filters/filter.h"

#include "third_party/blink/renderer/platform/graphics/filters/filter_effect.h"
#include "third_party/blink/renderer/platform/graphics/filters/source_graphic.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

Filter::Filter(float scale)
    : Filter(gfx::RectF(), gfx::RectF(), scale, kUserSpace) {}

Filter::Filter(const gfx::RectF& reference_box,
               const gfx::RectF& filter_region,
               float scale,
               UnitScaling unit_scaling)
    : reference_box_(reference_box),
      filter_region_(filter_region),
      scale_(scale),
      unit_scaling_(unit_scaling),
      source_graphic_(MakeGarbageCollected<SourceGraphic>(this)) {}

void Filter::Trace(Visitor* visitor) const {
  visitor->Trace(source_graphic_);
  visitor->Trace(last_effect_);
}

gfx::RectF Filter::MapLocalRectToAbsoluteRect(const gfx::RectF& rect) const {
  return gfx::ScaleRect(rect, scale_);
}

gfx::RectF Filter::MapAbsoluteRectToLocalRect(const gfx::RectF& rect) const {
  return gfx::ScaleRect(rect, 1.0f / scale_);
}

float Filter::ApplyHorizontalScale(float value) const {
  if (unit_scaling_ == kBoundingBox)
    value *= ReferenceBox().width();
  return scale_ * value;
}

float Filter::ApplyVerticalScale(float value) const {
  if (unit_scaling_ == kBoundingBox)
    value *= ReferenceBox().height();
  return scale_ * value;
}

gfx::Point3F Filter::Resolve3dPoint(gfx::Point3F point) const {
  if (unit_scaling_ == kBoundingBox) {
    point = gfx::Point3F(
        point.x() * ReferenceBox().width() + ReferenceBox().x(),
        point.y() * ReferenceBox().height() + ReferenceBox().y(),
        point.z() * sqrtf(gfx::Vector2dF(ReferenceBox().size().width(),
                                         ReferenceBox().size().height())
                              .LengthSquared() /
                          2));
  }
  return gfx::ScalePoint(point, scale_);
}

void Filter::SetLastEffect(FilterEffect* effect) {
  last_effect_ = effect;
}

}  // namespace blink

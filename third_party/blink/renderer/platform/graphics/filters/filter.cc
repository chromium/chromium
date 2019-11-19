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
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

Filter::Filter(float scale)
    : Filter(FloatRect(), FloatRect(), scale, kUserSpace) {}

Filter::Filter(const FloatRect& reference_box,
               const FloatRect& filter_region,
               float scale,
               UnitScaling unit_scaling)
    : reference_box_(reference_box),
      filter_region_(filter_region),
      scale_(scale),
      unit_scaling_(unit_scaling),
      source_graphic_(MakeGarbageCollected<SourceGraphic>(this)) {}

void Filter::Trace(blink::Visitor* visitor) {
  visitor->Trace(source_graphic_);
  visitor->Trace(last_effect_);
}

FloatRect Filter::MapLocalRectToAbsoluteRect(const FloatRect& rect) const {
  FloatRect result(rect);
  result.Scale(scale_);
  return result;
}

FloatRect Filter::MapAbsoluteRectToLocalRect(const FloatRect& rect) const {
  FloatRect result(rect);
  result.Scale(1.0f / scale_);
  return result;
}

float Filter::ApplyHorizontalScale(float value) const {
  if (unit_scaling_ == kBoundingBox)
    value *= ReferenceBox().Width();
  return scale_ * value;
}

float Filter::ApplyVerticalScale(float value) const {
  if (unit_scaling_ == kBoundingBox)
    value *= ReferenceBox().Height();
  return scale_ * value;
}

FloatPoint3D Filter::Resolve3dPoint(const FloatPoint3D& point) const {
  if (unit_scaling_ != kBoundingBox)
    return point;
  return FloatPoint3D(
      point.X() * ReferenceBox().Width() + ReferenceBox().X(),
      point.Y() * ReferenceBox().Height() + ReferenceBox().Y(),
      point.Z() * sqrtf(ReferenceBox().Size().DiagonalLengthSquared() / 2));
}

void Filter::SetLastEffect(FilterEffect* effect) {
  last_effect_ = effect;
}

}  // namespace blink

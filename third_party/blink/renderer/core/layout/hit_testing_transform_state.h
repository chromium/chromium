/*
 * Copyright (C) 2011 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_HIT_TESTING_TRANSFORM_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_HIT_TESTING_TRANSFORM_STATE_H_

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// FIXME: Now that TransformState lazily creates its TransformationMatrix it
// takes up less space.
// So there's really no need for a ref counted version. So This class should be
// removed and replaced with TransformState. There are some minor differences
// (like the way translate() works slightly differently than move()) so care has
// to be taken when this is done.
class HitTestingTransformState {
  STACK_ALLOCATED();

 public:
  HitTestingTransformState(const FloatPoint& p,
                           const FloatQuad& quad,
                           const FloatQuad& area)
      : last_planar_point_(p),
        last_planar_quad_(quad),
        last_planar_area_(area),
        accumulating_transform_(false) {}

  HitTestingTransformState(const HitTestingTransformState& other)
      : last_planar_point_(other.last_planar_point_),
        last_planar_quad_(other.last_planar_quad_),
        last_planar_area_(other.last_planar_area_),
        accumulated_transform_(other.accumulated_transform_),
        accumulating_transform_(other.accumulating_transform_) {}

  enum TransformAccumulation { kFlattenTransform, kAccumulateTransform };
  void Translate(int x, int y, TransformAccumulation);
  void ApplyTransform(const TransformationMatrix& transform_from_container,
                      TransformAccumulation);

  FloatPoint MappedPoint() const;
  FloatQuad MappedQuad() const;
  FloatQuad MappedArea() const;
  PhysicalRect BoundsOfMappedQuad() const;
  PhysicalRect BoundsOfMappedArea() const;
  void Flatten();

  FloatPoint last_planar_point_;
  FloatQuad last_planar_quad_;
  FloatQuad last_planar_area_;
  TransformationMatrix accumulated_transform_;
  bool accumulating_transform_;

 private:

  void FlattenWithTransform(const TransformationMatrix&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_HIT_TESTING_TRANSFORM_STATE_H_

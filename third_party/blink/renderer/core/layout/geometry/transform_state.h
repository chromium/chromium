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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_TRANSFORM_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_TRANSFORM_STATE_H_

#include <memory>
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_offset.h"
#include "third_party/blink/renderer/platform/geometry/float_point.h"
#include "third_party/blink/renderer/platform/geometry/float_quad.h"
#include "third_party/blink/renderer/platform/geometry/int_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_point.h"
#include "third_party/blink/renderer/platform/geometry/layout_size.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Accumulates transforms/offsets across multiple coordinate spaces.
// This is mainly used by other layout geometry mapping functions/classes (e.g.
// LayoutObject::LocalToAncestorPoint() and LayoutGeometryMap). In most cases
// other code should not use this class directly.
class CORE_EXPORT TransformState {
  STACK_ALLOCATED();

 public:
  enum TransformDirection {
    kApplyTransformDirection,
    kUnapplyInverseTransformDirection
  };
  enum TransformAccumulation { kFlattenTransform, kAccumulateTransform };

  TransformState(TransformDirection mapping_direction,
                 const FloatPoint& p,
                 const FloatQuad& quad)
      : last_planar_point_(p),
        last_planar_quad_(quad),
        accumulating_transform_(false),
        force_accumulating_transform_(false),
        map_point_(true),
        map_quad_(true),
        direction_(mapping_direction) {}

  TransformState(TransformDirection mapping_direction, const FloatPoint& p)
      : last_planar_point_(p),
        accumulating_transform_(false),
        force_accumulating_transform_(false),
        map_point_(true),
        map_quad_(false),
        direction_(mapping_direction) {}

  TransformState(TransformDirection mapping_direction, const FloatQuad& quad)
      : last_planar_quad_(quad),
        accumulating_transform_(false),
        force_accumulating_transform_(false),
        map_point_(false),
        map_quad_(true),
        direction_(mapping_direction) {}

  // Accumulate a transform but don't map any points directly.
  TransformState(TransformDirection mapping_direction)
      : accumulated_transform_(std::make_unique<TransformationMatrix>()),
        accumulating_transform_(true),
        force_accumulating_transform_(true),
        map_point_(false),
        map_quad_(false),
        direction_(mapping_direction) {}

  TransformState(const TransformState& other) { *this = other; }

  TransformState& operator=(const TransformState&);

  // Note: this overrides the quad and ignores any accumulatedOffset.
  // If it's desired to include the offset, call flatten() first.
  void SetQuad(const FloatQuad& quad) {
    DCHECK(!accumulating_transform_);
    // FIXME: this assumes that the quad being added is in the coordinate system
    // of the current state.  This breaks if we're simultaneously mapping a
    // point.  https://bugs.webkit.org/show_bug.cgi?id=106680
    DCHECK(!map_point_);
    accumulated_offset_ = PhysicalOffset();
    last_planar_quad_ = quad;
  }

  void Move(const PhysicalOffset& offset,
            TransformAccumulation accumulate = kFlattenTransform);
  void ApplyTransform(const AffineTransform& transform_from_container,
                      TransformAccumulation = kFlattenTransform);
  void ApplyTransform(const TransformationMatrix& transform_from_container,
                      TransformAccumulation = kFlattenTransform);
  void Flatten();

  // Return the coords of the point or quad in the last flattened layer
  FloatPoint LastPlanarPoint() const { return last_planar_point_; }
  FloatQuad LastPlanarQuad() const { return last_planar_quad_; }

  // Return the point or quad mapped through the current transform
  PhysicalOffset MappedPoint() const;
  FloatQuad MappedQuad() const;

  // Return the accumulated transform.
  const TransformationMatrix& AccumulatedTransform() const;

 private:
  void TranslateTransform(const PhysicalOffset&);
  void TranslateMappedCoordinates(const PhysicalOffset&);
  void FlattenWithTransform(const TransformationMatrix&);
  void ApplyAccumulatedOffset();

  FloatPoint last_planar_point_;
  FloatQuad last_planar_quad_;

  // We only allocate the transform if we need to
  std::unique_ptr<TransformationMatrix> accumulated_transform_;
  PhysicalOffset accumulated_offset_;
  bool accumulating_transform_;
  bool force_accumulating_transform_;
  bool map_point_, map_quad_;
  TransformDirection direction_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_GEOMETRY_TRANSFORM_STATE_H_

/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SHAPES_ELLIPSE_SHAPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SHAPES_ELLIPSE_SHAPE_H_

#include "base/check_op.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/shapes/shape.h"
#include "ui/gfx/geometry/point_f.h"

namespace blink {

class CORE_EXPORT EllipseShape final : public Shape {
 public:
  EllipseShape(const gfx::PointF& center,
               float radius_inline,
               float radius_block)
      : center_(center),
        radius_inline_(radius_inline),
        radius_block_(radius_block) {
    DCHECK_GE(radius_inline, 0);
    DCHECK_GE(radius_block, 0);
  }

  LogicalRect ShapeMarginLogicalBoundingBox() const override;
  bool IsEmpty() const override { return !radius_inline_ || !radius_block_; }
  LineSegment GetExcludedInterval(LayoutUnit logical_top,
                                  LayoutUnit logical_height) const override;
  void BuildDisplayPaths(DisplayPaths&) const override;

 private:
  // Returns a pair of the inline-axis radius and the block-axis radius.
  // They contains the ShapeMargin() value.
  std::pair<float, float> InlineAndBlockRadiiIncludingMargin() const;

  // The center point in a logical coordinate.
  gfx::PointF center_;
  // Radius in the inline direction.
  float radius_inline_;
  // Radius in the block direction.
  float radius_block_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SHAPES_ELLIPSE_SHAPE_H_

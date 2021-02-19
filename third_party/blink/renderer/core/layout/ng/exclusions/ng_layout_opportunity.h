// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_EXCLUSIONS_NG_LAYOUT_OPPORTUNITY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_EXCLUSIONS_NG_LAYOUT_OPPORTUNITY_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_line_layout_opportunity.h"
#include "third_party/blink/renderer/core/layout/ng/exclusions/ng_shape_exclusions.h"
#include "third_party/blink/renderer/core/layout/ng/geometry/ng_bfc_rect.h"

namespace blink {

class NGConstraintSpace;

// This struct represents an 2D-area where a NGFragment can fit within the
// exclusion space. A layout opportunity is produced by the exclusion space by
// calling FindLayoutOpportunity, or AllLayoutOpportunities.
//
// Its coordinates are relative to the BFC.
struct CORE_EXPORT NGLayoutOpportunity {
  NGLayoutOpportunity()
      : rect(NGBfcOffset(LayoutUnit::Min(), LayoutUnit::Min()),
             NGBfcOffset(LayoutUnit::Max(), LayoutUnit::Max())) {}
  NGLayoutOpportunity(
      const NGBfcRect& rect,
      scoped_refptr<const NGShapeExclusions> shape_exclusions = nullptr)
      : rect(rect), shape_exclusions(std::move(shape_exclusions)) {}

  // Rectangle in BFC coordinates that represents this opportunity.
  NGBfcRect rect;

  // The shape exclusions hold all of the adjacent exclusions which may affect
  // the line layout opportunity when queried. May be null if no shapes are
  // present.
  scoped_refptr<const NGShapeExclusions> shape_exclusions;

  // Returns if the opportunity has any shapes which may affect a line layout
  // opportunity.
  bool HasShapeExclusions() const { return shape_exclusions.get(); }

  // Returns if the given delta (relative to the start of the opportunity) will
  // be below any shapes.
  bool IsBlockDeltaBelowShapes(LayoutUnit block_delta) const;

  // Calculates a line layout opportunity which takes into account any shapes
  // which may affect the available inline size for the line breaker.
  NGLineLayoutOpportunity ComputeLineLayoutOpportunity(
      const NGConstraintSpace& space,
      LayoutUnit line_block_size,
      LayoutUnit block_delta) const {
    return NGLineLayoutOpportunity(
        ComputeLineLeftOffset(space, line_block_size, block_delta),
        ComputeLineRightOffset(space, line_block_size, block_delta),
        rect.LineStartOffset(), rect.LineEndOffset(),
        rect.BlockStartOffset() + block_delta, line_block_size);
  }

 private:
  LayoutUnit ComputeLineLeftOffset(const NGConstraintSpace&,
                                   LayoutUnit line_block_size,
                                   LayoutUnit block_delta) const;
  LayoutUnit ComputeLineRightOffset(const NGConstraintSpace&,
                                    LayoutUnit line_block_size,
                                    LayoutUnit block_delta) const;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_EXCLUSIONS_NG_LAYOUT_OPPORTUNITY_H_

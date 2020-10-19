// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ABSOLUTE_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ABSOLUTE_UTILS_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class LayoutObject;
class NGBlockNode;
class NGConstraintSpace;
struct NGLogicalStaticPosition;

struct CORE_EXPORT NGLogicalOutOfFlowDimensions {
  NGBoxStrut inset;
  LogicalSize size = {kIndefiniteSize, kIndefiniteSize};
  NGBoxStrut margins;
};

// Implements <dialog> static positioning.
//
// Returns new dialog top position if layout_dialog requires <dialog>
// OOF-positioned centering.
CORE_EXPORT base::Optional<LayoutUnit> ComputeAbsoluteDialogYPosition(
    const LayoutObject& layout_dialog,
    LayoutUnit height);

// The following routines implement the absolute size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-width
//
// The size is computed as |NGLogicalOutOfFlowDimensions|.
// It needs to be computed in 4 stages:
// 1. If |AbsoluteNeedsChildInlineSize| is true, compute estimated inline_size
//    using |NGBlockNode::ComputeMinMaxSize|.
// 2. Compute part of the |NGLogicalOutOfFlowDimensions| which depends on the
//    child inline-size with |ComputeOutOfFlowInlineDimensions|.
// 3. If |AbsoluteNeedsChildBlockSize| is true, compute estimated block_size by
//    performing layout with the inline_size calculated from (2).
// 4. Compute the full |NGLogicalOutOfFlowDimensions| with
//    |ComputeOutOfFlowBlockDimensions|.

// Returns true if |ComputeOutOfFlowInlineDimensions| will need an estimated
// inline-size.
CORE_EXPORT bool AbsoluteNeedsChildInlineSize(const NGBlockNode&);

// Returns true if |ComputeOutOfFlowBlockDimensions| will need an estimated
// block-size.
CORE_EXPORT bool AbsoluteNeedsChildBlockSize(const NGBlockNode&);

// Returns true if the inline size can be computed from an aspect ratio and
// the block size.
bool IsInlineSizeComputableFromBlockSize(const NGBlockNode&);

// Computes part of the absolute position which depends on the child's
// inline-size.
// |minmax_intrinsic_size_for_ar| is only used for min-inline-size: auto in
// combination with aspect-ratio.
// |replaced_size| should be set if and only if element is replaced element.
// Returns the partially filled position.
CORE_EXPORT void ComputeOutOfFlowInlineDimensions(
    const NGBlockNode&,
    const NGConstraintSpace&,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition&,
    const base::Optional<MinMaxSizes>& minmax_content_sizes,
    const base::Optional<MinMaxSizes>& minmax_intrinsic_sizes_for_ar,
    const base::Optional<LogicalSize>& replaced_size,
    const WritingDirectionMode container_writing_direction,
    NGLogicalOutOfFlowDimensions* dimensions);

// Computes the rest of the absolute position which depends on child's
// block-size.
CORE_EXPORT void ComputeOutOfFlowBlockDimensions(
    const NGBlockNode&,
    const NGConstraintSpace&,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition&,
    const base::Optional<LayoutUnit>& child_block_size,
    const base::Optional<LogicalSize>& replaced_size,
    const WritingDirectionMode container_writing_direction,
    NGLogicalOutOfFlowDimensions* dimensions);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ABSOLUTE_UTILS_H_

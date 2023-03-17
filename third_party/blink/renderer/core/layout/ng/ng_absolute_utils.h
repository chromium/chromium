// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ABSOLUTE_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ABSOLUTE_UTILS_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/core/layout/min_max_sizes.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class NGBoxFragmentBuilder;
class NGBlockNode;
class NGConstraintSpace;
class NGLayoutResult;
struct NGLogicalStaticPosition;

struct CORE_EXPORT NGLogicalOutOfFlowDimensions {
  LayoutUnit MarginBoxInlineStart() const {
    return inset.inline_start - margins.inline_start;
  }
  LayoutUnit MarginBoxBlockStart() const {
    return inset.block_start - margins.block_start;
  }
  LayoutUnit MarginBoxInlineEnd() const {
    return inset.inline_start + size.inline_size + margins.inline_end;
  }
  LayoutUnit MarginBoxBlockEnd() const {
    return inset.block_start + size.block_size + margins.block_end;
  }

  NGBoxStrut inset;
  LogicalSize size = {kIndefiniteSize, kIndefiniteSize};
  NGBoxStrut margins;
};

struct CORE_EXPORT NGLogicalOutOfFlowInsets {
  absl::optional<LayoutUnit> inline_start;
  absl::optional<LayoutUnit> inline_end;
  absl::optional<LayoutUnit> block_start;
  absl::optional<LayoutUnit> block_end;
};

CORE_EXPORT NGLogicalOutOfFlowInsets
ComputeOutOfFlowInsets(const ComputedStyle& style,
                       const LogicalSize& available_size,
                       NGAnchorEvaluatorImpl* anchor_evaluator);

// Computes the inset-modified containing block without the final step of
// clamping negative sizes to zero.
// https://www.w3.org/TR/css-position-3/#inset-modified-containing-block
CORE_EXPORT LogicalRect
ComputeOutOfFlowAvailableRect(const NGBlockNode&,
                              const NGConstraintSpace&,
                              const NGLogicalOutOfFlowInsets&,
                              const NGLogicalStaticPosition&);
CORE_EXPORT LogicalRect
ComputeOutOfFlowAvailableRect(const NGBlockNode&,
                              const LogicalSize& available_size,
                              const NGLogicalOutOfFlowInsets&,
                              const NGLogicalStaticPosition&);

// The following routines implement the absolute size resolution algorithm.
// https://www.w3.org/TR/css-position-3/#abs-non-replaced-width
//
// The size is computed as |NGLogicalOutOfFlowDimensions|.
// It needs to be computed in 2 stages:
// 1. The inline-dimensions with |ComputeOutOfFlowInlineDimensions|.
// 2. The block-dimensions with |ComputeOutOfFlowBlockDimensions|.
//
// NOTE: |ComputeOutOfFlowInlineDimensions| may call
// |ComputeOutOfFlowBlockDimensions| if its required to correctly determine the
// min/max content sizes.

// |replaced_size| should be set if and only if element is replaced element.
// Will return true if |NGBlockNode::ComputeMinMaxSizes| was called.
CORE_EXPORT bool ComputeOutOfFlowInlineDimensions(
    const NGBlockNode&,
    const ComputedStyle& style,
    const NGConstraintSpace&,
    const NGLogicalOutOfFlowInsets&,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition&,
    LogicalSize computed_available_size,
    const absl::optional<LogicalSize>& replaced_size,
    const WritingDirectionMode container_writing_direction,
    const Length::AnchorEvaluator* anchor_evaluator,
    NGLogicalOutOfFlowDimensions* dimensions);

// If layout was performed to determine the position, this will be returned
// otherwise it will return nullptr.
CORE_EXPORT const NGLayoutResult* ComputeOutOfFlowBlockDimensions(
    const NGBlockNode&,
    const ComputedStyle& style,
    const NGConstraintSpace&,
    const NGLogicalOutOfFlowInsets&,
    const NGBoxStrut& border_padding,
    const NGLogicalStaticPosition&,
    LogicalSize computed_available_size,
    const absl::optional<LogicalSize>& replaced_size,
    const WritingDirectionMode container_writing_direction,
    const Length::AnchorEvaluator* anchor_evaluator,
    NGLogicalOutOfFlowDimensions* dimensions);

CORE_EXPORT void AdjustOffsetForSplitInline(
    const NGBlockNode& node,
    const NGBoxFragmentBuilder* container_builder,
    LogicalOffset& offset);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_NG_ABSOLUTE_UTILS_H_

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
struct LogicalStaticPosition;

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

  BoxStrut inset;
  LogicalSize size = {kIndefiniteSize, kIndefiniteSize};
  BoxStrut margins;
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

struct CORE_EXPORT InsetModifiedContainingBlock {
  // The original containing block size that the insets refer to.
  LogicalSize available_size;

  // Resolved insets of the IMCB.
  LayoutUnit inline_start;
  LayoutUnit inline_end;
  LayoutUnit block_start;
  LayoutUnit block_end;

  // Indicates how the insets were calculated. Besides, when we need to clamp
  // the IMCB size, the stronger inset (i.e., the inset we are biased towards)
  // stays at the same place, and the weaker inset is moved; If both insets are
  // equally strong, both are moved by the same amount.
  enum class InsetBias { kStart, kEnd, kEqual };
  InsetBias inline_inset_bias = InsetBias::kStart;
  InsetBias block_inset_bias = InsetBias::kStart;

  LayoutUnit InlineEndOffset() const {
    return available_size.inline_size - inline_end;
  }
  LayoutUnit BlockEndOffset() const {
    return available_size.block_size - block_end;
  }
  LayoutUnit InlineSize() const {
    return available_size.inline_size - inline_start - inline_end;
  }
  LayoutUnit BlockSize() const {
    return available_size.block_size - block_start - block_end;
  }
  LogicalSize Size() const { return LogicalSize(InlineSize(), BlockSize()); }
};

// Computes the inset-modified containing block for resolving size, margins and
// final position of the out-of-flow node.
// https://www.w3.org/TR/css-position-3/#inset-modified-containing-block
CORE_EXPORT InsetModifiedContainingBlock ComputeInsetModifiedContainingBlock(
    const NGBlockNode& node,
    const LogicalSize& available_size,
    const NGLogicalOutOfFlowInsets&,
    const LogicalStaticPosition&,
    const WritingDirectionMode& container_writing_direction,
    const WritingDirectionMode& self_writing_direction);

// Similar to `ComputeInsetModifiedContainingBlock`, but returns the
// scroll-adjusted IMCB at the initial scroll position, which is for the
// position fallback algorithm only.
// https://www.w3.org/TR/css-anchor-position-1/#fallback-apply
CORE_EXPORT InsetModifiedContainingBlock ComputeIMCBForPositionFallback(
    const LogicalSize& available_size,
    const NGLogicalOutOfFlowInsets&,
    const LogicalStaticPosition&,
    const WritingDirectionMode& container_writing_direction,
    const WritingDirectionMode& self_writing_direction);

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
    const InsetModifiedContainingBlock&,
    const BoxStrut& border_padding,
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
    const InsetModifiedContainingBlock&,
    const BoxStrut& border_padding,
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

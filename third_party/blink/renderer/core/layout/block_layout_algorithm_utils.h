// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BLOCK_LAYOUT_ALGORITHM_UTILS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BLOCK_LAYOUT_ALGORITHM_UTILS_H_

#include "third_party/blink/renderer/core/layout/geometry/static_position.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"

namespace blink {

class BlockBreakToken;
class BlockNode;
class BoxFragmentBuilder;
class ComputedStyle;
class ExclusionSpace;
class UseCounter;
struct BfcOffset;

// OOF-positioned nodes which were initially inline-level, however are in a
// block-level context, pretend they are in an inline-level context. E.g.
// they avoid floats, and respect text-align.
//
// This function calculates the inline-offset to avoid floats, and respect
// text-align.
//
// TODO(ikilpatrick): Move this back into block_layout_algorithm.cc
LayoutUnit CalculateOutOfFlowStaticInlineLevelOffset(
    const ComputedStyle& container_style,
    const BfcOffset& origin_bfc_offset,
    const ExclusionSpace&,
    LayoutUnit child_available_inline_size);

// Final result of merging `align-content` value and `vertical-align` value.
// This is only for boxes with `display: block` and `display: table-cell`.
enum class BlockContentAlignment {
  kStart,
  kBaseline,
  kSafeCenter,
  kUnsafeCenter,
  kSafeEnd,
  kUnsafeEnd
};
BlockContentAlignment ComputeContentAlignmentForBlock(
    const ComputedStyle& style,
    UseCounter* use_counter = nullptr);
BlockContentAlignment ComputeContentAlignmentForTableCell(
    const ComputedStyle& style,
    UseCounter* use_counter = nullptr);

void AlignBlockContent(const ComputedStyle& style,
                       const BlockBreakToken* break_token,
                       LayoutUnit content_block_size,
                       BoxFragmentBuilder& builder);

// Returns the inline static position alignment edge for `oof_node`. If
// `should_swap_inline_axis` is true, that means that we are in an inline
// context, and the text direction is in RTL.
LogicalStaticPosition::InlineEdge InlineStaticPositionEdge(
    const BlockNode& oof_node,
    const ComputedStyle* justify_items_style,
    WritingDirectionMode parent_writing_direction,
    bool should_swap_inline_axis = false);

// Returns the block static position alignment edge for `oof_node`.
LogicalStaticPosition::BlockEdge BlockStaticPositionEdge(
    const BlockNode& oof_node,
    const ComputedStyle* align_items_style,
    WritingDirectionMode parent_writing_direction);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_BLOCK_LAYOUT_ALGORITHM_UTILS_H_

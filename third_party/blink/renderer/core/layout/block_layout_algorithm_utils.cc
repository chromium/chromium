// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/block_layout_algorithm_utils.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/box_fragment_builder.h"
#include "third_party/blink/renderer/core/layout/exclusions/exclusion_space.h"
#include "third_party/blink/renderer/core/layout/fragmentation_utils.h"
#include "third_party/blink/renderer/core/layout/length_utils.h"

namespace blink {

namespace {

BlockContentAlignment ComputeContentAlignment(const ComputedStyle& style,
                                              bool behave_like_table_cell,
                                              UseCounter* use_counter) {
  const StyleContentAlignmentData& alignment = style.AlignContent();
  ContentPosition position = alignment.GetPosition();
  OverflowAlignment overflow = alignment.Overflow();
  // https://drafts.csswg.org/css-align/#distribution-block
  // If a <content-distribution> is specified its fallback alignment is used
  // instead.
  switch (alignment.Distribution()) {
    case ContentDistributionType::kDefault:
      break;
    case ContentDistributionType::kSpaceBetween:
    case ContentDistributionType::kStretch:
      position = ContentPosition::kFlexStart;
      break;
    case ContentDistributionType::kSpaceAround:
    case ContentDistributionType::kSpaceEvenly:
      overflow = OverflowAlignment::kSafe;
      position = ContentPosition::kCenter;
      break;
  }
  if (position == ContentPosition::kLastBaseline) {
    overflow = OverflowAlignment::kSafe;
    position = ContentPosition::kEnd;
  }

  if (use_counter) {
    if (!behave_like_table_cell) {
      if (position != ContentPosition::kNormal &&
          position != ContentPosition::kStart &&
          position != ContentPosition::kBaseline &&
          position != ContentPosition::kFlexStart) {
        UseCounter::Count(*use_counter,
                          WebFeature::kEffectiveAlignContentForBlock);
      }
    } else if (position != ContentPosition::kNormal &&
               position != ContentPosition::kCenter) {
      UseCounter::Count(*use_counter,
                        WebFeature::kEffectiveAlignContentForTableCell);
    }
  }

  // https://drafts.csswg.org/css-align/#typedef-overflow-position
  // UAs that have not implemented the "smart" default behavior must behave as
  // safe for align-content on block containers
  if (overflow == OverflowAlignment::kDefault) {
    overflow = OverflowAlignment::kSafe;
  }
  const bool is_safe = overflow == OverflowAlignment::kSafe;
  switch (position) {
    case ContentPosition::kCenter:
      return is_safe ? BlockContentAlignment::kSafeCenter
                     : BlockContentAlignment::kUnsafeCenter;

    case ContentPosition::kEnd:
    case ContentPosition::kFlexEnd:
      return is_safe ? BlockContentAlignment::kSafeEnd
                     : BlockContentAlignment::kUnsafeEnd;

    case ContentPosition::kNormal:
      if (!behave_like_table_cell) {
        return BlockContentAlignment::kStart;
      }
      switch (style.VerticalAlign()) {
        case EVerticalAlign::kTop:
          // Do nothing for 'top' vertical alignment.
          return BlockContentAlignment::kStart;

        case EVerticalAlign::kBaselineMiddle:
        case EVerticalAlign::kSub:
        case EVerticalAlign::kSuper:
        case EVerticalAlign::kTextTop:
        case EVerticalAlign::kTextBottom:
        case EVerticalAlign::kLength:
          // All of the above are treated as 'baseline' for the purposes of
          // table-cell vertical alignment.
        case EVerticalAlign::kBaseline:
          return BlockContentAlignment::kBaseline;

        case EVerticalAlign::kMiddle:
          return RuntimeEnabledFeatures::LayoutTableCellAlignmentSafeEnabled()
                     ? BlockContentAlignment::kSafeCenter
                     : BlockContentAlignment::kUnsafeCenter;

        case EVerticalAlign::kBottom:
          return RuntimeEnabledFeatures::LayoutTableCellAlignmentSafeEnabled()
                     ? BlockContentAlignment::kSafeEnd
                     : BlockContentAlignment::kUnsafeEnd;
      }
      break;

    case ContentPosition::kStart:
    case ContentPosition::kFlexStart:
      return BlockContentAlignment::kStart;

    case ContentPosition::kBaseline:
      return BlockContentAlignment::kBaseline;

    case ContentPosition::kLastBaseline:
    case ContentPosition::kLeft:
    case ContentPosition::kRight:
      NOTREACHED();
  }
  return BlockContentAlignment::kStart;
}

}  // namespace

LayoutUnit CalculateOutOfFlowStaticInlineLevelOffset(
    const ComputedStyle& container_style,
    const BfcOffset& origin_bfc_offset,
    const ExclusionSpace& exclusion_space,
    LayoutUnit child_available_inline_size) {
  const TextDirection direction = container_style.Direction();

  // Find a layout opportunity, where we would have placed a zero-sized line.
  LayoutOpportunity opportunity = exclusion_space.FindLayoutOpportunity(
      origin_bfc_offset, child_available_inline_size);

  LayoutUnit child_line_offset = IsLtr(direction)
                                     ? opportunity.rect.LineStartOffset()
                                     : opportunity.rect.LineEndOffset();

  LayoutUnit relative_line_offset =
      child_line_offset - origin_bfc_offset.line_offset;

  // Convert back to the logical coordinate system. As the conversion is on an
  // OOF-positioned node, we pretent it has zero inline-size.
  LayoutUnit inline_offset =
      IsLtr(direction) ? relative_line_offset
                       : child_available_inline_size - relative_line_offset;

  // Adjust for text alignment, within the layout opportunity.
  LayoutUnit line_offset = LineOffsetForTextAlign(
      container_style.GetTextAlign(), direction, opportunity.rect.InlineSize());

  if (IsLtr(direction))
    inline_offset += line_offset;
  else
    inline_offset += opportunity.rect.InlineSize() - line_offset;

  // Adjust for the text-indent.
  inline_offset += MinimumValueForLength(container_style.TextIndent(),
                                         child_available_inline_size);

  return inline_offset;
}

BlockContentAlignment ComputeContentAlignmentForBlock(
    const ComputedStyle& style,
    UseCounter* use_counter) {
  // ruby-text uses BlockLayoutAlgorithm, but they are not a block container
  // officially.
  if (!style.IsDisplayBlockContainer()) {
    return BlockContentAlignment::kStart;
  }
  bool behave_like_table_cell = style.IsPageMarginBox();
  return ComputeContentAlignment(style, behave_like_table_cell, use_counter);
}

BlockContentAlignment ComputeContentAlignmentForTableCell(
    const ComputedStyle& style,
    UseCounter* use_counter) {
  return ComputeContentAlignment(style, /*behave_like_table_cell=*/true,
                                 use_counter);
}

void AlignBlockContent(const ComputedStyle& style,
                       const BlockBreakToken* break_token,
                       LayoutUnit content_block_size,
                       BoxFragmentBuilder& builder) {
  if (IsBreakInside(break_token)) {
    // Do nothing for the second or later fragments.
    return;
  }

  LayoutUnit free_space = builder.FragmentBlockSize() - content_block_size;
  if (style.AlignContentBlockCenter()) {
    // Buttons have safe alignment.
    if (builder.Node().IsButtonOrInputButton()) {
      free_space = free_space.ClampNegativeToZero();
    }
    builder.MoveChildrenInDirection(free_space / 2,
                                    /*is_block_direction=*/true);
    return;
  }

  if (!ShouldIncludeBlockEndBorderPadding(builder)) {
    // Do nothing for the first fragment without block-end border and padding.
    // See css/css-align/blocks/align-content-block-break-overflow-010.html
    return;
  }

  BlockContentAlignment alignment =
      ComputeContentAlignmentForBlock(style, &builder.Node().GetDocument());
  if (alignment == BlockContentAlignment::kSafeCenter ||
      alignment == BlockContentAlignment::kSafeEnd) {
    free_space = free_space.ClampNegativeToZero();
  }
  switch (alignment) {
    case BlockContentAlignment::kStart:
    case BlockContentAlignment::kBaseline:
      // Nothing to do.
      break;
    case BlockContentAlignment::kSafeCenter:
    case BlockContentAlignment::kUnsafeCenter:
      builder.MoveChildrenInDirection(free_space / 2,
                                      /*is_block_direction=*/true);
      break;
    case BlockContentAlignment::kSafeEnd:
    case BlockContentAlignment::kUnsafeEnd:
      builder.MoveChildrenInDirection(free_space, /*is_block_direction=*/true);
  }
}

LogicalStaticPosition::InlineEdge InlineStaticPositionEdge(
    const BlockNode& oof_node,
    const ComputedStyle* justify_items_style,
    WritingDirectionMode parent_writing_direction,
    bool should_swap_inline_axis) {
  CHECK(oof_node.IsOutOfFlowPositioned());
  StyleSelfAlignmentData normal_value_behavior = {ItemPosition::kStart,
                                                  OverflowAlignment::kDefault};
  const ItemPosition align_self =
      oof_node.Style()
          .ResolvedJustifySelf(normal_value_behavior, justify_items_style)
          .GetPosition();

  switch (align_self) {
    case ItemPosition::kEnd:
    case ItemPosition::kFlexEnd:
    case ItemPosition::kLastBaseline:
    case ItemPosition::kRight: {
      return should_swap_inline_axis ? LogicalStaticPosition::kInlineStart
                                     : LogicalStaticPosition::kInlineEnd;
    }
    case ItemPosition::kAnchorCenter:
    case ItemPosition::kCenter:
      return LogicalStaticPosition::kInlineCenter;
    case ItemPosition::kBaseline:
    case ItemPosition::kFlexStart:
    case ItemPosition::kLeft:
    case ItemPosition::kStart:
    case ItemPosition::kStretch: {
      return should_swap_inline_axis ? LogicalStaticPosition::kInlineEnd
                                     : LogicalStaticPosition::kInlineStart;
    }
    case ItemPosition::kSelfEnd:
    case ItemPosition::kSelfStart: {
      LogicalToLogical<LogicalStaticPosition::InlineEdge> logical(
          oof_node.Style().GetWritingDirection(), parent_writing_direction,
          LogicalStaticPosition::kInlineStart,
          LogicalStaticPosition::kInlineEnd,
          LogicalStaticPosition::kInlineStart,
          LogicalStaticPosition::kInlineEnd);
      return (align_self == ItemPosition::kSelfStart) ? logical.InlineStart()
                                                      : logical.InlineEnd();
    }
    case ItemPosition::kAuto:
    case ItemPosition::kLegacy:
    case ItemPosition::kNormal:
      NOTREACHED();
  }
}

LogicalStaticPosition::BlockEdge BlockStaticPositionEdge(
    const BlockNode& oof_node,
    const ComputedStyle* align_items_style,
    WritingDirectionMode parent_writing_direction) {
  CHECK(oof_node.IsOutOfFlowPositioned());
  StyleSelfAlignmentData normal_value_behavior = {ItemPosition::kStart,
                                                  OverflowAlignment::kDefault};
  const ItemPosition align_self =
      oof_node.Style()
          .ResolvedAlignSelf(normal_value_behavior, align_items_style)
          .GetPosition();

  switch (align_self) {
    case ItemPosition::kEnd:
    case ItemPosition::kFlexEnd:
    case ItemPosition::kLastBaseline:
      return LogicalStaticPosition::kBlockEnd;
    case ItemPosition::kAnchorCenter:
    case ItemPosition::kCenter:
      return LogicalStaticPosition::kBlockCenter;
    case ItemPosition::kBaseline:
    case ItemPosition::kFlexStart:
    case ItemPosition::kStart:
    case ItemPosition::kStretch:
      return LogicalStaticPosition::kBlockStart;
    case ItemPosition::kSelfEnd:
    case ItemPosition::kSelfStart: {
      LogicalToLogical<LogicalStaticPosition::BlockEdge> logical(
          oof_node.Style().GetWritingDirection(), parent_writing_direction,
          LogicalStaticPosition::kBlockStart, LogicalStaticPosition::kBlockEnd,
          LogicalStaticPosition::kBlockStart, LogicalStaticPosition::kBlockEnd);
      return (align_self == ItemPosition::kSelfStart) ? logical.BlockStart()
                                                      : logical.BlockEnd();
    }
    case ItemPosition::kAuto:
    case ItemPosition::kLeft:
    case ItemPosition::kRight:
    case ItemPosition::kLegacy:
    case ItemPosition::kNormal:
      NOTREACHED();
  }
}

}  // namespace blink

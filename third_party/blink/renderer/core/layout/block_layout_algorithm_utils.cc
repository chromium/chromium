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
          return BlockContentAlignment::kUnsafeCenter;

        case EVerticalAlign::kBottom:
          return BlockContentAlignment::kUnsafeEnd;
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
      NOTREACHED_IN_MIGRATION();
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
    builder.MoveChildrenInBlockDirection(free_space / 2);
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
      builder.MoveChildrenInBlockDirection(free_space / 2);
      break;
    case BlockContentAlignment::kSafeEnd:
    case BlockContentAlignment::kUnsafeEnd:
      builder.MoveChildrenInBlockDirection(free_space);
  }
}

}  // namespace blink

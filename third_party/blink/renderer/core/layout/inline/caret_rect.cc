// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/caret_rect.h"

#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/inline/inline_caret_position.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

// Gets the resolved direction for any inline, including non-atomic inline
// boxes.
//
// TODO(yosin): We should share |ResolvedDirection()| with "bidi_adjustment.cc"
TextDirection ResolvedDirection(const InlineCursor& cursor) {
  if (cursor.Current().IsText() || cursor.Current().IsAtomicInline())
    return cursor.Current().ResolvedDirection();

  // TODO(andreubotella): We should define the |TextDirection| of an inline box,
  // which is used to determine at which edge of a non-editable box to place the
  // text editing caret. We currently use the line's base direction, but this is
  // wrong:
  //   <div dir=ltr>abc A<b>B</b>C abc</div>
  InlineCursor line_box;
  line_box.MoveTo(cursor);
  line_box.MoveToContainingLine();
  return line_box.Current().BaseDirection();
}

PhysicalRect ComputeLocalCaretRectByBoxSide(
    const InlineCursor& cursor,
    InlineCaretPositionType position_type) {
  InlineCursor line_box(cursor);
  line_box.MoveToContainingLine();
  DCHECK(line_box);
  bool is_atomic_inline = cursor.Current().IsAtomicInline();
  // RTL is handled manually at the bottom of this function.
  WritingModeConverter converter(
      {cursor.Current().Style().GetWritingMode(), TextDirection::kLtr},
      is_atomic_inline ? cursor.Current().Size()
                       : cursor.ContainerFragment().Size());
  LogicalRect line_rect =
      converter.ToLogical(line_box.Current().RectInContainerFragment());
  LogicalRect item_rect =
      converter.ToLogical(cursor.Current().RectInContainerFragment());

  LogicalRect caret_rect;
  caret_rect.size.block_size = line_rect.size.block_size;
  // The block-start of the caret is always the block-start of the line.
  caret_rect.offset.block_offset = line_rect.offset.block_offset;
  if (is_atomic_inline) {
    // For atomic-inline, this function should return a rectangle relative to
    // the atomic-inline.
    caret_rect.offset.block_offset -= item_rect.offset.block_offset;
  }

  const LocalFrameView* frame_view =
      cursor.Current().GetLayoutObject()->GetDocument().View();
  caret_rect.size.inline_size = frame_view->CaretWidth();

  const bool is_ltr = IsLtr(ResolvedDirection(cursor));
  if (!is_atomic_inline) {
    caret_rect.offset.inline_offset = item_rect.offset.inline_offset;
  }
  if (is_ltr != (position_type == InlineCaretPositionType::kBeforeBox)) {
    caret_rect.offset.inline_offset +=
        item_rect.size.inline_size - caret_rect.size.inline_size;
  }

  return converter.ToPhysical(caret_rect);
}

bool ShouldAlignCaretRight(ETextAlign text_align, TextDirection direction) {
  switch (text_align) {
    case ETextAlign::kRight:
    case ETextAlign::kWebkitRight:
      return true;
    case ETextAlign::kLeft:
    case ETextAlign::kWebkitLeft:
    case ETextAlign::kCenter:
    case ETextAlign::kWebkitCenter:
      return false;
    case ETextAlign::kJustify:
    case ETextAlign::kStart:
      return IsRtl(direction);
    case ETextAlign::kEnd:
      return IsLtr(direction);
  }
  NOTREACHED_IN_MIGRATION();
  return false;
}

LayoutUnit ClampAndRound(LayoutUnit value, LayoutUnit min, LayoutUnit max) {
  LayoutUnit min_ceil = LayoutUnit(min.Ceil());
  LayoutUnit max_floor = LayoutUnit(max.Floor());
  if (min_ceil >= max_floor)
    return max_floor;
  return LayoutUnit(ClampTo<LayoutUnit>(value, min_ceil, max_floor).Round());
}

PhysicalRect ComputeLocalCaretRectAtTextOffset(const InlineCursor& cursor,
                                               unsigned offset) {
  DCHECK(cursor.Current().IsText());
  DCHECK_GE(offset, cursor.Current().TextStartOffset());
  DCHECK_LE(offset, cursor.Current().TextEndOffset());

  const LocalFrameView* frame_view =
      cursor.Current().GetLayoutObject()->GetDocument().View();
  LayoutUnit caret_width = frame_view->CaretWidth();

  const ComputedStyle& style = cursor.Current().Style();
  const bool is_horizontal = style.IsHorizontalWritingMode();

  WritingModeConverter converter({style.GetWritingMode(), TextDirection::kLtr},
                                 cursor.Current().Size());
  LogicalRect caret_rect;
  caret_rect.size.inline_size = caret_width;
  caret_rect.size.block_size =
      converter.ToLogical(cursor.Current().Size()).block_size;

  LayoutUnit caret_left = cursor.CaretInlinePositionForOffset(offset);
  if (cursor.CurrentItem()->IsSvgText()) {
    caret_left /= cursor.CurrentItem()->SvgScalingFactor();
  }
  if (!cursor.Current().IsLineBreak())
    caret_left -= caret_width / 2;
  caret_rect.offset.inline_offset = caret_left;

  PhysicalRect physical_caret_rect = converter.ToPhysical(caret_rect);

  // Adjust the location to be relative to the inline formatting context.
  PhysicalOffset caret_location =
      physical_caret_rect.offset + cursor.Current().OffsetInContainerFragment();
  const auto* const text_combine = DynamicTo<LayoutTextCombine>(
      cursor.Current().GetLayoutObject()->Parent());
  if (text_combine) [[unlikely]] {
    caret_location =
        text_combine->AdjustOffsetForLocalCaretRect(caret_location);
  }

  const PhysicalBoxFragment& fragment = cursor.ContainerFragment();
  InlineCursor line_box(cursor);
  line_box.MoveToContainingLine();
  const PhysicalOffset line_box_offset =
      line_box.Current().OffsetInContainerFragment();
  const PhysicalRect line_box_rect(line_box_offset, line_box.Current().Size());

  const auto* break_token = line_box.Current().GetInlineBreakToken();
  const bool is_last_line = !break_token || break_token->IsForcedBreak();
  const ComputedStyle& block_style = fragment.Style();
  bool should_align_caret_right =
      ShouldAlignCaretRight(block_style.GetTextAlign(is_last_line),
                            line_box.Current().BaseDirection());

  // For horizontal text, adjust the location in the x direction to ensure that
  // it completely falls in the union of line box and containing block, and
  // then round it to the nearest pixel.
  if (is_horizontal) {
    if (should_align_caret_right) {
      const LayoutUnit left_edge = std::min(LayoutUnit(), line_box_rect.X());
      const LayoutUnit right_limit = line_box_rect.Right() - caret_width;
      caret_location.left =
          ClampAndRound(caret_location.left, left_edge, right_limit);
    } else {
      const LayoutUnit right_limit =
          std::max(fragment.Size().width, line_box_rect.Right()) - caret_width;
      caret_location.left =
          ClampAndRound(caret_location.left, line_box_rect.X(), right_limit);
    }
    return PhysicalRect(caret_location, physical_caret_rect.size);
  }

  // Similar adjustment and rounding for vertical text.
  const LayoutUnit min_y = std::min(LayoutUnit(), line_box_offset.top);
  const LayoutUnit bottom_limit =
      std::max(fragment.Size().height, line_box_rect.Bottom()) - caret_width;
  caret_location.top = ClampAndRound(caret_location.top, min_y, bottom_limit);
  return PhysicalRect(caret_location, physical_caret_rect.size);
}

}  // namespace

LocalCaretRect ComputeLocalCaretRect(
    const InlineCaretPosition& caret_position) {
  if (caret_position.IsNull())
    return LocalCaretRect();

  const LayoutObject* const layout_object =
      caret_position.cursor.Current().GetLayoutObject();
  const PhysicalBoxFragment& container_fragment =
      caret_position.cursor.ContainerFragment();
  switch (caret_position.position_type) {
    case InlineCaretPositionType::kBeforeBox:
    case InlineCaretPositionType::kAfterBox: {
      DCHECK(!caret_position.cursor.Current().IsText());
      const PhysicalRect fragment_local_rect = ComputeLocalCaretRectByBoxSide(
          caret_position.cursor, caret_position.position_type);
      return {layout_object, fragment_local_rect, &container_fragment};
    }
    case InlineCaretPositionType::kAtTextOffset: {
      DCHECK(caret_position.cursor.Current().IsText());
      DCHECK(caret_position.text_offset.has_value());
      const PhysicalRect caret_rect = ComputeLocalCaretRectAtTextOffset(
          caret_position.cursor, *caret_position.text_offset);
      return {layout_object, caret_rect, &container_fragment};
    }
  }

  NOTREACHED_IN_MIGRATION();
  return {layout_object, PhysicalRect()};
}

LocalCaretRect ComputeLocalSelectionRect(
    const InlineCaretPosition& caret_position) {
  const LocalCaretRect caret_rect = ComputeLocalCaretRect(caret_position);
  if (!caret_rect.layout_object)
    return caret_rect;

  InlineCursor line_box(caret_position.cursor);
  line_box.MoveToContainingLine();
  // TODO(yosin): We'll hit this DCHECK for caret in empty block if we
  // enable LayoutNG in contenteditable.
  DCHECK(line_box);

  PhysicalRect rect = caret_rect.rect;
  if (caret_position.cursor.Current().Style().IsHorizontalWritingMode()) {
    rect.SetY(line_box.Current().OffsetInContainerFragment().top);
    rect.SetHeight(line_box.Current().Size().height);
  } else {
    rect.SetX(line_box.Current().OffsetInContainerFragment().left);
    rect.SetHeight(line_box.Current().Size().width);
  }
  return {caret_rect.layout_object, rect,
          &caret_position.cursor.ContainerFragment()};
}

}  // namespace blink

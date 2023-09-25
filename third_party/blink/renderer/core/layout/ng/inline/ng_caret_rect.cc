// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_rect.h"

#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_position.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

namespace {

// Gets the resolved direction for any inline, including non-atomic inline
// boxes.
//
// TODO(yosin): We should share |ResolvedDirection()| with "bidi_adjustment.cc"
TextDirection ResolvedDirection(const NGInlineCursor& cursor) {
  if (cursor.Current().IsText() || cursor.Current().IsAtomicInline())
    return cursor.Current().ResolvedDirection();

  // TODO(andreubotella): We should define the |TextDirection| of an inline box,
  // which is used to determine at which edge of a non-editable box to place the
  // text editing caret. We currently use the line's base direction, but this is
  // wrong:
  //   <div dir=ltr>abc A<b>B</b>C abc</div>
  NGInlineCursor line_box;
  line_box.MoveTo(cursor);
  line_box.MoveToContainingLine();
  return line_box.Current().BaseDirection();
}

PhysicalRect ComputeLocalCaretRectByBoxSide(const NGInlineCursor& cursor,
                                            NGCaretPositionType position_type) {
  const bool is_horizontal = cursor.Current().Style().IsHorizontalWritingMode();
  NGInlineCursor line_box(cursor);
  line_box.MoveToContainingLine();
  DCHECK(line_box);
  const PhysicalOffset offset = cursor.Current().OffsetInContainerFragment();
  const PhysicalOffset line_box_offset =
      line_box.Current().OffsetInContainerFragment();
  LayoutUnit caret_height = is_horizontal ? line_box.Current().Size().height
                                          : line_box.Current().Size().width;
  LayoutUnit caret_top;
  if (cursor.Current().IsAtomicInline()) {
    caret_top = is_horizontal ? line_box_offset.top - offset.top
                              : line_box_offset.left - offset.left;
  } else {
    caret_top = is_horizontal ? line_box_offset.top : line_box_offset.left;
  }

  const LocalFrameView* frame_view =
      cursor.Current().GetLayoutObject()->GetDocument().View();
  LayoutUnit caret_width = frame_view->CaretWidth();

  const bool is_ltr = IsLtr(ResolvedDirection(cursor));
  LayoutUnit caret_left;
  if (!cursor.Current().IsAtomicInline()) {
    caret_left = is_horizontal ? offset.left : offset.top;
  }
  if (is_ltr != (position_type == NGCaretPositionType::kBeforeBox)) {
    if (is_horizontal)
      caret_left += cursor.Current().Size().width - caret_width;
    else
      caret_left += cursor.Current().Size().height - caret_width;
  }

  if (!is_horizontal) {
    std::swap(caret_top, caret_left);
    std::swap(caret_width, caret_height);
  }

  const PhysicalOffset caret_location(caret_left, caret_top);
  const PhysicalSize caret_size(caret_width, caret_height);
  return PhysicalRect(caret_location, caret_size);
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
  NOTREACHED();
  return false;
}

LayoutUnit ClampAndRound(LayoutUnit value, LayoutUnit min, LayoutUnit max) {
  LayoutUnit min_ceil = LayoutUnit(min.Ceil());
  LayoutUnit max_floor = LayoutUnit(max.Floor());
  if (min_ceil >= max_floor)
    return max_floor;
  return LayoutUnit(ClampTo<LayoutUnit>(value, min_ceil, max_floor).Round());
}

PhysicalRect ComputeLocalCaretRectAtTextOffset(const NGInlineCursor& cursor,
                                               unsigned offset) {
  DCHECK(cursor.Current().IsText());
  DCHECK_GE(offset, cursor.Current().TextStartOffset());
  DCHECK_LE(offset, cursor.Current().TextEndOffset());

  const LocalFrameView* frame_view =
      cursor.Current().GetLayoutObject()->GetDocument().View();
  LayoutUnit caret_width = frame_view->CaretWidth();

  const ComputedStyle& style = cursor.Current().Style();
  const bool is_horizontal = style.IsHorizontalWritingMode();

  LayoutUnit caret_height = is_horizontal ? cursor.Current().Size().height
                                          : cursor.Current().Size().width;
  LayoutUnit caret_top;

  LayoutUnit caret_left = cursor.CaretInlinePositionForOffset(offset);
  if (!cursor.Current().IsLineBreak())
    caret_left -= caret_width / 2;

  if (!is_horizontal) {
    std::swap(caret_top, caret_left);
    std::swap(caret_width, caret_height);
  }

  // Adjust the location to be relative to the inline formatting context.
  PhysicalOffset caret_location = PhysicalOffset(caret_left, caret_top) +
                                  cursor.Current().OffsetInContainerFragment();
  const auto* const text_combine = DynamicTo<LayoutTextCombine>(
      cursor.Current().GetLayoutObject()->Parent());
  if (UNLIKELY(text_combine)) {
    caret_location =
        text_combine->AdjustOffsetForLocalCaretRect(caret_location);
  }
  const PhysicalSize caret_size(caret_width, caret_height);

  const NGPhysicalBoxFragment& fragment = cursor.ContainerFragment();
  NGInlineCursor line_box(cursor);
  line_box.MoveToContainingLine();
  const PhysicalOffset line_box_offset =
      line_box.Current().OffsetInContainerFragment();
  const PhysicalRect line_box_rect(line_box_offset, line_box.Current().Size());

  const NGInlineBreakToken* break_token = line_box.Current().InlineBreakToken();
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
    return PhysicalRect(caret_location, caret_size);
  }

  // Similar adjustment and rounding for vertical text.
  const LayoutUnit min_y = std::min(LayoutUnit(), line_box_offset.top);
  const LayoutUnit bottom_limit =
      std::max(fragment.Size().height, line_box_rect.Bottom()) - caret_height;
  caret_location.top = ClampAndRound(caret_location.top, min_y, bottom_limit);
  return PhysicalRect(caret_location, caret_size);
}

}  // namespace

LocalCaretRect ComputeLocalCaretRect(const NGCaretPosition& caret_position) {
  if (caret_position.IsNull())
    return LocalCaretRect();

  const LayoutObject* const layout_object =
      caret_position.cursor.Current().GetLayoutObject();
  const NGPhysicalBoxFragment& container_fragment =
      caret_position.cursor.ContainerFragment();
  switch (caret_position.position_type) {
    case NGCaretPositionType::kBeforeBox:
    case NGCaretPositionType::kAfterBox: {
      DCHECK(!caret_position.cursor.Current().IsText());
      const PhysicalRect fragment_local_rect = ComputeLocalCaretRectByBoxSide(
          caret_position.cursor, caret_position.position_type);
      return {layout_object, fragment_local_rect, &container_fragment};
    }
    case NGCaretPositionType::kAtTextOffset: {
      DCHECK(caret_position.cursor.Current().IsText());
      DCHECK(caret_position.text_offset.has_value());
      const PhysicalRect caret_rect = ComputeLocalCaretRectAtTextOffset(
          caret_position.cursor, *caret_position.text_offset);
      return {layout_object, caret_rect, &container_fragment};
    }
  }

  NOTREACHED();
  return {layout_object, PhysicalRect()};
}

LocalCaretRect ComputeLocalSelectionRect(
    const NGCaretPosition& caret_position) {
  const LocalCaretRect caret_rect = ComputeLocalCaretRect(caret_position);
  if (!caret_rect.layout_object)
    return caret_rect;

  NGInlineCursor line_box(caret_position.cursor);
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

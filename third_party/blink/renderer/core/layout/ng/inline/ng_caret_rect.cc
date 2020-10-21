// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_rect.h"

#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_caret_position.h"
#include "third_party/blink/renderer/core/layout/ng/ng_physical_box_fragment.h"

namespace blink {

namespace {

PhysicalRect ComputeLocalCaretRectByBoxSide(const NGInlineCursor& cursor,
                                            NGCaretPositionType position_type) {
  const bool is_horizontal = cursor.Current().Style().IsHorizontalWritingMode();
  NGInlineCursor line_box(cursor);
  line_box.MoveToContainingLine();
  DCHECK(line_box);
  const PhysicalOffset offset_to_line_box =
      cursor.Current().OffsetInContainerBlock() -
      line_box.Current().OffsetInContainerBlock();
  LayoutUnit caret_height = is_horizontal ? line_box.Current().Size().height
                                          : line_box.Current().Size().width;
  LayoutUnit caret_top =
      is_horizontal ? -offset_to_line_box.top : -offset_to_line_box.left;

  const LocalFrameView* frame_view =
      cursor.Current().GetLayoutObject()->GetDocument().View();
  LayoutUnit caret_width = frame_view->CaretWidth();

  const bool is_ltr = IsLtr(cursor.Current().ResolvedDirection());
  LayoutUnit caret_left;
  if (is_ltr != (position_type == NGCaretPositionType::kBeforeBox)) {
    if (is_horizontal)
      caret_left = cursor.Current().Size().width - caret_width;
    else
      caret_left = cursor.Current().Size().height - caret_width;
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

  LayoutUnit caret_left = cursor.InlinePositionForOffset(offset);
  if (!cursor.Current().IsLineBreak())
    caret_left -= caret_width / 2;

  if (!is_horizontal) {
    std::swap(caret_top, caret_left);
    std::swap(caret_width, caret_height);
  }

  // Adjust the location to be relative to the inline formatting context.
  PhysicalOffset caret_location = PhysicalOffset(caret_left, caret_top) +
                                  cursor.Current().OffsetInContainerBlock();
  const PhysicalSize caret_size(caret_width, caret_height);

  // TODO(crbug.com/1061423): Shouldn't assume that there's only one
  // fragment.
  const NGPhysicalBoxFragment& fragment =
      *cursor.Current().GetLayoutObject()->ContainingBlockFlowFragment();
  NGInlineCursor line_box(cursor);
  line_box.MoveToContainingLine();
  const PhysicalOffset line_box_offset =
      line_box.Current().OffsetInContainerBlock();
  const PhysicalRect line_box_rect(line_box_offset, line_box.Current().Size());

  const NGInlineBreakToken& break_token =
      *line_box.Current().InlineBreakToken();
  const bool is_last_line =
      break_token.IsFinished() || break_token.IsForcedBreak();
  const ComputedStyle& block_style = fragment.Style();
  bool should_align_caret_right =
      ShouldAlignCaretRight(block_style.GetTextAlign(is_last_line),
                            block_style.Direction()) &&
      (style.GetUnicodeBidi() != UnicodeBidi::kPlaintext ||
       IsLtr(cursor.Current().ResolvedDirection()));

  // For horizontal text, adjust the location in the x direction to ensure that
  // it completely falls in the union of line box and containing block, and
  // then round it to the nearest pixel.
  if (is_horizontal) {
    if (should_align_caret_right) {
      const LayoutUnit left_edge = std::min(LayoutUnit(), line_box_rect.X());
      caret_location.left = std::max(caret_location.left, left_edge);
      caret_location.left =
          std::min(caret_location.left, line_box_rect.Right() - caret_width);
    } else {
      const LayoutUnit right_edge =
          std::max(fragment.Size().width, line_box_rect.Right());
      caret_location.left =
          std::min(caret_location.left, right_edge - caret_width);
      caret_location.left = std::max(caret_location.left, line_box_rect.X());
    }
    caret_location.left = LayoutUnit(caret_location.left.Round());
    return PhysicalRect(caret_location, caret_size);
  }

  // Similar adjustment and rounding for vertical text.
  const LayoutUnit min_y = std::min(LayoutUnit(), line_box_offset.top);
  caret_location.top = std::max(caret_location.top, min_y);
  const LayoutUnit max_y =
      std::max(fragment.Size().height, line_box_rect.Bottom());
  caret_location.top = std::min(caret_location.top, max_y - caret_height);
  caret_location.top = LayoutUnit(caret_location.top.Round());
  return PhysicalRect(caret_location, caret_size);
}

LocalCaretRect ComputeLocalCaretRect(const NGCaretPosition& caret_position) {
  if (caret_position.IsNull())
    return LocalCaretRect();

  const LayoutObject* layout_object =
      caret_position.cursor.Current().GetLayoutObject();
  switch (caret_position.position_type) {
    case NGCaretPositionType::kBeforeBox:
    case NGCaretPositionType::kAfterBox: {
      DCHECK(!caret_position.cursor.Current().IsText());
      const PhysicalRect fragment_local_rect = ComputeLocalCaretRectByBoxSide(
          caret_position.cursor, caret_position.position_type);
      return {layout_object, fragment_local_rect};
    }
    case NGCaretPositionType::kAtTextOffset: {
      DCHECK(caret_position.cursor.Current().IsText());
      DCHECK(caret_position.text_offset.has_value());
      const PhysicalRect caret_rect = ComputeLocalCaretRectAtTextOffset(
          caret_position.cursor, *caret_position.text_offset);
      return {layout_object, caret_rect};
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
    rect.SetY(line_box.Current().OffsetInContainerBlock().top);
    rect.SetHeight(line_box.Current().Size().height);
  } else {
    rect.SetX(line_box.Current().OffsetInContainerBlock().left);
    rect.SetHeight(line_box.Current().Size().width);
  }
  return {caret_rect.layout_object, rect};
}

}  // namespace

LocalCaretRect ComputeNGLocalCaretRect(const PositionWithAffinity& position) {
  return ComputeLocalCaretRect(ComputeNGCaretPosition(position));
}

LocalCaretRect ComputeNGLocalSelectionRect(
    const PositionWithAffinity& position) {
  return ComputeLocalSelectionRect(ComputeNGCaretPosition(position));
}

}  // namespace blink

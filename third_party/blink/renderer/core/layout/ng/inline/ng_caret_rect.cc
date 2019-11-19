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
  const bool is_horizontal = cursor.CurrentStyle().IsHorizontalWritingMode();
  NGInlineCursor line_box(cursor);
  line_box.MoveToContainingLine();
  DCHECK(line_box);
  const PhysicalOffset offset_to_line_box =
      cursor.CurrentOffset() - line_box.CurrentOffset();
  LayoutUnit caret_height = is_horizontal ? line_box.CurrentSize().height
                                          : line_box.CurrentSize().width;
  LayoutUnit caret_top =
      is_horizontal ? -offset_to_line_box.top : -offset_to_line_box.left;

  const LocalFrameView* frame_view =
      cursor.CurrentLayoutObject()->GetDocument().View();
  LayoutUnit caret_width = frame_view->CaretWidth();

  const bool is_ltr = IsLtr(cursor.CurrentResolvedDirection());
  LayoutUnit caret_left;
  if (is_ltr != (position_type == NGCaretPositionType::kBeforeBox)) {
    if (is_horizontal)
      caret_left = cursor.CurrentSize().width - caret_width;
    else
      caret_left = cursor.CurrentSize().height - caret_width;
  }

  if (!is_horizontal) {
    std::swap(caret_top, caret_left);
    std::swap(caret_width, caret_height);
  }

  const PhysicalOffset caret_location(caret_left, caret_top);
  const PhysicalSize caret_size(caret_width, caret_height);
  return PhysicalRect(caret_location, caret_size);
}

PhysicalRect ComputeLocalCaretRectAtTextOffset(const NGInlineCursor& cursor,
                                               unsigned offset) {
  DCHECK(cursor.IsText());
  DCHECK_GE(offset, cursor.CurrentTextStartOffset());
  DCHECK_LE(offset, cursor.CurrentTextEndOffset());

  const LocalFrameView* frame_view =
      cursor.CurrentLayoutObject()->GetDocument().View();
  LayoutUnit caret_width = frame_view->CaretWidth();

  const bool is_horizontal = cursor.CurrentStyle().IsHorizontalWritingMode();

  LayoutUnit caret_height =
      is_horizontal ? cursor.CurrentSize().height : cursor.CurrentSize().width;
  LayoutUnit caret_top;

  LayoutUnit caret_left = cursor.InlinePositionForOffset(offset);
  if (!cursor.IsLineBreak())
    caret_left -= caret_width / 2;

  if (!is_horizontal) {
    std::swap(caret_top, caret_left);
    std::swap(caret_width, caret_height);
  }

  // Adjust the location to be relative to the inline formatting context.
  PhysicalOffset caret_location =
      PhysicalOffset(caret_left, caret_top) + cursor.CurrentOffset();
  const PhysicalSize caret_size(caret_width, caret_height);

  const NGPhysicalBoxFragment& fragmentainer =
      *cursor.CurrentLayoutObject()->ContainingBlockFlowFragment();
  NGInlineCursor line_box(cursor);
  line_box.MoveToContainingLine();
  const PhysicalOffset line_box_offset = line_box.CurrentOffset();
  const PhysicalRect line_box_rect(line_box_offset, line_box.CurrentSize());

  // For horizontal text, adjust the location in the x direction to ensure that
  // it completely falls in the union of line box and containing block, and
  // then round it to the nearest pixel.
  if (is_horizontal) {
    const LayoutUnit min_x = std::min(LayoutUnit(), line_box_offset.left);
    caret_location.left = std::max(caret_location.left, min_x);
    const LayoutUnit max_x =
        std::max(fragmentainer.Size().width, line_box_rect.Right());
    caret_location.left = std::min(caret_location.left, max_x - caret_width);
    caret_location.left = LayoutUnit(caret_location.left.Round());
    return PhysicalRect(caret_location, caret_size);
  }

  // Similar adjustment and rounding for vertical text.
  const LayoutUnit min_y = std::min(LayoutUnit(), line_box_offset.top);
  caret_location.top = std::max(caret_location.top, min_y);
  const LayoutUnit max_y =
      std::max(fragmentainer.Size().height, line_box_rect.Bottom());
  caret_location.top = std::min(caret_location.top, max_y - caret_height);
  caret_location.top = LayoutUnit(caret_location.top.Round());
  return PhysicalRect(caret_location, caret_size);
}

LocalCaretRect ComputeLocalCaretRect(const NGCaretPosition& caret_position) {
  if (caret_position.IsNull())
    return LocalCaretRect();

  const LayoutObject* layout_object =
      caret_position.cursor.CurrentLayoutObject();
  switch (caret_position.position_type) {
    case NGCaretPositionType::kBeforeBox:
    case NGCaretPositionType::kAfterBox: {
      DCHECK(!caret_position.cursor.IsText());
      const PhysicalRect fragment_local_rect = ComputeLocalCaretRectByBoxSide(
          caret_position.cursor, caret_position.position_type);
      return {layout_object, fragment_local_rect};
    }
    case NGCaretPositionType::kAtTextOffset: {
      DCHECK(caret_position.cursor.IsText());
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
  if (caret_position.cursor.CurrentStyle().IsHorizontalWritingMode()) {
    rect.SetY(line_box.CurrentOffset().top);
    rect.SetHeight(line_box.CurrentSize().height);
  } else {
    rect.SetX(line_box.CurrentOffset().left);
    rect.SetHeight(line_box.CurrentSize().width);
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

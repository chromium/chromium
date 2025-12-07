// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/inline/caret_rect.h"

#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/editing/ime/input_method_controller.h"
#include "third_party/blink/renderer/core/editing/local_caret_rect.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/writing_mode_converter.h"
#include "third_party/blink/renderer/core/layout/inline/inline_caret_position.h"
#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_text_combine.h"
#include "third_party/blink/renderer/core/layout/physical_box_fragment.h"
#include "third_party/blink/renderer/core/layout/text_utils.h"
#include "third_party/blink/renderer/core/style/computed_style_base_constants.h"
#include "third_party/blink/renderer/platform/text/character_break_iterator.h"
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
    case ETextAlign::kMatchParent:
      return IsRtl(direction);
  }
  NOTREACHED();
}

LayoutUnit ClampAndRound(LayoutUnit value, LayoutUnit min, LayoutUnit max) {
  LayoutUnit min_ceil = LayoutUnit(min.Ceil());
  LayoutUnit max_floor = LayoutUnit(max.Floor());
  if (min_ceil >= max_floor)
    return max_floor;
  return LayoutUnit(ClampTo<LayoutUnit>(value, min_ceil, max_floor).Round());
}

LayoutUnit ComputeCharacterWidthAtOffset(const InlineCursor& cursor,
                                         unsigned offset,
                                         const ComputedStyle& style) {
  unsigned cluster_size =
      LengthOfGraphemeCluster(cursor.CurrentText().ToString(), offset);
  // The width calculation takes into account font information from style.
  // The zoom value is baked into the font size already.
  float width = ComputeTextWidth(
      StringView(cursor.CurrentText(), offset, cluster_size), style);

  return LayoutUnit(width);
}

LogicalRect ComputeNextCharacterLogicalRect(const InlineCursor& cursor,
                                            unsigned offset,
                                            CaretShape caret_shape) {
  const LocalFrameView* frame_view =
      cursor.Current().GetLayoutObject()->GetFrameView();
  LayoutUnit caret_width = frame_view->BarCaretWidth();

  const ComputedStyle& style = cursor.Current().Style();

  WritingModeConverter converter({style.GetWritingMode(), TextDirection::kLtr},
                                 cursor.Current().Size());
  LogicalRect caret_rect;
  LayoutUnit cursor_block_size =
      converter.ToLogical(cursor.Current().Size()).block_size;

  LayoutUnit cursor_inline_size = caret_width;
  LayoutUnit cursor_block_offset;

  if (offset < cursor.Current().TextEndOffset()) {
    cursor_inline_size = ComputeCharacterWidthAtOffset(
        cursor, offset - cursor.Current().TextStartOffset(), style);
    // Fall back to 1ch.
    if (cursor_inline_size == LayoutUnit()) {
      cursor_inline_size = LayoutUnit(
          style.GetFont()->PrimaryFont()->GetFontMetrics().ZeroWidth());
    }
  } else {
    // If the next fragment is text, we need to get the width and height of
    // the first visible character in this fragment.
    auto next = cursor;
    if (!IsLtr(ResolvedDirection(cursor))) {
      next.MoveToPrevious();
    } else {
      next.MoveToNext();
    }
    if (next && next.Current().IsText() && !cursor.Current().IsLineBreak()) {
      const ComputedStyle& style_next = next.Current().Style();
      WritingModeConverter converter_next(
          {style_next.GetWritingMode(), ResolvedDirection(next)},
          next.Current().Size());
      cursor_inline_size = ComputeCharacterWidthAtOffset(next, 0, style_next);
      if (cursor_inline_size == LayoutUnit()) {
        cursor_inline_size = LayoutUnit(
            style_next.GetFont()->PrimaryFont()->GetFontMetrics().ZeroWidth());
      }
      cursor_block_size =
          converter_next.ToLogical(next.Current().Size()).block_size;
      switch (style.GetWritingMode()) {
        case WritingMode::kHorizontalTb:
          cursor_block_offset =
              next.Current().OffsetInContainerFragment().top -
              cursor.Current().OffsetInContainerFragment().top;
          break;
        case WritingMode::kVerticalRl:
        case WritingMode::kVerticalLr:
        case WritingMode::kSidewaysRl:
        case WritingMode::kSidewaysLr:
          cursor_block_offset =
              next.Current().OffsetInContainerFragment().left -
              cursor.Current().OffsetInContainerFragment().left;
          break;
      }
    } else {
      // The width of the block and underscore carets should be 1ch if
      // this information is impractical to determine.
      cursor_inline_size = LayoutUnit(
          style.GetFont()->PrimaryFont()->GetFontMetrics().ZeroWidth());
  }
  }
  caret_rect.offset.block_offset = cursor_block_offset;

  if (caret_shape == CaretShape::kBlock) {
    caret_rect.size.block_size = cursor_block_size;
    caret_rect.size.inline_size = cursor_inline_size;
  } else {
    caret_rect.size.block_size = caret_width;
    caret_rect.size.inline_size = cursor_inline_size;
    if (!IsFlippedLinesWritingMode(style.GetWritingMode())) {
      caret_rect.offset.block_offset += cursor_block_size - caret_width;
    }
  }
  return caret_rect;
}

LogicalRect ComputeLogicalCaretRectAtTextOffset(const InlineCursor& cursor,
                                                unsigned offset,
                                                CaretShape caret_shape) {
  DCHECK(cursor.Current().IsText());
  DCHECK_GE(offset, cursor.Current().TextStartOffset());
  DCHECK_LE(offset, cursor.Current().TextEndOffset());

  const LocalFrameView* frame_view =
      cursor.Current().GetLayoutObject()->GetDocument().View();
  LayoutUnit caret_width = frame_view->BarCaretWidth();

  const ComputedStyle& style = cursor.Current().Style();

  WritingModeConverter converter({style.GetWritingMode(), TextDirection::kLtr},
                                 cursor.Current().Size());
  LogicalRect caret_rect;
  LayoutUnit cursor_block_size =
      converter.ToLogical(cursor.Current().Size()).block_size;
  if (caret_shape != CaretShape::kBar) [[unlikely]] {
    // Get the width of the "next" character, or width of the last visible
    // character if there is no visible next character.
    caret_rect = ComputeNextCharacterLogicalRect(cursor, offset, caret_shape);
  } else {
    caret_rect.size.inline_size = caret_width;
    caret_rect.size.block_size = cursor_block_size;
  }

  LayoutUnit caret_left = cursor.CaretInlinePositionForOffset(offset);
  if (cursor.CurrentItem()->IsSvgText()) {
    caret_left /= cursor.CurrentItem()->SvgScalingFactor();
  }
  if (!cursor.Current().IsLineBreak() && caret_shape == CaretShape::kBar) {
    caret_left -= caret_width / 2;
  }

  caret_rect.offset.inline_offset = caret_left;

  if (caret_shape == CaretShape::kBlock ||
      caret_shape == CaretShape::kUnderscore) {
    if (!IsLtr(ResolvedDirection(cursor))) {
      caret_rect.offset.inline_offset =
          caret_left - caret_rect.size.inline_size;
    }
  }

  return caret_rect;
}

PhysicalRect ComputeLocalCaretRectAtTextOffset(const InlineCursor& cursor,
                                               unsigned offset,
                                               CaretShape caret_shape) {
  const LocalFrameView* frame_view =
      cursor.Current().GetLayoutObject()->GetFrameView();
  LayoutUnit caret_width = frame_view->BarCaretWidth();
  const ComputedStyle& style = cursor.Current().Style();
  const bool is_horizontal = style.IsHorizontalWritingMode();

  WritingModeConverter converter({style.GetWritingMode(), TextDirection::kLtr},
                                 cursor.Current().Size());
  LogicalRect caret_rect =
      ComputeLogicalCaretRectAtTextOffset(cursor, offset, caret_shape);
  PhysicalRect physical_caret_rect = converter.ToPhysical(caret_rect);

  // Adjust the location to be relative to the inline formatting context.
  PhysicalOffset caret_location =
      physical_caret_rect.offset + cursor.Current().OffsetInContainerFragment();
  const auto* const text_combine = DynamicTo<LayoutTextCombine>(
      cursor.Current().GetLayoutObject()->Parent());
  if (text_combine) [[unlikely]] {
    caret_location =
        text_combine->AdjustOffsetForLocalCaretRect(caret_location);
    if (caret_shape != CaretShape::kBar) {
      physical_caret_rect =
          text_combine->AdjustRectForBoundingBox(physical_caret_rect);
    }
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

PhysicalRect ComputeLocalCaretRectByBoxSide(
    const InlineCursor& cursor,
    InlineCaretPositionType position_type,
    CaretShape caret_shape) {
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
  caret_rect.size.inline_size = frame_view->BarCaretWidth();

  const bool is_ltr = IsLtr(ResolvedDirection(cursor));
  if (!is_atomic_inline) {
    caret_rect.offset.inline_offset = item_rect.offset.inline_offset;
  }
  if (is_ltr != (position_type == InlineCaretPositionType::kBeforeBox)) {
    caret_rect.offset.inline_offset +=
        item_rect.size.inline_size - caret_rect.size.inline_size;
  }

  if (caret_shape != CaretShape::kBar) [[unlikely]] {
    if (position_type == InlineCaretPositionType::kAfterBox) {
      auto next = cursor;
      if (!IsLtr(ResolvedDirection(cursor))) {
        next.MoveToPrevious();
      } else {
        next.MoveToNext();
      }
      if (next && next.Current().IsText()) {
        LogicalSize text_caret_size =
            ComputeLogicalCaretRectAtTextOffset(
                next, next.Current().TextStartOffset(), caret_shape)
                .size;
        switch (next.Current().Style().GetWritingMode()) {
          case WritingMode::kHorizontalTb:
            caret_rect.offset.block_offset +=
                caret_rect.size.block_size - text_caret_size.block_size;
            break;
          case WritingMode::kVerticalLr:
          case WritingMode::kVerticalRl:
            if (caret_shape == CaretShape::kBlock) {
              caret_rect.offset.block_offset +=
                  (caret_rect.size.block_size - text_caret_size.block_size) / 2;
            } else {
              if (next.Current().Style().GetWritingMode() ==
                  WritingMode::kVerticalLr) {
                caret_rect.offset.block_offset +=
                    (caret_rect.size.block_size - next.Current().Size().width) /
                    2;
              } else {
                caret_rect.offset.block_offset +=
                    (caret_rect.size.block_size + next.Current().Size().width) /
                        2 -
                    text_caret_size.block_size;
              }
            }
            break;
          case WritingMode::kSidewaysRl:
          case WritingMode::kSidewaysLr:
            // Get the half-way difference for block_size between line_rect and
            // item_rect.
            LayoutUnit adjusted_offset =
                (caret_rect.size.block_size - item_rect.size.block_size) / 2;
            if (caret_shape == CaretShape::kBlock) {
              caret_rect.offset.block_offset += caret_rect.size.block_size -
                                                text_caret_size.block_size -
                                                adjusted_offset;
            } else {
              caret_rect.offset.block_offset +=
                  caret_rect.size.block_size - adjusted_offset;
            }
            break;
        }
        caret_rect.size = text_caret_size;
      }
    }
  }

  return converter.ToPhysical(caret_rect);
}

}  // namespace

CaretShape GetCaretShapeFromComputedStyle(const ComputedStyle& style) {
  CaretShape caret_shape = CaretShape::kBar;
  if (RuntimeEnabledFeatures::CSSCaretShapeEnabled()) {
    switch (style.CaretShape()) {
      case ECaretShape::kAuto:
      case ECaretShape::kBar:
        caret_shape = CaretShape::kBar;
        break;
      case ECaretShape::kBlock:
        caret_shape = CaretShape::kBlock;
        break;
      case ECaretShape::kUnderscore:
        caret_shape = CaretShape::kUnderscore;
        break;
    }
  }
  return caret_shape;
}

LocalCaretRect ComputeLocalCaretRect(const InlineCaretPosition& caret_position,
                                     CaretShape caret_shape) {
  if (caret_position.IsNull())
    return LocalCaretRect();

  const LayoutObject* const layout_object =
      caret_position.cursor.Current().GetLayoutObject();
  // Care-shape applies to text or elements that accept text input.
  const Node* node = layout_object->GetNode();
  if (!node || !IsEditable(*node)) {
    caret_shape = CaretShape::kBar;
  }

  // Keep the caret-shape as bar during IME compositing.
  const LocalFrame* local_frame = layout_object->GetFrame();
  if (local_frame && local_frame->GetInputMethodController().HasComposition()) {
    caret_shape = CaretShape::kBar;
  }

  const PhysicalBoxFragment& container_fragment =
      caret_position.cursor.ContainerFragment();
  switch (caret_position.position_type) {
    case InlineCaretPositionType::kBeforeBox:
    case InlineCaretPositionType::kAfterBox: {
      DCHECK(!caret_position.cursor.Current().IsText());
      const PhysicalRect fragment_local_rect = ComputeLocalCaretRectByBoxSide(
          caret_position.cursor, caret_position.position_type, caret_shape);
      return {layout_object, fragment_local_rect, &container_fragment};
    }
    case InlineCaretPositionType::kAtTextOffset: {
      DCHECK(caret_position.cursor.Current().IsText());
      DCHECK(caret_position.text_offset.has_value());
      const PhysicalRect caret_rect = ComputeLocalCaretRectAtTextOffset(
          caret_position.cursor, *caret_position.text_offset, caret_shape);
      return {layout_object, caret_rect, &container_fragment};
    }
  }

  NOTREACHED();
}

LocalCaretRect ComputeLocalSelectionRect(
    const InlineCaretPosition& caret_position) {
  const LocalCaretRect caret_rect =
      ComputeLocalCaretRect(caret_position, CaretShape::kBar);
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

LogicalRect GetCaretRectAtTextOffset(const InlineCursor& cursor,
                                     unsigned text_offset,
                                     CaretShape caret_shape) {
  return ComputeLogicalCaretRectAtTextOffset(cursor, text_offset, caret_shape);
}

}  // namespace blink

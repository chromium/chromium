// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/line_relative_rect.h"

#include "third_party/blink/renderer/core/layout/inline/fragment_item.h"

namespace blink {

LineRelativeRect LineRelativeRect::EnclosingRect(const gfx::RectF& rect) {
  LineRelativeOffset offset{LayoutUnit::FromFloatFloor(rect.x()),
                            LayoutUnit::FromFloatFloor(rect.y())};
  LogicalSize size{LayoutUnit::FromFloatCeil(rect.right()) - offset.line_left,
                   LayoutUnit::FromFloatCeil(rect.bottom()) - offset.line_over};
  return {offset, size};
}

AffineTransform LineRelativeRect::ComputeRelativeToPhysicalTransform(
    WritingMode writing_mode) const {
  if (writing_mode == WritingMode::kHorizontalTb) {
    return AffineTransform();
  }

  // Constructing the matrix: consider the kVertical* case.
  //
  //      kVerticalRl
  //      kVerticalLr
  //      kSidewaysRl           kSidewaysLr
  //
  //  [A]   ooooo              [A]  °o   o°
  //       O°   °O                    °O°
  //    oooOOoooOO               °°°°°°°°°°
  //
  //       o°°°°°°                   o   o
  //       °o                       O     O
  //       °°°°°°°                  °OoooO°
  //       o     o                        O
  //       OoooooO  o            °  O°°°°°O
  //       O                        °     °
  //                                ooooooo
  //       oO°°°Oo                       °o
  //       O     O                  oooooo°
  //        °   °
  //       oooooooooo               OO°°°OO°°°
  //         oOo                    Oo   oO
  //       o°   °o                   °°°°°
  //
  // For kVerticalRl, the line relative coordinate system has the inline
  // direction running down the page and the block direction running left on
  // the page. The physical space has x running right on the page and y
  // running down. To align the inline direction with x and the block
  // direction with y, we need the rotation of:
  //   0 -1
  //   1  0
  // rotates the inline directions to physical directions.
  // The point A is at [x,y] in the physical coordinate system, and
  // [x, y + height] in the line relative space. Note that height is
  // the block direction in line relative space, and the given rect is
  // already line relative.
  // When [x, y + height] is rotated by the matrix above, a translation of
  // [x + y + height, y - x] is required to place it at [x,y].
  //
  // For the sideways cases, the rotation is
  //   0 1
  //  -1 0
  // A is at [x,y] in physical and [x + width, y] in the line relative space.

  return writing_mode != WritingMode::kSidewaysLr
             ? AffineTransform(0, 1, -1, 0,
                               LineLeft() + LineOver() + BlockSize(),
                               LineOver() - LineLeft())
             : AffineTransform(0, -1, 1, 0, LineLeft() - LineOver(),
                               LineLeft() + LineOver() + InlineSize());
}

LineRelativeRect LineRelativeRect::EnclosingLineRelativeRect() {
  int left = FloorToInt(offset.line_left);
  int top = FloorToInt(offset.line_over);
  int max_right = (offset.line_left + size.inline_size).Ceil();
  int max_bottom = (offset.line_over + size.block_size).Ceil();
  return {{LayoutUnit(left), LayoutUnit(top)},
          {LayoutUnit(max_right - left), LayoutUnit(max_bottom - top)}};
}

// Shift up the inline-start edge and the block-start by `d`, and
// shift down the inline-end edge and the block-end edge by `d`.
void LineRelativeRect::Inflate(LayoutUnit d) {
  offset.line_left -= d;
  size.inline_size += d * 2;
  offset.line_over -= d;
  size.block_size += d * 2;
}

void LineRelativeRect::Unite(const LineRelativeRect& other) {
  // Based on PhysicalRect::UniteEvenIfEmpty
  LayoutUnit left = std::min(offset.line_left, other.offset.line_left);
  LayoutUnit top = std::min(offset.line_over, other.offset.line_over);
  LayoutUnit right = std::max(offset.line_left + size.inline_size,
                              other.offset.line_left + other.size.inline_size);
  LayoutUnit bottom = std::max(offset.line_over + size.block_size,
                               other.offset.line_over + other.size.block_size);
  size = {right - left, bottom - top};
  offset = {right - size.inline_size, bottom - size.block_size};
}

void LineRelativeRect::AdjustLineStartToInkOverflow(
    const FragmentItem& fragment) {
  WritingMode writing_mode = fragment.GetWritingMode();
  // Offset from the inline-start position of `fragment`.
  // It should be negative or zero.
  LayoutUnit ink_left(0);
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      ink_left = fragment.InkOverflowRect().X();
      break;
    case WritingMode::kVerticalRl:
    case WritingMode::kVerticalLr:
    case WritingMode::kSidewaysRl:
      ink_left = fragment.InkOverflowRect().Y();
      break;
    case WritingMode::kSidewaysLr:
      ink_left = fragment.Size().height - fragment.InkOverflowRect().Bottom();
      break;
  }
  offset.line_left += ink_left;
  size.inline_size -= ink_left;
}

void LineRelativeRect::AdjustLineEndToInkOverflow(
    const FragmentItem& fragment) {
  WritingMode writing_mode = fragment.GetWritingMode();
  // Offset from the inline-start position of `fragment`.
  // It should be equal to or greater than the inline-size of `fragment`.
  LayoutUnit ink_right(0);
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      ink_right = fragment.InkOverflowRect().Right();
      break;
    case WritingMode::kVerticalRl:
    case WritingMode::kVerticalLr:
    case WritingMode::kSidewaysRl:
      ink_right = fragment.InkOverflowRect().Bottom();
      break;
    case WritingMode::kSidewaysLr:
      ink_right = fragment.Size().height - fragment.InkOverflowRect().Y();
      break;
  }
  if (fragment.IsSvgText()) [[unlikely]] {
    // SVG InkOverflow is before scaling.
    ink_right *= fragment.SvgScalingFactor();
  }
  size.inline_size = ink_right;
}

}  // namespace blink

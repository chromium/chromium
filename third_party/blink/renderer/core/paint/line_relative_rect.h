// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LINE_RELATIVE_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LINE_RELATIVE_RECT_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/layout/geometry/logical_size.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_size.h"
#include "third_party/blink/renderer/platform/geometry/layout_unit.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"
#include "ui/gfx/geometry/rect_f.h"

namespace blink {

// Type-safe geometry for line-relative coordinate spaces.
//
// When painting text fragments in a vertical writing mode (where ‘writing-mode’
// is vertical or sideways), we rotate the canvas into a line-relative
// coordinate space, where +x is line-right and +y is line-under.
//
// Paint ops done while rotated (like text and text decorations) need
// coordinates in this rotated space, but ops done outside of these rotations
// (like selection backgrounds) need coordinates in the original physical space.
//
// Note that the bi-orientational transform for upright typesetting (see
// ‘text-orientation’) is handled by the lower-level text painting code with a
// nested rotation (CanvasRotationInVertical), which can be ignored at the
// painter level.

// 2D point or vector in line-relative space (physical space rotated for
// ‘writing-mode’), like gfx::PointF or gfx::Vector2dF but in fixed-point
// coordinates (LayoutUnit).
struct CORE_EXPORT LineRelativeOffset {
  LayoutUnit line_left;
  LayoutUnit line_over;

  LineRelativeOffset(LayoutUnit line_left, LayoutUnit line_over)
      : line_left(line_left), line_over(line_over) {}

  // Map a physical offset of a line box to line-relative space, by reusing the
  // offset coordinates (physical top-left). Line-relative space is defined such
  // that the origin of the line box is the same in both the line relative
  // coordinate system and the physical coordinate system, regardless of the
  // writing flow.
  static LineRelativeOffset CreateFromBoxOrigin(const PhysicalOffset& origin) {
    return {origin.left, origin.top};
  }

  constexpr explicit operator gfx::PointF() const {
    return {line_left, line_over};
  }
  LineRelativeOffset operator+(const LineRelativeOffset& other) const {
    return {line_left + other.line_left, line_over + other.line_over};
  }
};

// TODO(crbug.com/962299): These functions should upgraded to force correct
// pixel snapping in a type-safe way.
inline gfx::Point ToRoundedPoint(const LineRelativeOffset& o) {
  return {o.line_left.Round(), o.line_over.Round()};
}

// 2D rect in line-relative space (physical space rotated for ‘writing-mode’),
// like gfx::RectF but in fixed-point coordinates (LayoutUnit).
struct CORE_EXPORT LineRelativeRect {
  LineRelativeOffset offset;
  LogicalSize size;

  static LineRelativeRect EnclosingRect(const gfx::RectF& rect) {
    LineRelativeOffset offset{LayoutUnit::FromFloatFloor(rect.x()),
                              LayoutUnit::FromFloatFloor(rect.y())};
    LogicalSize size{
        LayoutUnit::FromFloatCeil(rect.right()) - offset.line_left,
        LayoutUnit::FromFloatCeil(rect.bottom()) - offset.line_over};
    return {offset, size};
  }

  // Map a physical rect line box to line-relative space, by reusing the offset
  // coordinates and (if not horizontal) swapping width and height.
  //
  // To explain why this shortcut is correct (for the line box only: during
  // paint ops, the line box is specifically rotated such that the top left
  // corner of the box before and after rotation has the same x, y coordinate):
  // When the direction is clockwise (kVertical* and kSidewaysRl), the
  // line-left-under (line-right-over) corner moves to the top-left corner [A],
  // while the line-left-over corner moves to the top-right (bottom-left) corner
  // [B].
  //
  // In both cases, the rotation is around some arbitrary third point [C], but
  // the coordinates of [B] in rotated space are the same as the coordinates of
  // [A] in physical space, which means that the line box can be mapped between
  // these spaces by swapping width and height only.
  //
  //      clockwise            counter-clockwise
  //
  //  [A]   ooooo    [B]       [A]  °o   o°
  //       O°   °O                    °O°
  //    oooOOoooOO               °°°°°°°°°°
  //          [C]
  //       o°°°°°°                   o   o
  //       °o                       O     O
  //       °°°°°°°                  °OoooO°
  //       o     o                        O
  //       OoooooO  o            °  O°°°°°O       [C]
  //       O                        °     °
  //                                ooooooo
  //       oO°°°Oo                       °o
  //       O     O                  oooooo°
  //        °   °
  //       oooooooooo               OO°°°OO°°°
  //         oOo                    Oo   oO
  //       o°   °o             [B]   °°°°°
  //
  static LineRelativeRect CreateFromLineBox(const PhysicalRect& rect,
                                            bool is_horizontal) {
    return {LineRelativeOffset::CreateFromBoxOrigin(rect.offset),
            LogicalSize{is_horizontal ? rect.size.width : rect.size.height,
                        is_horizontal ? rect.size.height : rect.size.width}};
  }

  // Map a physical rect that may be line box or contained text fragment
  // to line-relative space, by mapping it through the inverse of the given
  // rotation matrix (see ComputeRelativeToPhysicalTransform).
  static LineRelativeRect Create(
      const PhysicalRect& rect,
      const absl::optional<AffineTransform>& rotation) {
    if (!rotation || rotation == AffineTransform()) {
      return {{rect.offset.left, rect.offset.top},
              {rect.size.width, rect.size.height}};
    }
    return EnclosingRect(rotation->Inverse().MapRect(gfx::RectF{rect}));
  }

  constexpr explicit operator gfx::RectF() const {
    return {offset.line_left, offset.line_over, size.inline_size,
            size.block_size};
  }
  LineRelativeRect operator+(const LineRelativeOffset& other) const {
    return {offset + other, size};
  }

  constexpr LayoutUnit LineLeft() const { return offset.line_left; }
  constexpr LayoutUnit LineOver() const { return offset.line_over; }
  constexpr LayoutUnit InlineSize() const { return size.inline_size; }
  constexpr LayoutUnit BlockSize() const { return size.block_size; }

  void Move(const LineRelativeOffset& other) {
    offset.line_left += other.line_left;
    offset.line_over += other.line_over;
  }

  // TODO(crbug.com/962299): These functions should upgraded to force correct
  // pixel snapping in a type-safe way.
  gfx::Point PixelSnappedOffset() const { return ToRoundedPoint(offset); }
  int PixelSnappedInlineSize() const {
    return SnapSizeToPixel(size.inline_size, offset.line_left);
  }
  int PixelSnappedBlockSize() const {
    return SnapSizeToPixel(size.block_size, offset.line_over);
  }
  gfx::Size PixelSnappedSize() const {
    return {PixelSnappedInlineSize(), PixelSnappedBlockSize()};
  }

  // Returns the transformation that would rotate the canvas in the appropriate
  // direction for a vertical writing mode, while keeping the physical top-left
  // corner of the given line box in the same place (changing the coordinate
  // while keeping the box in the same place on the page).
  AffineTransform ComputeRelativeToPhysicalTransform(
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
};

// TODO(crbug.com/962299): These functions should upgraded to force correct
// pixel snapping in a type-safe way.
inline gfx::Rect ToPixelSnappedRect(const LineRelativeRect& r) {
  return {r.PixelSnappedOffset(), r.PixelSnappedSize()};
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_LINE_RELATIVE_RECT_H_

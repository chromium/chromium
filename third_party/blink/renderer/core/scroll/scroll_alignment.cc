/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@fas.harvard.edu>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Randall Jesup <rjesup@wgate.com>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *   Josh Soref <timeless@mac.com>
 *   Boris Zbarsky <bzbarsky@mit.edu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 *
 * Alternatively, the contents of this file may be used under the terms
 * of either the Mozilla Public License Version 1.1, found at
 * http://www.mozilla.org/MPL/ (the "MPL") or the GNU General Public
 * License Version 2.0, found at http://www.fsf.org/copyleft/gpl.html
 * (the "GPL"), in which case the provisions of the MPL or the GPL are
 * applicable instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of one of those two
 * licenses (the MPL or the GPL) and not to allow others to use your
 * version of this file under the LGPL, indicate your decision by
 * deletingthe provisions above and replace them with the notice and
 * other provisions required by the MPL or the GPL, as the case may be.
 * If you do not delete the provisions above, a recipient may use your
 * version of this file under any of the LGPL, the MPL or the GPL.
 */

#include "third_party/blink/renderer/core/scroll/scroll_alignment.h"

#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"

namespace blink {

const ScrollAlignment ScrollAlignment::kAlignCenterIfNeeded = {
    kScrollAlignmentNoScroll, kScrollAlignmentCenter,
    kScrollAlignmentClosestEdge};
const ScrollAlignment ScrollAlignment::kAlignToEdgeIfNeeded = {
    kScrollAlignmentNoScroll, kScrollAlignmentClosestEdge,
    kScrollAlignmentClosestEdge};
const ScrollAlignment ScrollAlignment::kAlignCenterAlways = {
    kScrollAlignmentCenter, kScrollAlignmentCenter, kScrollAlignmentCenter};
const ScrollAlignment ScrollAlignment::kAlignTopAlways = {
    kScrollAlignmentTop, kScrollAlignmentTop, kScrollAlignmentTop};
const ScrollAlignment ScrollAlignment::kAlignBottomAlways = {
    kScrollAlignmentBottom, kScrollAlignmentBottom, kScrollAlignmentBottom};
const ScrollAlignment ScrollAlignment::kAlignLeftAlways = {
    kScrollAlignmentLeft, kScrollAlignmentLeft, kScrollAlignmentLeft};
const ScrollAlignment ScrollAlignment::kAlignRightAlways = {
    kScrollAlignmentRight, kScrollAlignmentRight, kScrollAlignmentRight};

#define MIN_INTERSECT_FOR_REVEAL 32

ScrollOffset ScrollAlignment::GetScrollOffsetToExpose(
    const PhysicalRect& scroll_snapport_rect,
    const PhysicalRect& expose_rect,
    const ScrollAlignment& align_x,
    const ScrollAlignment& align_y,
    const ScrollOffset& current_scroll_offset) {
  // Prevent degenerate cases by giving the visible rect a minimum non-0 size.
  PhysicalRect non_zero_visible_rect = scroll_snapport_rect;
  LayoutUnit minimum_layout_unit;
  minimum_layout_unit.SetRawValue(1);
  if (non_zero_visible_rect.Width() == LayoutUnit())
    non_zero_visible_rect.SetWidth(minimum_layout_unit);
  if (non_zero_visible_rect.Height() == LayoutUnit())
    non_zero_visible_rect.SetHeight(minimum_layout_unit);

  // Determine the appropriate X behavior.
  ScrollAlignmentBehavior scroll_x;
  PhysicalRect expose_rect_x(expose_rect.X(), non_zero_visible_rect.Y(),
                             expose_rect.Width(),
                             non_zero_visible_rect.Height());
  LayoutUnit intersect_width =
      Intersection(non_zero_visible_rect, expose_rect_x).Width();
  if (intersect_width == expose_rect.Width() ||
      intersect_width >= MIN_INTERSECT_FOR_REVEAL) {
    // If the rectangle is fully visible, use the specified visible behavior.
    // If the rectangle is partially visible, but over a certain threshold,
    // then treat it as fully visible to avoid unnecessary horizontal scrolling
    scroll_x = GetVisibleBehavior(align_x);
  } else if (intersect_width == non_zero_visible_rect.Width()) {
    // If the rect is bigger than the visible area, don't bother trying to
    // center. Other alignments will work.
    scroll_x = GetVisibleBehavior(align_x);
    if (scroll_x == kScrollAlignmentCenter)
      scroll_x = kScrollAlignmentNoScroll;
  } else if (intersect_width > 0) {
    // If the rectangle is partially visible, but not above the minimum
    // threshold, use the specified partial behavior
    scroll_x = GetPartialBehavior(align_x);
  } else {
    scroll_x = GetHiddenBehavior(align_x);
  }

  if (scroll_x == kScrollAlignmentClosestEdge) {
    // Closest edge is the right in two cases:
    // (1) exposeRect to the right of and smaller than nonZeroVisibleRect
    // (2) exposeRect to the left of and larger than nonZeroVisibleRect
    if ((expose_rect.Right() > non_zero_visible_rect.Right() &&
         expose_rect.Width() < non_zero_visible_rect.Width()) ||
        (expose_rect.Right() < non_zero_visible_rect.Right() &&
         expose_rect.Width() > non_zero_visible_rect.Width())) {
      scroll_x = kScrollAlignmentRight;
    }
  }

  // Determine the appropriate Y behavior.
  ScrollAlignmentBehavior scroll_y;
  PhysicalRect expose_rect_y(non_zero_visible_rect.X(), expose_rect.Y(),
                             non_zero_visible_rect.Width(),
                             expose_rect.Height());
  LayoutUnit intersect_height =
      Intersection(non_zero_visible_rect, expose_rect_y).Height();
  if (intersect_height == expose_rect.Height()) {
    // If the rectangle is fully visible, use the specified visible behavior.
    scroll_y = GetVisibleBehavior(align_y);
  } else if (intersect_height == non_zero_visible_rect.Height()) {
    // If the rect is bigger than the visible area, don't bother trying to
    // center. Other alignments will work.
    scroll_y = GetVisibleBehavior(align_y);
    if (scroll_y == kScrollAlignmentCenter)
      scroll_y = kScrollAlignmentNoScroll;
  } else if (intersect_height > 0) {
    // If the rectangle is partially visible, use the specified partial behavior
    scroll_y = GetPartialBehavior(align_y);
  } else {
    scroll_y = GetHiddenBehavior(align_y);
  }

  if (scroll_y == kScrollAlignmentClosestEdge) {
    // Closest edge is the bottom in two cases:
    // (1) exposeRect below and smaller than nonZeroVisibleRect
    // (2) exposeRect above and larger than nonZeroVisibleRect
    if ((expose_rect.Bottom() > non_zero_visible_rect.Bottom() &&
         expose_rect.Height() < non_zero_visible_rect.Height()) ||
        (expose_rect.Bottom() < non_zero_visible_rect.Bottom() &&
         expose_rect.Height() > non_zero_visible_rect.Height())) {
      scroll_y = kScrollAlignmentBottom;
    }
  }

  // We would like calculate the ScrollPosition to move |expose_rect| inside
  // the scroll_snapport, which is based on the scroll_origin of the scroller.
  non_zero_visible_rect.Move(
      -PhysicalOffset::FromFloatSizeRound(current_scroll_offset));

  // Given the X behavior, compute the X coordinate.
  float x;
  if (scroll_x == kScrollAlignmentNoScroll) {
    x = current_scroll_offset.Width();
  } else if (scroll_x == kScrollAlignmentRight) {
    x = (expose_rect.Right() - non_zero_visible_rect.Right()).ToFloat();
  } else if (scroll_x == kScrollAlignmentCenter) {
    x = ((expose_rect.X() + expose_rect.Right() -
          (non_zero_visible_rect.X() + non_zero_visible_rect.Right())) /
         2)
            .ToFloat();
  } else {
    x = (expose_rect.X() - non_zero_visible_rect.X()).ToFloat();
  }

  // Given the Y behavior, compute the Y coordinate.
  float y;
  if (scroll_y == kScrollAlignmentNoScroll) {
    y = current_scroll_offset.Height();
  } else if (scroll_y == kScrollAlignmentBottom) {
    y = (expose_rect.Bottom() - non_zero_visible_rect.Bottom()).ToFloat();
  } else if (scroll_y == kScrollAlignmentCenter) {
    y = ((expose_rect.Y() + expose_rect.Bottom() -
          (non_zero_visible_rect.Y() + non_zero_visible_rect.Bottom())) /
         2)
            .ToFloat();
  } else {
    y = (expose_rect.Y() - non_zero_visible_rect.Y()).ToFloat();
  }

  return ScrollOffset(x, y);
}

}  // namespace blink

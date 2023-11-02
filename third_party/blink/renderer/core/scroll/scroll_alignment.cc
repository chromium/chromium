/*
 * Copyright (C) 2006, 2007, 2008 Apple Inc. All rights reserved.
 *
 * Portions are Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Other contributors:
 *   Robert O'Callahan <roc+@cs.cmu.edu>
 *   David Baron <dbaron@dbaron.org>
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

#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_scroll_into_view_options.h"
#include "third_party/blink/renderer/core/layout/geometry/physical_rect.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/text/writing_mode.h"

namespace blink {

// static
ScrollOffset ScrollAlignment::GetScrollOffsetToExpose(
    const PhysicalRect& scroll_snapport_rect,
    const PhysicalRect& expose_rect,
    const mojom::blink::ScrollAlignment& align_x,
    const mojom::blink::ScrollAlignment& align_y,
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
  mojom::blink::ScrollAlignment::Behavior scroll_x;
  PhysicalRect expose_rect_x(expose_rect.X(), non_zero_visible_rect.Y(),
                             expose_rect.Width(),
                             non_zero_visible_rect.Height());
  LayoutUnit intersect_width =
      Intersection(non_zero_visible_rect, expose_rect_x).Width();
  if (intersect_width == expose_rect.Width()) {
    // If the rectangle is fully visible, use the specified visible behavior.
    // If the rectangle is partially visible, but over a certain threshold,
    // then treat it as fully visible to avoid unnecessary horizontal scrolling
    scroll_x = align_x.rect_visible;
  } else if (intersect_width == non_zero_visible_rect.Width()) {
    // If the rect is bigger than the visible area, don't bother trying to
    // center. Other alignments will work.
    scroll_x = align_x.rect_visible;
    if (scroll_x == mojom::blink::ScrollAlignment::Behavior::kCenter)
      scroll_x = mojom::blink::ScrollAlignment::Behavior::kNoScroll;
  } else if (intersect_width > 0) {
    // If the rectangle is partially visible, but not above the minimum
    // threshold, use the specified partial behavior
    scroll_x = align_x.rect_partial;
  } else {
    scroll_x = align_x.rect_hidden;
  }

  if (scroll_x == mojom::blink::ScrollAlignment::Behavior::kClosestEdge) {
    // Closest edge is the right in two cases:
    // (1) exposeRect to the right of and smaller than nonZeroVisibleRect
    // (2) exposeRect to the left of and larger than nonZeroVisibleRect
    if ((expose_rect.Right() > non_zero_visible_rect.Right() &&
         expose_rect.Width() < non_zero_visible_rect.Width()) ||
        (expose_rect.Right() < non_zero_visible_rect.Right() &&
         expose_rect.Width() > non_zero_visible_rect.Width())) {
      scroll_x = mojom::blink::ScrollAlignment::Behavior::kRight;
    }
  }

  // Determine the appropriate Y behavior.
  mojom::blink::ScrollAlignment::Behavior scroll_y;
  PhysicalRect expose_rect_y(non_zero_visible_rect.X(), expose_rect.Y(),
                             non_zero_visible_rect.Width(),
                             expose_rect.Height());
  LayoutUnit intersect_height =
      Intersection(non_zero_visible_rect, expose_rect_y).Height();
  if (intersect_height == expose_rect.Height()) {
    // If the rectangle is fully visible, use the specified visible behavior.
    scroll_y = align_y.rect_visible;
  } else if (intersect_height == non_zero_visible_rect.Height()) {
    // If the rect is bigger than the visible area, don't bother trying to
    // center. Other alignments will work.
    scroll_y = align_y.rect_visible;
    if (scroll_y == mojom::blink::ScrollAlignment::Behavior::kCenter)
      scroll_y = mojom::blink::ScrollAlignment::Behavior::kNoScroll;
  } else if (intersect_height > 0) {
    // If the rectangle is partially visible, use the specified partial behavior
    scroll_y = align_y.rect_partial;
  } else {
    scroll_y = align_y.rect_hidden;
  }

  if (scroll_y == mojom::blink::ScrollAlignment::Behavior::kClosestEdge) {
    // Closest edge is the bottom in two cases:
    // (1) exposeRect below and smaller than nonZeroVisibleRect
    // (2) exposeRect above and larger than nonZeroVisibleRect
    if ((expose_rect.Bottom() > non_zero_visible_rect.Bottom() &&
         expose_rect.Height() < non_zero_visible_rect.Height()) ||
        (expose_rect.Bottom() < non_zero_visible_rect.Bottom() &&
         expose_rect.Height() > non_zero_visible_rect.Height())) {
      scroll_y = mojom::blink::ScrollAlignment::Behavior::kBottom;
    }
  }

  // We would like calculate the ScrollPosition to move |expose_rect| inside
  // the scroll_snapport, which is based on the scroll_origin of the scroller.
  non_zero_visible_rect.Move(
      -PhysicalOffset::FromVector2dFRound(current_scroll_offset));

  // Given the X behavior, compute the X coordinate.
  float x;
  if (scroll_x == mojom::blink::ScrollAlignment::Behavior::kNoScroll) {
    x = current_scroll_offset.x();
  } else if (scroll_x == mojom::blink::ScrollAlignment::Behavior::kRight) {
    x = (expose_rect.Right() - non_zero_visible_rect.Right()).ToFloat();
  } else if (scroll_x == mojom::blink::ScrollAlignment::Behavior::kCenter) {
    x = ((expose_rect.X() + expose_rect.Right() -
          (non_zero_visible_rect.X() + non_zero_visible_rect.Right())) /
         2)
            .ToFloat();
  } else {
    x = (expose_rect.X() - non_zero_visible_rect.X()).ToFloat();
  }

  // Given the Y behavior, compute the Y coordinate.
  float y;
  if (scroll_y == mojom::blink::ScrollAlignment::Behavior::kNoScroll) {
    y = current_scroll_offset.y();
  } else if (scroll_y == mojom::blink::ScrollAlignment::Behavior::kBottom) {
    y = (expose_rect.Bottom() - non_zero_visible_rect.Bottom()).ToFloat();
  } else if (scroll_y == mojom::blink::ScrollAlignment::Behavior::kCenter) {
    y = ((expose_rect.Y() + expose_rect.Bottom() -
          (non_zero_visible_rect.Y() + non_zero_visible_rect.Bottom())) /
         2)
            .ToFloat();
  } else {
    y = (expose_rect.Y() - non_zero_visible_rect.Y()).ToFloat();
  }

  return ScrollOffset(x, y);
}

// static
const mojom::blink::ScrollAlignment& ScrollAlignment::CenterIfNeeded() {
  DEFINE_STATIC_LOCAL(const mojom::blink::ScrollAlignment,
                      g_scroll_align_center_if_needed,
                      (mojom::blink::ScrollAlignment::Behavior::kNoScroll,
                       mojom::blink::ScrollAlignment::Behavior::kCenter,
                       mojom::blink::ScrollAlignment::Behavior::kClosestEdge));
  return g_scroll_align_center_if_needed;
}

// static
const mojom::blink::ScrollAlignment& ScrollAlignment::ToEdgeIfNeeded() {
  DEFINE_STATIC_LOCAL(const mojom::blink::ScrollAlignment,
                      g_scroll_align_to_edge_if_needed,
                      (mojom::blink::ScrollAlignment::Behavior::kNoScroll,
                       mojom::blink::ScrollAlignment::Behavior::kClosestEdge,
                       mojom::blink::ScrollAlignment::Behavior::kClosestEdge));
  return g_scroll_align_to_edge_if_needed;
}

// static
const mojom::blink::ScrollAlignment& ScrollAlignment::CenterAlways() {
  DEFINE_STATIC_LOCAL(const mojom::blink::ScrollAlignment,
                      g_scroll_align_center_always,
                      (mojom::blink::ScrollAlignment::Behavior::kCenter,
                       mojom::blink::ScrollAlignment::Behavior::kCenter,
                       mojom::blink::ScrollAlignment::Behavior::kCenter));
  return g_scroll_align_center_always;
}

// static
const mojom::blink::ScrollAlignment& ScrollAlignment::TopAlways() {
  DEFINE_STATIC_LOCAL(const mojom::blink::ScrollAlignment,
                      g_scroll_align_top_always,
                      (mojom::blink::ScrollAlignment::Behavior::kTop,
                       mojom::blink::ScrollAlignment::Behavior::kTop,
                       mojom::blink::ScrollAlignment::Behavior::kTop));
  return g_scroll_align_top_always;
}

// static
const mojom::blink::ScrollAlignment& ScrollAlignment::BottomAlways() {
  DEFINE_STATIC_LOCAL(const mojom::blink::ScrollAlignment,
                      g_scroll_align_bottom_always,
                      (mojom::blink::ScrollAlignment::Behavior::kBottom,
                       mojom::blink::ScrollAlignment::Behavior::kBottom,
                       mojom::blink::ScrollAlignment::Behavior::kBottom));
  return g_scroll_align_bottom_always;
}

// static
const mojom::blink::ScrollAlignment& ScrollAlignment::LeftAlways() {
  DEFINE_STATIC_LOCAL(const mojom::blink::ScrollAlignment,
                      g_scroll_align_left_always,
                      (mojom::blink::ScrollAlignment::Behavior::kLeft,
                       mojom::blink::ScrollAlignment::Behavior::kLeft,
                       mojom::blink::ScrollAlignment::Behavior::kLeft));
  return g_scroll_align_left_always;
}

// static
const mojom::blink::ScrollAlignment& ScrollAlignment::RightAlways() {
  DEFINE_STATIC_LOCAL(const mojom::blink::ScrollAlignment,
                      g_scroll_align_right_always,
                      (mojom::blink::ScrollAlignment::Behavior::kRight,
                       mojom::blink::ScrollAlignment::Behavior::kRight,
                       mojom::blink::ScrollAlignment::Behavior::kRight));
  return g_scroll_align_right_always;
}

// static
mojom::blink::ScrollIntoViewParamsPtr
ScrollAlignment::CreateScrollIntoViewParams(
    const mojom::blink::ScrollAlignment& align_x,
    const mojom::blink::ScrollAlignment& align_y,
    mojom::blink::ScrollType scroll_type,
    bool make_visible_in_visual_viewport,
    mojom::blink::ScrollBehavior scroll_behavior,
    bool is_for_scroll_sequence,
    bool cross_origin_boundaries) {
  auto params = mojom::blink::ScrollIntoViewParams::New();
  params->align_x = mojom::blink::ScrollAlignment::New(align_x);
  params->align_y = mojom::blink::ScrollAlignment::New(align_y);
  params->type = scroll_type;
  params->make_visible_in_visual_viewport = make_visible_in_visual_viewport;
  params->behavior = scroll_behavior;
  params->is_for_scroll_sequence = is_for_scroll_sequence;
  params->cross_origin_boundaries = cross_origin_boundaries;
  return params;
}

namespace {
mojom::blink::ScrollAlignment AlignmentFromOptions(
    const ScrollIntoViewOptions& options,
    ScrollOrientation axis,
    const ComputedStyle& computed_style) {
  WritingMode writing_mode = computed_style.GetWritingMode();
  bool is_ltr = computed_style.IsLeftToRightDirection();

  bool is_horizontal_writing_mode = IsHorizontalWritingMode(writing_mode);
  String alignment =
      ((axis == kHorizontalScroll && is_horizontal_writing_mode) ||
       (axis == kVerticalScroll && !is_horizontal_writing_mode))
          ? options.inlinePosition()
          : options.block();

  if (alignment == "center")
    return ScrollAlignment::CenterAlways();
  if (alignment == "nearest")
    return ScrollAlignment::ToEdgeIfNeeded();
  if (alignment == "start") {
    if (axis == kHorizontalScroll) {
      switch (writing_mode) {
        case WritingMode::kHorizontalTb:
          return is_ltr ? ScrollAlignment::LeftAlways()
                        : ScrollAlignment::RightAlways();
        case WritingMode::kVerticalRl:
        case WritingMode::kSidewaysRl:
          return ScrollAlignment::RightAlways();
        case WritingMode::kVerticalLr:
        case WritingMode::kSidewaysLr:
          return ScrollAlignment::LeftAlways();
        default:
          NOTREACHED();
          return ScrollAlignment::LeftAlways();
      }
    } else {
      switch (writing_mode) {
        case WritingMode::kHorizontalTb:
          return ScrollAlignment::TopAlways();
        case WritingMode::kVerticalRl:
        case WritingMode::kSidewaysRl:
        case WritingMode::kVerticalLr:
          return is_ltr ? ScrollAlignment::TopAlways()
                        : ScrollAlignment::BottomAlways();
        case WritingMode::kSidewaysLr:
          return is_ltr ? ScrollAlignment::BottomAlways()
                        : ScrollAlignment::TopAlways();
        default:
          NOTREACHED();
          return ScrollAlignment::TopAlways();
      }
    }
  }
  if (alignment == "end") {
    if (axis == kHorizontalScroll) {
      switch (writing_mode) {
        case WritingMode::kHorizontalTb:
          return is_ltr ? ScrollAlignment::RightAlways()
                        : ScrollAlignment::LeftAlways();
        case WritingMode::kVerticalRl:
        case WritingMode::kSidewaysRl:
          return ScrollAlignment::LeftAlways();
        case WritingMode::kVerticalLr:
        case WritingMode::kSidewaysLr:
          return ScrollAlignment::RightAlways();
        default:
          NOTREACHED();
          return ScrollAlignment::RightAlways();
      }
    } else {
      switch (writing_mode) {
        case WritingMode::kHorizontalTb:
          return ScrollAlignment::BottomAlways();
        case WritingMode::kVerticalRl:
        case WritingMode::kSidewaysRl:
        case WritingMode::kVerticalLr:
          return is_ltr ? ScrollAlignment::BottomAlways()
                        : ScrollAlignment::TopAlways();
        case WritingMode::kSidewaysLr:
          return is_ltr ? ScrollAlignment::TopAlways()
                        : ScrollAlignment::BottomAlways();
        default:
          NOTREACHED();
          return ScrollAlignment::BottomAlways();
      }
    }
  }

  // Default values
  if (is_horizontal_writing_mode) {
    return (axis == kHorizontalScroll) ? ScrollAlignment::ToEdgeIfNeeded()
                                       : ScrollAlignment::TopAlways();
  }
  return (axis == kHorizontalScroll) ? ScrollAlignment::LeftAlways()
                                     : ScrollAlignment::ToEdgeIfNeeded();
}
}  // namespace

// static
mojom::blink::ScrollIntoViewParamsPtr
ScrollAlignment::CreateScrollIntoViewParams(
    const ScrollIntoViewOptions& options,
    const ComputedStyle& computed_style) {
  mojom::blink::ScrollBehavior behavior =
      (options.behavior() == "smooth") ? mojom::blink::ScrollBehavior::kSmooth
                                       : mojom::blink::ScrollBehavior::kAuto;

  auto align_x =
      AlignmentFromOptions(options, kHorizontalScroll, computed_style);
  auto align_y = AlignmentFromOptions(options, kVerticalScroll, computed_style);

  mojom::blink::ScrollIntoViewParamsPtr params =
      ScrollAlignment::CreateScrollIntoViewParams(align_x, align_y);
  params->behavior = behavior;
  return params;
}

}  // namespace blink

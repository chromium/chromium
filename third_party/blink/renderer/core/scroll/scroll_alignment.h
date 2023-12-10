/*
 * Copyright (C) 2003, 2009 Apple Inc.  All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_ALIGNMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_ALIGNMENT_H_

#include "third_party/blink/public/mojom/scroll/scroll_into_view_params.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/scroll/scroll_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class ComputedStyle;
struct PhysicalBoxStrut;
struct PhysicalRect;
class ScrollIntoViewOptions;

class CORE_EXPORT ScrollAlignment {
 public:
  // Returns the scroll offset the scroller needs to scroll to in order to put
  // |expose_rect| into |visible_scroll_snapport_rect| aligned by |align_x| and
  // |align_y|.
  // The coordinates for |visible_scroll_snapport_rect|, |expose_rect| and
  // |current_scroll_position| are based on the scroller's scroll_origin
  // Note that the |current_scroll_offset| is not the location of
  // |visible_scroll_snapport_rect|, as |visible_scroll_snapport_rect| is the
  // visible rect contracted by its scroll-padding.
  // FIXME: This function should probably go somewhere else but where?
  static ScrollOffset GetScrollOffsetToExpose(
      const PhysicalRect& visible_scroll_snapport_rect,
      const PhysicalRect& expose_rect,
      const PhysicalBoxStrut& expose_scroll_margin,
      const mojom::blink::ScrollAlignment& align_x,
      const mojom::blink::ScrollAlignment& align_y,
      const ScrollOffset& current_scroll_offset);

  static const mojom::blink::ScrollAlignment& CenterIfNeeded();
  static const mojom::blink::ScrollAlignment& ToEdgeIfNeeded();
  static const mojom::blink::ScrollAlignment& CenterAlways();
  static const mojom::blink::ScrollAlignment& TopAlways();
  static const mojom::blink::ScrollAlignment& BottomAlways();
  static const mojom::blink::ScrollAlignment& LeftAlways();
  static const mojom::blink::ScrollAlignment& RightAlways();

  static mojom::blink::ScrollIntoViewParamsPtr CreateScrollIntoViewParams(
      const mojom::blink::ScrollAlignment& align_x = CenterIfNeeded(),
      const mojom::blink::ScrollAlignment& align_y = CenterIfNeeded(),
      mojom::blink::ScrollType scroll_type =
          mojom::blink::ScrollType::kProgrammatic,
      bool make_visible_in_visual_viewport = true,
      mojom::blink::ScrollBehavior scroll_behavior =
          mojom::blink::ScrollBehavior::kAuto,
      bool is_for_scroll_sequence = false,
      bool cross_origin_boundaries = true);

  static mojom::blink::ScrollIntoViewParamsPtr CreateScrollIntoViewParams(
      const ScrollIntoViewOptions& options,
      const ComputedStyle& computed_style);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_SCROLL_SCROLL_ALIGNMENT_H_

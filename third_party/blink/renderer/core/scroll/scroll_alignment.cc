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

namespace blink {

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

}  // namespace blink

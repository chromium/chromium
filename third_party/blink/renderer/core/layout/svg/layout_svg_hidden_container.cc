/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/layout/svg/layout_svg_hidden_container.h"

#include "third_party/blink/renderer/core/layout/svg/svg_layout_info.h"

namespace blink {

LayoutSVGHiddenContainer::LayoutSVGHiddenContainer(SVGElement* element)
    : LayoutSVGContainer(element) {}

SVGLayoutResult LayoutSVGHiddenContainer::UpdateSVGLayout(
    const SVGLayoutInfo& layout_info) {
  NOT_DESTROYED();
  DCHECK(NeedsLayout());

  SVGLayoutInfo child_layout_info = layout_info;
  child_layout_info.force_layout = SelfNeedsFullLayout();
  // When HasRelativeLengths() is false, no descendants have relative lengths
  // (hence no one is interested in viewport size changes).
  child_layout_info.viewport_changed =
      layout_info.viewport_changed && GetElement()->HasRelativeLengths();

  Content().Layout(child_layout_info);
  ClearNeedsLayout();
  return {};
}

bool LayoutSVGHiddenContainer::NodeAtPoint(HitTestResult&,
                                           const HitTestLocation&,
                                           const PhysicalOffset&,
                                           HitTestPhase) {
  NOT_DESTROYED();
  return false;
}

}  // namespace blink

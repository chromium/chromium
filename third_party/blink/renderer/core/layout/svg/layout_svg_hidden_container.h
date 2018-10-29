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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_HIDDEN_CONTAINER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_HIDDEN_CONTAINER_H_

#include "third_party/blink/renderer/core/layout/svg/layout_svg_container.h"

namespace blink {

class SVGElement;

// This class is for containers which are never drawn, but do need to support
// style <defs>, <linearGradient>, <radialGradient> are all good examples
class LayoutSVGHiddenContainer : public LayoutSVGContainer {
 public:
  explicit LayoutSVGHiddenContainer(SVGElement*);

  const char* GetName() const override { return "LayoutSVGHiddenContainer"; }

 protected:
  void UpdateLayout() override;

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectSVGHiddenContainer ||
           LayoutSVGContainer::IsOfType(type);
  }

 private:
  // LayoutSVGHiddenContainer paints nothing.
  void Paint(const PaintInfo&) const final {}
  bool PaintedOutputOfObjectHasNoEffectRegardlessOfSize() const final {
    return true;
  }
  LayoutRect VisualRectInDocument() const final { return LayoutRect(); }
  FloatRect VisualRectInLocalSVGCoordinates() const final {
    return FloatRect();
  }
  void AbsoluteQuads(Vector<FloatQuad>&,
                     MapCoordinatesFlags mode = 0) const final {}

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation& location_in_container,
                   const LayoutPoint& accumulated_offset,
                   HitTestAction) final;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_HIDDEN_CONTAINER_H_

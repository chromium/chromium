/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2009 Google, Inc.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_FOREIGN_OBJECT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_FOREIGN_OBJECT_H_

#include "third_party/blink/renderer/core/layout/svg/layout_svg_block.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

// LayoutSVGForeignObject is the LayoutObject associated with <foreignobject>.
// http://www.w3.org/TR/SVG/extend.html#ForeignObjectElement
//
// Foreign object is a way of inserting arbitrary non-SVG content into SVG.
// A good example of this is HTML in SVG. Because of this, CSS content has to
// be aware of SVG: e.g. when determining containing blocks we stop at the
// enclosing foreign object (see LayoutObject::canContainFixedPositionObjects).
//
// Note that SVG is also allowed in HTML with the HTML5 parsing rules so SVG
// content also has to be aware of CSS objects.
// See http://www.w3.org/TR/html5/syntax.html#elements-0 with the rules for
// 'foreign elements'. TODO(jchaffraix): Find a better place for this paragraph.
//
// The coordinate space for the descendants of the foreignObject does not
// include the effective zoom (it is baked into any lengths as usual). The
// transform that defines the userspace of the element is:
//
//   [CSS transform] * [inverse effective zoom] (* ['x' and 'y' translation])
//
// Because of this, the frame rect and visual rect includes effective zoom. The
// object bounding box (ObjectBoundingBox method) is however not zoomed to be
// compatible with the expectations of the getBBox() DOM interface.
class LayoutSVGForeignObject final : public LayoutSVGBlock {
 public:
  explicit LayoutSVGForeignObject(Element*);
  ~LayoutSVGForeignObject() override;

  const char* GetName() const override {
    NOT_DESTROYED();
    return "LayoutSVGForeignObject";
  }

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  void Paint(const PaintInfo&) const override;

  void UpdateLayout() override;

  gfx::RectF ObjectBoundingBox() const override {
    NOT_DESTROYED();
    return viewport_;
  }
  gfx::RectF StrokeBoundingBox() const override {
    NOT_DESTROYED();
    return VisualRectInLocalSVGCoordinates();
  }
  gfx::RectF VisualRectInLocalSVGCoordinates() const override {
    NOT_DESTROYED();
    return gfx::RectF(FrameRect());
  }
  bool IsObjectBoundingBoxValid() const {
    NOT_DESTROYED();
    return !viewport_.IsEmpty();
  }

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset&,
                   HitTestPhase) override;

  // A method to call when recursively hit testing from an SVG parent.
  // Since LayoutSVGRoot has a PaintLayer always, this will cause a
  // trampoline through PaintLayer::HitTest and back to a call to NodeAtPoint
  // on this object. This is why there are two methods.
  bool NodeAtPointFromSVG(HitTestResult&,
                          const HitTestLocation&,
                          const PhysicalOffset&,
                          HitTestPhase);

  bool IsOfType(LayoutObjectType type) const override {
    NOT_DESTROYED();
    return type == kLayoutObjectSVGForeignObject ||
           LayoutSVGBlock::IsOfType(type);
  }

  PaintLayerType LayerTypeRequired() const override;

  bool CreatesNewFormattingContext() const final {
    NOT_DESTROYED();
    // This is the root of a foreign object. Don't let anything inside it escape
    // to our ancestors.
    return true;
  }

 private:
  void UpdateLogicalWidth() override;
  void ComputeLogicalHeight(LayoutUnit logical_height,
                            LayoutUnit logical_top,
                            LogicalExtentComputedValues&) const override;
  AffineTransform LocalToSVGParentTransform() const override;

  // The resolved viewport in the regular SVG coordinate space (after any
  // 'transform' has been applied but without zoom-adjustment).
  gfx::RectF viewport_;
};

template <>
struct DowncastTraits<LayoutSVGForeignObject> {
  static bool AllowFrom(const LayoutObject& object) {
    return object.IsSVGForeignObject();
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_SVG_LAYOUT_SVG_FOREIGN_OBJECT_H_

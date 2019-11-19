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

namespace blink {

class SVGForeignObjectElement;

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
class LayoutSVGForeignObject final : public LayoutSVGBlock {
 public:
  explicit LayoutSVGForeignObject(SVGForeignObjectElement*);
  ~LayoutSVGForeignObject() override;

  const char* GetName() const override { return "LayoutSVGForeignObject"; }

  bool IsChildAllowed(LayoutObject*, const ComputedStyle&) const override;

  void Paint(const PaintInfo&) const override;

  void UpdateLayout() override;

  FloatRect ObjectBoundingBox() const override {
    return FloatRect(FrameRect());
  }
  FloatRect StrokeBoundingBox() const override { return ObjectBoundingBox(); }
  FloatRect VisualRectInLocalSVGCoordinates() const override {
    return ObjectBoundingBox();
  }
  bool IsObjectBoundingBoxValid() const { return !FrameRect().IsEmpty(); }

  bool NodeAtPoint(HitTestResult&,
                   const HitTestLocation&,
                   const PhysicalOffset&,
                   HitTestAction) override;

  // A method to call when recursively hit testing from an SVG parent.
  // Since LayoutSVGRoot has a PaintLayer always, this will cause a
  // trampoline through PaintLayer::HitTest and back to a call to NodeAtPoint
  // on this object. This is why there are two methods.
  bool NodeAtPointFromSVG(HitTestResult&,
                          const HitTestLocation&,
                          const PhysicalOffset&,
                          HitTestAction);

  bool IsOfType(LayoutObjectType type) const override {
    return type == kLayoutObjectSVGForeignObject ||
           LayoutSVGBlock::IsOfType(type);
  }

  void SetNeedsTransformUpdate() override { needs_transform_update_ = true; }

  PaintLayerType LayerTypeRequired() const override;

  bool CreatesNewFormattingContext() const final {
    // This is the root of a foreign object. Don't let anything inside it escape
    // to our ancestors.
    return true;
  }

 private:
  LayoutUnit ElementX() const;
  LayoutUnit ElementY() const;
  LayoutUnit ElementWidth() const;
  LayoutUnit ElementHeight() const;
  void UpdateLogicalWidth() override;
  void ComputeLogicalHeight(LayoutUnit logical_height,
                            LayoutUnit logical_top,
                            LogicalExtentComputedValues&) const override;
  void StyleDidChange(StyleDifference, const ComputedStyle* old_style) override;

  bool needs_transform_update_;
};

DEFINE_LAYOUT_OBJECT_TYPE_CASTS(LayoutSVGForeignObject, IsSVGForeignObject());

}  // namespace blink

#endif

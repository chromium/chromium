// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/layout/ng/svg/layout_ng_svg_foreign_object.h"

#include "third_party/blink/renderer/core/svg_element_type_helpers.h"

namespace blink {

LayoutNGSVGForeignObject::LayoutNGSVGForeignObject(Element* element)
    : LayoutNGBlockFlowMixin<LayoutSVGBlock>(element) {
  DCHECK(IsA<SVGForeignObjectElement>(element));
}

const char* LayoutNGSVGForeignObject::GetName() const {
  NOT_DESTROYED();
  return "LayoutNGSVGForeignObject";
}

bool LayoutNGSVGForeignObject::IsOfType(LayoutObjectType type) const {
  NOT_DESTROYED();
  return type == kLayoutObjectNGSVGForeignObject ||
         LayoutNGBlockFlowMixin<LayoutSVGBlock>::IsOfType(type);
}

bool LayoutNGSVGForeignObject::IsChildAllowed(
    LayoutObject* child,
    const ComputedStyle& style) const {
  NOT_DESTROYED();
  // Disallow arbitrary SVG content. Only allow proper <svg xmlns="svgNS">
  // subdocuments.
  return !child->IsSVGChild();
}

bool LayoutNGSVGForeignObject::IsObjectBoundingBoxValid() const {
  NOT_DESTROYED();
  return !viewport_.IsEmpty();
}

gfx::RectF LayoutNGSVGForeignObject::ObjectBoundingBox() const {
  NOT_DESTROYED();
  return viewport_;
}

gfx::RectF LayoutNGSVGForeignObject::StrokeBoundingBox() const {
  NOT_DESTROYED();
  return VisualRectInLocalSVGCoordinates();
}

gfx::RectF LayoutNGSVGForeignObject::VisualRectInLocalSVGCoordinates() const {
  NOT_DESTROYED();
  return gfx::RectF(FrameRect());
}

AffineTransform LayoutNGSVGForeignObject::LocalToSVGParentTransform() const {
  NOT_DESTROYED();
  // Include a zoom inverse in the local-to-parent transform since descendants
  // of the <foreignObject> will have regular zoom applied, and thus need to
  // have that removed when moving into the <fO> ancestors chain (the SVG root
  // will then reapply the zoom again if that boundary is crossed).
  AffineTransform transform = local_transform_;
  transform.Scale(1 / StyleRef().EffectiveZoom());
  return transform;
}

}  // namespace blink

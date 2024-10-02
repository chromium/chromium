/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2014 Google, Inc.
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

#include "third_party/blink/renderer/core/svg/svg_graphics_element.h"

#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/svg/svg_foreign_object_element.h"
#include "third_party/blink/renderer/core/svg/svg_image_element.h"
#include "third_party/blink/renderer/core/svg/svg_matrix_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_rect_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_svg_element.h"
#include "third_party/blink/renderer/core/svg/svg_symbol_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

SVGGraphicsElement::SVGGraphicsElement(const QualifiedName& tag_name,
                                       Document& document,
                                       ConstructionType construction_type)
    : SVGTransformableElement(tag_name, document, construction_type),
      SVGTests(this) {}

SVGGraphicsElement::~SVGGraphicsElement() = default;

void SVGGraphicsElement::Trace(Visitor* visitor) const {
  SVGTransformableElement::Trace(visitor);
  SVGTests::Trace(visitor);
}

static bool IsViewportElement(const Element& element) {
  return (IsA<SVGSVGElement>(element) || IsA<SVGSymbolElement>(element) ||
          IsA<SVGForeignObjectElement>(element) ||
          IsA<SVGImageElement>(element));
}

AffineTransform SVGGraphicsElement::ComputeCTM(
    SVGElement::CTMScope mode,
    const SVGGraphicsElement* ancestor) const {
  AffineTransform ctm;
  bool done = false;

  for (const Element* current_element = this; current_element && !done;
       current_element = current_element->ParentOrShadowHostElement()) {
    auto* svg_element = DynamicTo<SVGElement>(current_element);
    if (!svg_element)
      break;

    ctm = svg_element->LocalCoordinateSpaceTransform(mode).PreConcat(ctm);

    switch (mode) {
      case kNearestViewportScope:
        // Stop at the nearest viewport ancestor.
        done = current_element != this && IsViewportElement(*current_element);
        break;
      case kAncestorScope:
        // Stop at the designated ancestor.
        done = current_element == ancestor;
        break;
      default:
        DCHECK_EQ(mode, kScreenScope);
        break;
    }
  }
  return ctm;
}

SVGMatrixTearOff* SVGGraphicsElement::getCTM() {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  return MakeGarbageCollected<SVGMatrixTearOff>(
      ComputeCTM(kNearestViewportScope));
}

SVGMatrixTearOff* SVGGraphicsElement::getScreenCTM() {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  return MakeGarbageCollected<SVGMatrixTearOff>(ComputeCTM(kScreenScope));
}

void SVGGraphicsElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  // Reattach so the isValid() check will be run again during layoutObject
  // creation.
  if (SVGTests::IsKnownAttribute(attr_name)) {
    SetForceReattachLayoutTree();
    return;
  }
  SVGTransformableElement::SvgAttributeChanged(params);
}

SVGElement* SVGGraphicsElement::nearestViewportElement() const {
  for (Element* current = ParentOrShadowHostElement(); current;
       current = current->ParentOrShadowHostElement()) {
    if (IsViewportElement(*current))
      return To<SVGElement>(current);
  }

  return nullptr;
}

SVGElement* SVGGraphicsElement::farthestViewportElement() const {
  SVGElement* farthest = nullptr;
  for (Element* current = ParentOrShadowHostElement(); current;
       current = current->ParentOrShadowHostElement()) {
    if (IsViewportElement(*current))
      farthest = To<SVGElement>(current);
  }
  return farthest;
}

gfx::RectF SVGGraphicsElement::GetBBox() {
  DCHECK(GetLayoutObject());
  return GetLayoutObject()->ObjectBoundingBox();
}

SVGRectTearOff* SVGGraphicsElement::getBBoxFromJavascript() {
  GetDocument().UpdateStyleAndLayoutForNode(this,
                                            DocumentUpdateReason::kJavaScript);

  // FIXME: Eventually we should support getBBox for detached elements.
  gfx::RectF bounding_box;
  if (const auto* layout_object = GetLayoutObject()) {
    bounding_box = GetBBox();

    if (layout_object->IsSVGInline()) {
      UseCounter::Count(GetDocument(), WebFeature::kGetBBoxForText);
    }
  }
  return SVGRectTearOff::CreateDetached(bounding_box);
}

SVGAnimatedPropertyBase* SVGGraphicsElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  SVGAnimatedPropertyBase* ret =
      SVGTests::PropertyFromAttribute(attribute_name);
  if (ret) {
    return ret;
  }
  return SVGTransformableElement::PropertyFromAttribute(attribute_name);
}

void SVGGraphicsElement::SynchronizeAllSVGAttributes() const {
  SVGTests::SynchronizeAllSVGAttributes();
  SVGTransformableElement::SynchronizeAllSVGAttributes();
}

}  // namespace blink

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

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/svg/svg_element_rare_data.h"
#include "third_party/blink/renderer/core/svg/svg_matrix_tear_off.h"
#include "third_party/blink/renderer/core/svg/svg_rect_tear_off.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

SVGGraphicsElement::SVGGraphicsElement(const QualifiedName& tag_name,
                                       Document& document,
                                       ConstructionType construction_type)
    : SVGElement(tag_name, document, construction_type),
      SVGTests(this),
      transform_(MakeGarbageCollected<SVGAnimatedTransformList>(
          this,
          svg_names::kTransformAttr,
          CSSPropertyID::kTransform)) {
  AddToPropertyMap(transform_);
}

SVGGraphicsElement::~SVGGraphicsElement() = default;

void SVGGraphicsElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(transform_);
  SVGElement::Trace(visitor);
  SVGTests::Trace(visitor);
}

static bool IsViewportElement(const Element& element) {
  return (IsSVGSVGElement(element) || IsSVGSymbolElement(element) ||
          IsSVGForeignObjectElement(element) || IsSVGImageElement(element));
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

    ctm = svg_element->LocalCoordinateSpaceTransform(mode).Multiply(ctm);

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
  GetDocument().UpdateStyleAndLayoutForNode(this);

  return MakeGarbageCollected<SVGMatrixTearOff>(
      ComputeCTM(kNearestViewportScope));
}

SVGMatrixTearOff* SVGGraphicsElement::getScreenCTM() {
  GetDocument().UpdateStyleAndLayoutForNode(this);

  return MakeGarbageCollected<SVGMatrixTearOff>(ComputeCTM(kScreenScope));
}

void SVGGraphicsElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == svg_names::kTransformAttr) {
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kTransform,
        *transform_->CurrentValue()->CssValue());
    return;
  }
  SVGElement::CollectStyleForPresentationAttribute(name, value, style);
}

AffineTransform* SVGGraphicsElement::AnimateMotionTransform() {
  return EnsureSVGRareData()->AnimateMotionTransform();
}

void SVGGraphicsElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  // Reattach so the isValid() check will be run again during layoutObject
  // creation.
  if (SVGTests::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    SetForceReattachLayoutTree();
    return;
  }

  if (attr_name == svg_names::kTransformAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    InvalidateSVGPresentationAttributeStyle();
    // TODO(fs): The InvalidationGuard will make sure all instances are
    // invalidated, but the style recalc will propagate to instances too. So
    // there is some redundant operations being performed here. Could we get
    // away with removing the InvalidationGuard?
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::FromAttribute(attr_name));
    if (LayoutObject* object = GetLayoutObject())
      MarkForLayoutAndParentResourceInvalidation(*object);
    return;
  }

  SVGElement::SvgAttributeChanged(attr_name);
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

FloatRect SVGGraphicsElement::GetBBox() {
  DCHECK(GetLayoutObject());
  return GetLayoutObject()->ObjectBoundingBox();
}

SVGRectTearOff* SVGGraphicsElement::getBBoxFromJavascript() {
  GetDocument().UpdateStyleAndLayout();

  // FIXME: Eventually we should support getBBox for detached elements.
  FloatRect boundingBox;
  if (GetLayoutObject())
    boundingBox = GetBBox();
  return SVGRectTearOff::CreateDetached(boundingBox);
}

}  // namespace blink

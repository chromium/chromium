/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_path_element.h"

#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/svg/svg_mpath_element.h"
#include "third_party/blink/renderer/core/svg/svg_path_query.h"
#include "third_party/blink/renderer/core/svg/svg_path_utilities.h"
#include "third_party/blink/renderer/core/svg/svg_point_tear_off.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SVGPathElement::SVGPathElement(Document& document)
    : SVGGeometryElement(svg_names::kPathTag, document),
      path_(MakeGarbageCollected<SVGAnimatedPath>(this,
                                                  svg_names::kDAttr,
                                                  CSSPropertyID::kD)) {
  AddToPropertyMap(path_);
}

void SVGPathElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(path_);
  SVGGeometryElement::Trace(visitor);
}

Path SVGPathElement::AttributePath() const {
  return path_->CurrentValue()->GetStylePath()->GetPath();
}

const StylePath* SVGPathElement::GetStylePath() const {
  if (LayoutObject* layout_object = GetLayoutObject()) {
    const StylePath* style_path = layout_object->StyleRef().SvgStyle().D();
    if (style_path)
      return style_path;
    return StylePath::EmptyPath();
  }
  return path_->CurrentValue()->GetStylePath();
}

float SVGPathElement::ComputePathLength() const {
  return GetStylePath()->length();
}

Path SVGPathElement::AsPath() const {
  return GetStylePath()->GetPath();
}

float SVGPathElement::getTotalLength(ExceptionState& exception_state) {
  GetDocument().UpdateStyleAndLayoutForNode(this);
  return SVGPathQuery(PathByteStream()).GetTotalLength();
}

SVGPointTearOff* SVGPathElement::getPointAtLength(float length) {
  GetDocument().UpdateStyleAndLayoutForNode(this);
  SVGPathQuery path_query(PathByteStream());
  if (length < 0) {
    length = 0;
  } else {
    float computed_length = path_query.GetTotalLength();
    if (length > computed_length)
      length = computed_length;
  }
  FloatPoint point = path_query.GetPointAtLength(length);
  return SVGPointTearOff::CreateDetached(point);
}

void SVGPathElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == svg_names::kDAttr) {
    InvalidateMPathDependencies();
    GeometryPresentationAttributeChanged(attr_name);
    return;
  }

  SVGGeometryElement::SvgAttributeChanged(attr_name);
}

void SVGPathElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  SVGAnimatedPropertyBase* property = PropertyFromAttribute(name);
  if (property == path_) {
    SVGAnimatedPath* path = GetPath();
    // If this is a <use> instance, return the referenced path to maximize
    // geometry sharing.
    if (const SVGElement* element = CorrespondingElement())
      path = ToSVGPathElement(element)->GetPath();
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            path->CssValue());
    return;
  }
  SVGGeometryElement::CollectStyleForPresentationAttribute(name, value, style);
}

void SVGPathElement::InvalidateMPathDependencies() {
  // <mpath> can only reference <path> but this dependency is not handled in
  // markForLayoutAndParentResourceInvalidation so we update any mpath
  // dependencies manually.
  if (SVGElementSet* dependencies = SetOfIncomingReferences()) {
    for (SVGElement* element : *dependencies) {
      if (auto* mpath = ToSVGMPathElementOrNull(*element))
        mpath->TargetPathChanged();
    }
  }
}

Node::InsertionNotificationRequest SVGPathElement::InsertedInto(
    ContainerNode& root_parent) {
  SVGGeometryElement::InsertedInto(root_parent);
  InvalidateMPathDependencies();
  return kInsertionDone;
}

void SVGPathElement::RemovedFrom(ContainerNode& root_parent) {
  SVGGeometryElement::RemovedFrom(root_parent);
  InvalidateMPathDependencies();
}

FloatRect SVGPathElement::GetBBox() {
  // We want the exact bounds.
  return SVGPathElement::AsPath().BoundingRect();
}

}  // namespace blink

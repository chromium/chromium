/*
 * Copyright (C) 2004, 2005 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_animate_transform_element.h"

#include "third_party/blink/renderer/core/svg/properties/svg_animated_property.h"
#include "third_party/blink/renderer/core/svg/svg_transform_list.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

inline SVGAnimateTransformElement::SVGAnimateTransformElement(
    Document& document)
    : SVGAnimateElement(svg_names::kAnimateTransformTag, document),
      transform_type_(kSvgTransformUnknown) {}

DEFINE_NODE_FACTORY(SVGAnimateTransformElement)

bool SVGAnimateTransformElement::HasValidTarget() {
  if (!SVGAnimateElement::HasValidTarget())
    return false;
  if (GetAttributeType() == kAttributeTypeCSS)
    return false;
  return type_ == kAnimatedTransformList;
}

void SVGAnimateTransformElement::ResolveTargetProperty() {
  DCHECK(targetElement());
  target_property_ = targetElement()->PropertyFromAttribute(AttributeName());
  type_ = target_property_ ? target_property_->GetType() : kAnimatedUnknown;
  // <animateTransform> only animates AnimatedTransformList.
  // http://www.w3.org/TR/SVG/animate.html#AnimationAttributesAndProperties
  if (type_ != kAnimatedTransformList)
    type_ = kAnimatedUnknown;
  // Because of the syntactic mismatch between the CSS and SVGProperty
  // representations, disallow CSS animations of transforms. Support for that
  // is better added to the <animate> element since the <animateTransform>
  // element is deprecated and quirky. (We also reject this case via
  // hasValidAttributeType above.)
  css_property_id_ = CSSPropertyInvalid;
}

SVGPropertyBase* SVGAnimateTransformElement::CreatePropertyForAnimation(
    const String& value) const {
  DCHECK(IsAnimatingSVGDom());
  return SVGTransformList::Create(transform_type_, value);
}

void SVGAnimateTransformElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == svg_names::kTypeAttr) {
    transform_type_ = ParseTransformType(params.new_value);
    if (transform_type_ == kSvgTransformMatrix)
      transform_type_ = kSvgTransformUnknown;
    return;
  }

  SVGAnimateElement::ParseAttribute(params);
}

}  // namespace blink

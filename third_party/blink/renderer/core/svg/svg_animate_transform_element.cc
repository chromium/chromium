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
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGAnimateTransformElement::SVGAnimateTransformElement(Document& document)
    : SVGAnimateElement(svg_names::kAnimateTransformTag, document),
      transform_type_(SVGTransformType::kTranslate) {}

bool SVGAnimateTransformElement::HasValidAnimation() const {
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
  css_property_id_ = CSSPropertyID::kInvalid;
}

SVGPropertyBase* SVGAnimateTransformElement::CreateUnderlyingValueForAnimation()
    const {
  DCHECK(IsAnimatingSVGDom());
  return To<SVGTransformList>(target_property_->BaseValueBase()).Clone();
}

SVGPropertyBase* SVGAnimateTransformElement::ParseValue(
    const String& value) const {
  DCHECK(IsAnimatingSVGDom());
  return MakeGarbageCollected<SVGTransformList>(transform_type_, value);
}

static SVGTransformType ParseTypeAttribute(const String& value) {
  if (value.IsNull())
    return SVGTransformType::kTranslate;
  SVGTransformType transform_type = ParseTransformType(value);
  // Since ParseTransformType() is also used when parsing transform lists, it
  // accepts the value "matrix". That value is however not recognized by the
  // 'type' attribute, so treat it as invalid.
  if (transform_type == SVGTransformType::kMatrix)
    transform_type = SVGTransformType::kUnknown;
  return transform_type;
}

void SVGAnimateTransformElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == svg_names::kTypeAttr) {
    SVGTransformType old_transform_type = transform_type_;
    transform_type_ = ParseTypeAttribute(params.new_value);
    if (transform_type_ != old_transform_type)
      AnimationAttributeChanged();
    return;
  }

  SVGAnimateElement::ParseAttribute(params);
}

}  // namespace blink

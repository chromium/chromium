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
#include "third_party/blink/renderer/core/svg/svg_animated_transform_list.h"
#include "third_party/blink/renderer/core/svg/svg_element_rare_data.h"
#include "third_party/blink/renderer/core/svg_names.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

SVGTransformableElement::SVGTransformableElement(
    const QualifiedName& tag_name,
    Document& document,
    ConstructionType construction_type)
    : SVGElement(tag_name, document, construction_type),
      transform_(MakeGarbageCollected<SVGAnimatedTransformList>(
          this,
          svg_names::kTransformAttr,
          CSSPropertyID::kTransform)) {}

SVGTransformableElement::~SVGTransformableElement() = default;

void SVGTransformableElement::Trace(Visitor* visitor) const {
  visitor->Trace(transform_);
  SVGElement::Trace(visitor);
}

void SVGTransformableElement::CollectExtraStyleForPresentationAttribute(
    MutableCSSPropertyValueSet* style) {
  AddAnimatedPropertyToPresentationAttributeStyle(*transform_, style);
  SVGElement::CollectExtraStyleForPresentationAttribute(style);
}

AffineTransform SVGTransformableElement::LocalCoordinateSpaceTransform(
    CTMScope) const {
  return CalculateTransform(kIncludeMotionTransform);
}

AffineTransform* SVGTransformableElement::AnimateMotionTransform() {
  return EnsureSVGRareData()->AnimateMotionTransform();
}

void SVGTransformableElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kTransformAttr) {
    UpdatePresentationAttributeStyle(*transform_);
    return;
  }

  SVGElement::SvgAttributeChanged(params);
}

SVGAnimatedPropertyBase* SVGTransformableElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kTransformAttr) {
    return transform_.Get();
  }
  return SVGElement::PropertyFromAttribute(attribute_name);
}

void SVGTransformableElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{transform_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGElement::SynchronizeAllSVGAttributes();
}

}  // namespace blink

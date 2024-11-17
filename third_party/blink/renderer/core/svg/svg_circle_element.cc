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

#include "third_party/blink/renderer/core/svg/svg_circle_element.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_ellipse.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGCircleElement::SVGCircleElement(Document& document)
    : SVGGeometryElement(svg_names::kCircleTag, document),
      cx_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kCxAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kCx)),
      cy_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kCyAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kCy)),
      r_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kRAttr,
          SVGLengthMode::kOther,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kR)) {}

void SVGCircleElement::Trace(Visitor* visitor) const {
  visitor->Trace(cx_);
  visitor->Trace(cy_);
  visitor->Trace(r_);
  SVGGeometryElement::Trace(visitor);
}

Path SVGCircleElement::AsPath() const {
  Path path;

  const SVGViewportResolver viewport_resolver(*this);
  const ComputedStyle& style = ComputedStyleRef();

  float r = ValueForLength(style.R(), viewport_resolver, style,
                           SVGLengthMode::kOther);
  if (r > 0) {
    gfx::PointF center =
        PointForLengthPair(style.Cx(), style.Cy(), viewport_resolver, style);
    path.AddEllipse(center, r, r);
  }
  return path;
}

void SVGCircleElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kRAttr || attr_name == svg_names::kCxAttr ||
      attr_name == svg_names::kCyAttr) {
    UpdateRelativeLengthsInformation();
    GeometryPresentationAttributeChanged(params.property);
    return;
  }

  SVGGeometryElement::SvgAttributeChanged(params);
}

bool SVGCircleElement::SelfHasRelativeLengths() const {
  return cx_->CurrentValue()->IsRelative() ||
         cy_->CurrentValue()->IsRelative() || r_->CurrentValue()->IsRelative();
}

LayoutObject* SVGCircleElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutSVGEllipse>(this);
}

SVGAnimatedPropertyBase* SVGCircleElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kCxAttr) {
    return cx_.Get();
  } else if (attribute_name == svg_names::kCyAttr) {
    return cy_.Get();
  } else if (attribute_name == svg_names::kRAttr) {
    return r_.Get();
  } else {
    return SVGGeometryElement::PropertyFromAttribute(attribute_name);
  }
}

void SVGCircleElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{cx_.Get(), cy_.Get(), r_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGGeometryElement::SynchronizeAllSVGAttributes();
}

void SVGCircleElement::CollectExtraStyleForPresentationAttribute(
    MutableCSSPropertyValueSet* style) {
  auto pres_attrs = std::to_array<const SVGAnimatedPropertyBase*>(
      {cx_.Get(), cy_.Get(), r_.Get()});
  AddAnimatedPropertiesToPresentationAttributeStyle(pres_attrs, style);
  SVGGeometryElement::CollectExtraStyleForPresentationAttribute(style);
}

}  // namespace blink

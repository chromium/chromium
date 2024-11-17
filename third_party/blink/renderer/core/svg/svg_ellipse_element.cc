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

#include "third_party/blink/renderer/core/svg/svg_ellipse_element.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_ellipse.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGEllipseElement::SVGEllipseElement(Document& document)
    : SVGGeometryElement(svg_names::kEllipseTag, document),
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
      rx_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kRxAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kRx)),
      ry_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kRyAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kRy)) {}

void SVGEllipseElement::Trace(Visitor* visitor) const {
  visitor->Trace(cx_);
  visitor->Trace(cy_);
  visitor->Trace(rx_);
  visitor->Trace(ry_);
  SVGGeometryElement::Trace(visitor);
}

Path SVGEllipseElement::AsPath() const {
  Path path;

  const SVGViewportResolver viewport_resolver(*this);
  const ComputedStyle& style = ComputedStyleRef();

  gfx::Vector2dF radii =
      VectorForLengthPair(style.Rx(), style.Ry(), viewport_resolver, style);
  if (style.Rx().IsAuto())
    radii.set_x(radii.y());
  else if (style.Ry().IsAuto())
    radii.set_y(radii.x());
  if (radii.x() < 0 || radii.y() < 0 || (!radii.x() && !radii.y()))
    return path;

  gfx::PointF center =
      PointForLengthPair(style.Cx(), style.Cy(), viewport_resolver, style);
  path.AddEllipse(center, radii.x(), radii.y());
  return path;
}

void SVGEllipseElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kCxAttr || attr_name == svg_names::kCyAttr ||
      attr_name == svg_names::kRxAttr || attr_name == svg_names::kRyAttr) {
    UpdateRelativeLengthsInformation();
    GeometryPresentationAttributeChanged(params.property);
    return;
  }

  SVGGeometryElement::SvgAttributeChanged(params);
}

bool SVGEllipseElement::SelfHasRelativeLengths() const {
  return cx_->CurrentValue()->IsRelative() ||
         cy_->CurrentValue()->IsRelative() ||
         rx_->CurrentValue()->IsRelative() || ry_->CurrentValue()->IsRelative();
}

LayoutObject* SVGEllipseElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutSVGEllipse>(this);
}

SVGAnimatedPropertyBase* SVGEllipseElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kCxAttr) {
    return cx_.Get();
  } else if (attribute_name == svg_names::kCyAttr) {
    return cy_.Get();
  } else if (attribute_name == svg_names::kRxAttr) {
    return rx_.Get();
  } else if (attribute_name == svg_names::kRyAttr) {
    return ry_.Get();
  } else {
    return SVGGeometryElement::PropertyFromAttribute(attribute_name);
  }
}

void SVGEllipseElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{cx_.Get(), cy_.Get(), rx_.Get(), ry_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGGeometryElement::SynchronizeAllSVGAttributes();
}

void SVGEllipseElement::CollectExtraStyleForPresentationAttribute(
    MutableCSSPropertyValueSet* style) {
  auto pres_attrs = std::to_array<const SVGAnimatedPropertyBase*>(
      {cx_.Get(), cy_.Get(), rx_.Get(), ry_.Get()});
  AddAnimatedPropertiesToPresentationAttributeStyle(pres_attrs, style);
  SVGGeometryElement::CollectExtraStyleForPresentationAttribute(style);
}

}  // namespace blink

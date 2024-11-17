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

#include "third_party/blink/renderer/core/svg/svg_rect_element.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_rect.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_functions.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

SVGRectElement::SVGRectElement(Document& document)
    : SVGGeometryElement(svg_names::kRectTag, document),
      x_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kXAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kX)),
      y_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kYAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kY)),
      width_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kWidthAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kWidth)),
      height_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kHeightAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kUnitlessZero,
          CSSPropertyID::kHeight)),
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

void SVGRectElement::Trace(Visitor* visitor) const {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  visitor->Trace(rx_);
  visitor->Trace(ry_);
  SVGGeometryElement::Trace(visitor);
}

Path SVGRectElement::AsPath() const {
  Path path;

  const SVGViewportResolver viewport_resolver(*this);
  const ComputedStyle& style = ComputedStyleRef();

  gfx::Vector2dF size = VectorForLengthPair(style.Width(), style.Height(),
                                            viewport_resolver, style);
  if (size.x() < 0 || size.y() < 0 || size.IsZero())
    return path;

  gfx::PointF origin =
      PointForLengthPair(style.X(), style.Y(), viewport_resolver, style);
  gfx::RectF rect(origin, gfx::SizeF(size.x(), size.y()));

  gfx::Vector2dF radii =
      VectorForLengthPair(style.Rx(), style.Ry(), viewport_resolver, style);
  // Apply the SVG corner radius constraints, per the rect section of the SVG
  // shapes spec: if one of radii.x() and radii.y() is auto or negative, then
  // the other corner radius value is used. If both are auto or negative, then
  // they are both set to 0.
  if (style.Rx().IsAuto() || radii.x() < 0)
    radii.set_x(std::max(0.f, radii.y()));
  if (style.Ry().IsAuto() || radii.y() < 0)
    radii.set_y(radii.x());

  if (radii.x() > 0 || radii.y() > 0) {
    // Apply SVG corner radius constraints, continued: if radii.x() is greater
    // than half of the width of the rectangle then its set to half of the
    // width; radii.y() is handled similarly.
    radii.SetToMin(gfx::ScaleVector2d(size, 0.5));
    path.AddRoundedRect(FloatRoundedRect(rect, radii.x(), radii.y()));
  } else {
    path.AddRect(rect);
  }
  return path;
}

void SVGRectElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr ||
      attr_name == svg_names::kWidthAttr ||
      attr_name == svg_names::kHeightAttr || attr_name == svg_names::kRxAttr ||
      attr_name == svg_names::kRyAttr) {
    UpdateRelativeLengthsInformation();
    GeometryPresentationAttributeChanged(params.property);
    return;
  }

  SVGGeometryElement::SvgAttributeChanged(params);
}

bool SVGRectElement::SelfHasRelativeLengths() const {
  return x_->CurrentValue()->IsRelative() || y_->CurrentValue()->IsRelative() ||
         width_->CurrentValue()->IsRelative() ||
         height_->CurrentValue()->IsRelative() ||
         rx_->CurrentValue()->IsRelative() || ry_->CurrentValue()->IsRelative();
}

LayoutObject* SVGRectElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutSVGRect>(this);
}

SVGAnimatedPropertyBase* SVGRectElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kXAttr) {
    return x_.Get();
  } else if (attribute_name == svg_names::kYAttr) {
    return y_.Get();
  } else if (attribute_name == svg_names::kWidthAttr) {
    return width_.Get();
  } else if (attribute_name == svg_names::kHeightAttr) {
    return height_.Get();
  } else if (attribute_name == svg_names::kRxAttr) {
    return rx_.Get();
  } else if (attribute_name == svg_names::kRyAttr) {
    return ry_.Get();
  } else {
    return SVGGeometryElement::PropertyFromAttribute(attribute_name);
  }
}

void SVGRectElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{x_.Get(),      y_.Get(),  width_.Get(),
                                   height_.Get(), rx_.Get(), ry_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGGeometryElement::SynchronizeAllSVGAttributes();
}

void SVGRectElement::CollectExtraStyleForPresentationAttribute(
    MutableCSSPropertyValueSet* style) {
  auto pres_attrs = std::to_array<const SVGAnimatedPropertyBase*>(
      {x_.Get(), y_.Get(), width_.Get(), height_.Get(), rx_.Get(), ry_.Get()});
  AddAnimatedPropertiesToPresentationAttributeStyle(pres_attrs, style);
  SVGGeometryElement::CollectExtraStyleForPresentationAttribute(style);
}

}  // namespace blink

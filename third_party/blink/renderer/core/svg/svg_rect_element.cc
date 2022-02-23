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
          CSSPropertyID::kRy)) {
  AddToPropertyMap(x_);
  AddToPropertyMap(y_);
  AddToPropertyMap(width_);
  AddToPropertyMap(height_);
  AddToPropertyMap(rx_);
  AddToPropertyMap(ry_);
}

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

  SVGLengthContext length_context(this);
  const ComputedStyle& style = ComputedStyleRef();

  gfx::Vector2dF size =
      length_context.ResolveLengthPair(style.Width(), style.Height(), style);
  if (size.x() < 0 || size.y() < 0 || size.IsZero())
    return path;

  gfx::Vector2dF origin =
      length_context.ResolveLengthPair(style.X(), style.Y(), style);
  gfx::RectF rect(origin.x(), origin.y(), size.x(), size.y());

  gfx::Vector2dF radii =
      length_context.ResolveLengthPair(style.Rx(), style.Ry(), style);
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

void SVGRectElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  SVGAnimatedPropertyBase* property = PropertyFromAttribute(name);
  if (property == x_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            x_->CssValue());
  } else if (property == y_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            y_->CssValue());
  } else if (property == width_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            width_->CssValue());
  } else if (property == height_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            height_->CssValue());
  } else if (property == rx_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            rx_->CssValue());
  } else if (property == ry_) {
    AddPropertyToPresentationAttributeStyle(style, property->CssPropertyId(),
                                            ry_->CssValue());
  } else {
    SVGGeometryElement::CollectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

void SVGRectElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr ||
      attr_name == svg_names::kWidthAttr ||
      attr_name == svg_names::kHeightAttr || attr_name == svg_names::kRxAttr ||
      attr_name == svg_names::kRyAttr) {
    UpdateRelativeLengthsInformation();
    GeometryPresentationAttributeChanged(attr_name);
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

LayoutObject* SVGRectElement::CreateLayoutObject(const ComputedStyle&,
                                                 LegacyLayout) {
  return MakeGarbageCollected<LayoutSVGRect>(this);
}

}  // namespace blink

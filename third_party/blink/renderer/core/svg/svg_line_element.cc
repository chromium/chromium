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

#include "third_party/blink/renderer/core/svg/svg_line_element.h"

#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SVGLineElement::SVGLineElement(Document& document)
    : SVGGeometryElement(svg_names::kLineTag, document),
      x1_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kX1Attr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kUnitlessZero)),
      y1_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kY1Attr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kUnitlessZero)),
      x2_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kX2Attr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kUnitlessZero)),
      y2_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kY2Attr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kUnitlessZero)) {
  AddToPropertyMap(x1_);
  AddToPropertyMap(y1_);
  AddToPropertyMap(x2_);
  AddToPropertyMap(y2_);
}

void SVGLineElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(x1_);
  visitor->Trace(y1_);
  visitor->Trace(x2_);
  visitor->Trace(y2_);
  SVGGeometryElement::Trace(visitor);
}

Path SVGLineElement::AsPath() const {
  Path path;

  SVGLengthContext length_context(this);
  path.MoveTo(FloatPoint(x1()->CurrentValue()->Value(length_context),
                         y1()->CurrentValue()->Value(length_context)));
  path.AddLineTo(FloatPoint(x2()->CurrentValue()->Value(length_context),
                            y2()->CurrentValue()->Value(length_context)));

  return path;
}

void SVGLineElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == svg_names::kX1Attr || attr_name == svg_names::kY1Attr ||
      attr_name == svg_names::kX2Attr || attr_name == svg_names::kY2Attr) {
    UpdateRelativeLengthsInformation();
    GeometryAttributeChanged();
    return;
  }

  SVGGeometryElement::SvgAttributeChanged(attr_name);
}

bool SVGLineElement::SelfHasRelativeLengths() const {
  return x1_->CurrentValue()->IsRelative() ||
         y1_->CurrentValue()->IsRelative() ||
         x2_->CurrentValue()->IsRelative() || y2_->CurrentValue()->IsRelative();
}

}  // namespace blink

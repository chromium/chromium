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

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/core/svg/svg_length_context.h"
#include "third_party/blink/renderer/platform/graphics/path.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

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
          SVGLength::Initial::kUnitlessZero)) {}

void SVGLineElement::Trace(Visitor* visitor) const {
  visitor->Trace(x1_);
  visitor->Trace(y1_);
  visitor->Trace(x2_);
  visitor->Trace(y2_);
  SVGGeometryElement::Trace(visitor);
}

Path SVGLineElement::AsPath() const {
  Path path;

  SVGLengthContext length_context(this);
  DCHECK(GetComputedStyle());

  path.MoveTo(gfx::PointF(x1()->CurrentValue()->Value(length_context),
                          y1()->CurrentValue()->Value(length_context)));
  path.AddLineTo(gfx::PointF(x2()->CurrentValue()->Value(length_context),
                             y2()->CurrentValue()->Value(length_context)));

  return path;
}

void SVGLineElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kX1Attr || attr_name == svg_names::kY1Attr ||
      attr_name == svg_names::kX2Attr || attr_name == svg_names::kY2Attr) {
    UpdateRelativeLengthsInformation();
    GeometryAttributeChanged();
    return;
  }

  SVGGeometryElement::SvgAttributeChanged(params);
}

bool SVGLineElement::SelfHasRelativeLengths() const {
  return x1_->CurrentValue()->IsRelative() ||
         y1_->CurrentValue()->IsRelative() ||
         x2_->CurrentValue()->IsRelative() || y2_->CurrentValue()->IsRelative();
}

SVGAnimatedPropertyBase* SVGLineElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kX1Attr) {
    return x1_.Get();
  } else if (attribute_name == svg_names::kY1Attr) {
    return y1_.Get();
  } else if (attribute_name == svg_names::kX2Attr) {
    return x2_.Get();
  } else if (attribute_name == svg_names::kY2Attr) {
    return y2_.Get();
  } else {
    return SVGGeometryElement::PropertyFromAttribute(attribute_name);
  }
}

void SVGLineElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{x1_.Get(), y1_.Get(), x2_.Get(), y2_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGGeometryElement::SynchronizeAllSVGAttributes();
}

}  // namespace blink

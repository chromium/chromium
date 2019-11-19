/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#include "third_party/blink/renderer/core/svg/svg_linear_gradient_element.h"

#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_linear_gradient.h"
#include "third_party/blink/renderer/core/svg/linear_gradient_attributes.h"
#include "third_party/blink/renderer/core/svg/svg_length.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SVGLinearGradientElement::SVGLinearGradientElement(Document& document)
    : SVGGradientElement(svg_names::kLinearGradientTag, document),
      // Spec: If the x1|y1|y2 attribute is not specified, the effect is as if a
      // value of "0%" were specified.
      // Spec: If the x2 attribute is not specified, the effect is as if a value
      // of "100%" were specified.
      x1_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kX1Attr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kPercent0)),
      y1_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kY1Attr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kPercent0)),
      x2_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kX2Attr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kPercent100)),
      y2_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kY2Attr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kPercent0)) {
  AddToPropertyMap(x1_);
  AddToPropertyMap(y1_);
  AddToPropertyMap(x2_);
  AddToPropertyMap(y2_);
}

void SVGLinearGradientElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(x1_);
  visitor->Trace(y1_);
  visitor->Trace(x2_);
  visitor->Trace(y2_);
  SVGGradientElement::Trace(visitor);
}

void SVGLinearGradientElement::SvgAttributeChanged(
    const QualifiedName& attr_name) {
  if (attr_name == svg_names::kX1Attr || attr_name == svg_names::kX2Attr ||
      attr_name == svg_names::kY1Attr || attr_name == svg_names::kY2Attr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    UpdateRelativeLengthsInformation();
    InvalidateGradient(layout_invalidation_reason::kAttributeChanged);
    return;
  }

  SVGGradientElement::SvgAttributeChanged(attr_name);
}

LayoutObject* SVGLinearGradientElement::CreateLayoutObject(const ComputedStyle&,
                                                           LegacyLayout) {
  return new LayoutSVGResourceLinearGradient(this);
}

static void SetGradientAttributes(const SVGGradientElement& element,
                                  LinearGradientAttributes& attributes,
                                  bool is_linear) {
  element.SynchronizeAnimatedSVGAttribute(AnyQName());
  element.CollectCommonAttributes(attributes);

  if (!is_linear)
    return;
  const SVGLinearGradientElement& linear = ToSVGLinearGradientElement(element);

  if (!attributes.HasX1() && linear.x1()->IsSpecified())
    attributes.SetX1(linear.x1()->CurrentValue());

  if (!attributes.HasY1() && linear.y1()->IsSpecified())
    attributes.SetY1(linear.y1()->CurrentValue());

  if (!attributes.HasX2() && linear.x2()->IsSpecified())
    attributes.SetX2(linear.x2()->CurrentValue());

  if (!attributes.HasY2() && linear.y2()->IsSpecified())
    attributes.SetY2(linear.y2()->CurrentValue());
}

bool SVGLinearGradientElement::CollectGradientAttributes(
    LinearGradientAttributes& attributes) {
  DCHECK(GetLayoutObject());

  VisitedSet visited;
  const SVGGradientElement* current = this;

  while (true) {
    SetGradientAttributes(*current, attributes,
                          IsSVGLinearGradientElement(*current));
    visited.insert(current);

    current = current->ReferencedElement();
    if (!current || visited.Contains(current))
      break;
    if (!current->GetLayoutObject())
      return false;
  }
  return true;
}

bool SVGLinearGradientElement::SelfHasRelativeLengths() const {
  return x1_->CurrentValue()->IsRelative() ||
         y1_->CurrentValue()->IsRelative() ||
         x2_->CurrentValue()->IsRelative() || y2_->CurrentValue()->IsRelative();
}

}  // namespace blink

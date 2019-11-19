/*
 * Copyright (C) 2004, 2005, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_stop_element.h"

#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/svg/svg_gradient_element.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

SVGStopElement::SVGStopElement(Document& document)
    : SVGElement(svg_names::kStopTag, document),
      offset_(MakeGarbageCollected<SVGAnimatedNumber>(
          this,
          svg_names::kOffsetAttr,
          MakeGarbageCollected<SVGNumberAcceptPercentage>())) {
  AddToPropertyMap(offset_);

  // Since stop elements don't have corresponding layout objects, we rely on
  // style recalc callbacks for invalidation.
  DCHECK(HasCustomStyleCallbacks());
}

void SVGStopElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(offset_);
  SVGElement::Trace(visitor);
}

namespace {

void InvalidateInstancesAndAncestorResources(SVGStopElement* stop_element) {
  SVGElement::InvalidationGuard invalidation_guard(stop_element);

  Element* parent = stop_element->parentElement();
  if (auto* gradient = DynamicTo<SVGGradientElement>(parent))
    gradient->InvalidateGradient(layout_invalidation_reason::kChildChanged);
}

}  // namespace

void SVGStopElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == svg_names::kOffsetAttr) {
    InvalidateInstancesAndAncestorResources(this);
    return;
  }

  SVGElement::SvgAttributeChanged(attr_name);
}

void SVGStopElement::DidRecalcStyle(const StyleRecalcChange change) {
  SVGElement::DidRecalcStyle(change);

  InvalidateInstancesAndAncestorResources(this);
}

Color SVGStopElement::StopColorIncludingOpacity() const {
  const ComputedStyle* style = GetComputedStyle();

  // Normally, we should always have a computed style for <stop> elements. But
  // there are some odd corner cases (*cough* shadow DOM v0 undistributed light
  // tree *cough*) which leave it null.
  if (!style)
    return Color::kBlack;

  Color base_color = style->VisitedDependentColor(GetCSSPropertyStopColor());
  return base_color.CombineWithAlpha(style->SvgStyle().StopOpacity());
}

}  // namespace blink

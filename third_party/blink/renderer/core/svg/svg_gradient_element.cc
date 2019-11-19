/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_gradient_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/svg/gradient_attributes.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg/svg_stop_element.h"
#include "third_party/blink/renderer/core/svg/svg_transform_list.h"
#include "third_party/blink/renderer/platform/heap/heap.h"

namespace blink {

template <>
const SVGEnumerationMap& GetEnumerationMap<SVGSpreadMethodType>() {
  static const SVGEnumerationMap::Entry enum_items[] = {
      {kSVGSpreadMethodPad, "pad"},
      {kSVGSpreadMethodReflect, "reflect"},
      {kSVGSpreadMethodRepeat, "repeat"},
  };
  static const SVGEnumerationMap entries(enum_items);
  return entries;
}

SVGGradientElement::SVGGradientElement(const QualifiedName& tag_name,
                                       Document& document)
    : SVGElement(tag_name, document),
      SVGURIReference(this),
      gradient_transform_(MakeGarbageCollected<SVGAnimatedTransformList>(
          this,
          svg_names::kGradientTransformAttr,
          CSSPropertyID::kTransform)),
      spread_method_(
          MakeGarbageCollected<SVGAnimatedEnumeration<SVGSpreadMethodType>>(
              this,
              svg_names::kSpreadMethodAttr,
              kSVGSpreadMethodPad)),
      gradient_units_(MakeGarbageCollected<
                      SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>>(
          this,
          svg_names::kGradientUnitsAttr,
          SVGUnitTypes::kSvgUnitTypeObjectboundingbox)) {
  AddToPropertyMap(gradient_transform_);
  AddToPropertyMap(spread_method_);
  AddToPropertyMap(gradient_units_);
}

void SVGGradientElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(gradient_transform_);
  visitor->Trace(spread_method_);
  visitor->Trace(gradient_units_);
  visitor->Trace(target_id_observer_);
  SVGElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
}

void SVGGradientElement::BuildPendingResource() {
  ClearResourceReferences();
  if (!isConnected())
    return;
  Element* target = ObserveTarget(target_id_observer_, *this);
  if (auto* gradient = DynamicTo<SVGGradientElement>(target))
    AddReferenceTo(gradient);

  InvalidateGradient(layout_invalidation_reason::kSvgResourceInvalidated);
}

void SVGGradientElement::ClearResourceReferences() {
  UnobserveTarget(target_id_observer_);
  RemoveAllOutgoingReferences();
}

void SVGGradientElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == svg_names::kGradientTransformAttr) {
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kTransform,
        *gradient_transform_->CurrentValue()->CssValue());
    return;
  }
  SVGElement::CollectStyleForPresentationAttribute(name, value, style);
}

void SVGGradientElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (attr_name == svg_names::kGradientTransformAttr) {
    InvalidateSVGPresentationAttributeStyle();
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::FromAttribute(attr_name));
  }

  if (attr_name == svg_names::kGradientUnitsAttr ||
      attr_name == svg_names::kGradientTransformAttr ||
      attr_name == svg_names::kSpreadMethodAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    InvalidateGradient(layout_invalidation_reason::kAttributeChanged);
    return;
  }

  if (SVGURIReference::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    BuildPendingResource();
    return;
  }

  SVGElement::SvgAttributeChanged(attr_name);
}

Node::InsertionNotificationRequest SVGGradientElement::InsertedInto(
    ContainerNode& root_parent) {
  SVGElement::InsertedInto(root_parent);
  if (root_parent.isConnected())
    BuildPendingResource();
  return kInsertionDone;
}

void SVGGradientElement::RemovedFrom(ContainerNode& root_parent) {
  SVGElement::RemovedFrom(root_parent);
  if (root_parent.isConnected())
    ClearResourceReferences();
}

void SVGGradientElement::ChildrenChanged(const ChildrenChange& change) {
  SVGElement::ChildrenChanged(change);

  if (change.by_parser)
    return;

  InvalidateGradient(layout_invalidation_reason::kChildChanged);
}

void SVGGradientElement::InvalidateGradient(
    LayoutInvalidationReasonForTracing reason) {
  if (auto* layout_object = ToLayoutSVGResourceContainer(GetLayoutObject()))
    layout_object->InvalidateCacheAndMarkForLayout(reason);
}

void SVGGradientElement::InvalidateDependentGradients() {
  NotifyIncomingReferences([](SVGElement& element) {
    if (auto* gradient = DynamicTo<SVGGradientElement>(element)) {
      gradient->InvalidateGradient(
          layout_invalidation_reason::kSvgResourceInvalidated);
    }
  });
}

void SVGGradientElement::CollectCommonAttributes(
    GradientAttributes& attributes) const {
  if (!attributes.HasSpreadMethod() && spreadMethod()->IsSpecified())
    attributes.SetSpreadMethod(spreadMethod()->CurrentValue()->EnumValue());

  if (!attributes.HasGradientUnits() && gradientUnits()->IsSpecified())
    attributes.SetGradientUnits(gradientUnits()->CurrentValue()->EnumValue());

  if (!attributes.HasGradientTransform() &&
      HasTransform(SVGElement::kExcludeMotionTransform)) {
    attributes.SetGradientTransform(
        CalculateTransform(SVGElement::kExcludeMotionTransform));
  }

  if (!attributes.HasStops()) {
    const Vector<Gradient::ColorStop>& stops(BuildStops());
    if (!stops.IsEmpty())
      attributes.SetStops(stops);
  }
}

const SVGGradientElement* SVGGradientElement::ReferencedElement() const {
  // Respect xlink:href, take attributes from referenced element.
  return DynamicTo<SVGGradientElement>(
      TargetElementFromIRIString(HrefString(), GetTreeScope()));
}

Vector<Gradient::ColorStop> SVGGradientElement::BuildStops() const {
  Vector<Gradient::ColorStop> stops;

  float previous_offset = 0.0f;
  for (const SVGStopElement& stop :
       Traversal<SVGStopElement>::ChildrenOf(*this)) {
    // Figure out right monotonic offset.
    float offset = stop.offset()->CurrentValue()->Value();
    offset = std::min(std::max(previous_offset, offset), 1.0f);
    previous_offset = offset;

    stops.push_back(
        Gradient::ColorStop(offset, stop.StopColorIncludingOpacity()));
  }
  return stops;
}

}  // namespace blink

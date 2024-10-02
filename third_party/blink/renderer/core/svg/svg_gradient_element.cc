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

#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_container.h"
#include "third_party/blink/renderer/core/svg/gradient_attributes.h"
#include "third_party/blink/renderer/core/svg/svg_animated_number.h"
#include "third_party/blink/renderer/core/svg/svg_animated_transform_list.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg/svg_stop_element.h"
#include "third_party/blink/renderer/core/svg/svg_transform_list.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

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
          SVGUnitTypes::kSvgUnitTypeObjectboundingbox)) {}

void SVGGradientElement::Trace(Visitor* visitor) const {
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

  InvalidateGradient();
}

void SVGGradientElement::ClearResourceReferences() {
  UnobserveTarget(target_id_observer_);
  RemoveAllOutgoingReferences();
}

void SVGGradientElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (attr_name == svg_names::kGradientTransformAttr) {
    UpdatePresentationAttributeStyle(*gradient_transform_);
  }

  if (attr_name == svg_names::kGradientUnitsAttr ||
      attr_name == svg_names::kGradientTransformAttr ||
      attr_name == svg_names::kSpreadMethodAttr) {
    InvalidateGradient();
    return;
  }

  if (SVGURIReference::IsKnownAttribute(attr_name)) {
    BuildPendingResource();
    return;
  }

  SVGElement::SvgAttributeChanged(params);
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

  if (!change.ByParser())
    InvalidateGradient();
}

void SVGGradientElement::InvalidateGradient() {
  if (auto* layout_object = To<LayoutSVGResourceContainer>(GetLayoutObject()))
    layout_object->InvalidateCache();
}

void SVGGradientElement::InvalidateDependentGradients() {
  NotifyIncomingReferences([](SVGElement& element) {
    if (auto* gradient = DynamicTo<SVGGradientElement>(element)) {
      gradient->InvalidateGradient();
    }
  });
}

void SVGGradientElement::CollectCommonAttributes(
    GradientAttributes& attributes) const {
  if (!attributes.HasSpreadMethod() && spreadMethod()->IsSpecified())
    attributes.SetSpreadMethod(spreadMethod()->CurrentEnumValue());

  if (!attributes.HasGradientUnits() && gradientUnits()->IsSpecified())
    attributes.SetGradientUnits(gradientUnits()->CurrentEnumValue());

  if (!attributes.HasGradientTransform() &&
      HasTransform(SVGElement::kExcludeMotionTransform)) {
    attributes.SetGradientTransform(
        CalculateTransform(SVGElement::kExcludeMotionTransform));
  }

  if (!attributes.HasStops()) {
    attributes.SetStops(BuildStops());
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

SVGAnimatedPropertyBase* SVGGradientElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kGradientTransformAttr) {
    return gradient_transform_.Get();
  } else if (attribute_name == svg_names::kSpreadMethodAttr) {
    return spread_method_.Get();
  } else if (attribute_name == svg_names::kGradientUnitsAttr) {
    return gradient_units_.Get();
  } else {
    SVGAnimatedPropertyBase* ret =
        SVGURIReference::PropertyFromAttribute(attribute_name);
    if (ret) {
      return ret;
    } else {
      return SVGElement::PropertyFromAttribute(attribute_name);
    }
  }
}

void SVGGradientElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{gradient_transform_.Get(),
                                   spread_method_.Get(), gradient_units_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGURIReference::SynchronizeAllSVGAttributes();
  SVGElement::SynchronizeAllSVGAttributes();
}

void SVGGradientElement::CollectExtraStyleForPresentationAttribute(
    MutableCSSPropertyValueSet* style) {
  AddAnimatedPropertyToPresentationAttributeStyle(*gradient_transform_, style);
  SVGElement::CollectExtraStyleForPresentationAttribute(style);
}

}  // namespace blink

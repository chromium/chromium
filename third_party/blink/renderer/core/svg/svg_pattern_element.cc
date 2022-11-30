/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann
 * <zimmermann@kde.org>
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

#include "third_party/blink/renderer/core/svg/svg_pattern_element.h"

#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_resource_pattern.h"
#include "third_party/blink/renderer/core/svg/pattern_attributes.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_animated_preserve_aspect_ratio.h"
#include "third_party/blink/renderer/core/svg/svg_animated_rect.h"
#include "third_party/blink/renderer/core/svg/svg_animated_transform_list.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/transforms/affine_transform.h"

namespace blink {

SVGPatternElement::SVGPatternElement(Document& document)
    : SVGElement(svg_names::kPatternTag, document),
      SVGURIReference(this),
      SVGTests(this),
      SVGFitToViewBox(this),
      x_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kXAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kUnitlessZero)),
      y_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kYAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kUnitlessZero)),
      width_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kWidthAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kUnitlessZero)),
      height_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kHeightAttr,
          SVGLengthMode::kHeight,
          SVGLength::Initial::kUnitlessZero)),
      pattern_transform_(MakeGarbageCollected<SVGAnimatedTransformList>(
          this,
          svg_names::kPatternTransformAttr,
          CSSPropertyID::kTransform)),
      pattern_units_(MakeGarbageCollected<
                     SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>>(
          this,
          svg_names::kPatternUnitsAttr,
          SVGUnitTypes::kSvgUnitTypeObjectboundingbox)),
      pattern_content_units_(MakeGarbageCollected<
                             SVGAnimatedEnumeration<SVGUnitTypes::SVGUnitType>>(
          this,
          svg_names::kPatternContentUnitsAttr,
          SVGUnitTypes::kSvgUnitTypeUserspaceonuse)) {
  AddToPropertyMap(x_);
  AddToPropertyMap(y_);
  AddToPropertyMap(width_);
  AddToPropertyMap(height_);
  AddToPropertyMap(pattern_transform_);
  AddToPropertyMap(pattern_units_);
  AddToPropertyMap(pattern_content_units_);
}

void SVGPatternElement::Trace(Visitor* visitor) const {
  visitor->Trace(x_);
  visitor->Trace(y_);
  visitor->Trace(width_);
  visitor->Trace(height_);
  visitor->Trace(pattern_transform_);
  visitor->Trace(pattern_units_);
  visitor->Trace(pattern_content_units_);
  visitor->Trace(target_id_observer_);
  SVGElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
  SVGTests::Trace(visitor);
  SVGFitToViewBox::Trace(visitor);
}

void SVGPatternElement::BuildPendingResource() {
  ClearResourceReferences();
  if (!isConnected())
    return;
  Element* target = ObserveTarget(target_id_observer_, *this);
  if (auto* pattern = DynamicTo<SVGPatternElement>(target))
    AddReferenceTo(pattern);

  InvalidatePattern(layout_invalidation_reason::kSvgResourceInvalidated);
}

void SVGPatternElement::ClearResourceReferences() {
  UnobserveTarget(target_id_observer_);
  RemoveAllOutgoingReferences();
}

void SVGPatternElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == svg_names::kPatternTransformAttr) {
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kTransform,
        *pattern_transform_->CurrentValue()->CssValue());
    return;
  }
  SVGElement::CollectStyleForPresentationAttribute(name, value, style);
}

void SVGPatternElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  bool is_length_attr =
      attr_name == svg_names::kXAttr || attr_name == svg_names::kYAttr ||
      attr_name == svg_names::kWidthAttr || attr_name == svg_names::kHeightAttr;

  if (attr_name == svg_names::kPatternTransformAttr) {
    InvalidateSVGPresentationAttributeStyle();
    SetNeedsStyleRecalc(kLocalStyleChange,
                        StyleChangeReasonForTracing::FromAttribute(attr_name));
  }

  if (is_length_attr || attr_name == svg_names::kPatternUnitsAttr ||
      attr_name == svg_names::kPatternContentUnitsAttr ||
      attr_name == svg_names::kPatternTransformAttr ||
      SVGFitToViewBox::IsKnownAttribute(attr_name) ||
      SVGTests::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);

    if (is_length_attr)
      UpdateRelativeLengthsInformation();

    InvalidatePattern(layout_invalidation_reason::kAttributeChanged);
    return;
  }

  if (SVGURIReference::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    BuildPendingResource();
    return;
  }

  SVGElement::SvgAttributeChanged(params);
}

Node::InsertionNotificationRequest SVGPatternElement::InsertedInto(
    ContainerNode& root_parent) {
  SVGElement::InsertedInto(root_parent);
  if (root_parent.isConnected())
    BuildPendingResource();
  return kInsertionDone;
}

void SVGPatternElement::RemovedFrom(ContainerNode& root_parent) {
  SVGElement::RemovedFrom(root_parent);
  if (root_parent.isConnected())
    ClearResourceReferences();
}

void SVGPatternElement::ChildrenChanged(const ChildrenChange& change) {
  SVGElement::ChildrenChanged(change);

  if (!change.ByParser())
    InvalidatePattern(layout_invalidation_reason::kChildChanged);
}

void SVGPatternElement::InvalidatePattern(
    LayoutInvalidationReasonForTracing reason) {
  if (auto* layout_object = To<LayoutSVGResourceContainer>(GetLayoutObject()))
    layout_object->InvalidateCacheAndMarkForLayout(reason);
}

void SVGPatternElement::InvalidateDependentPatterns() {
  NotifyIncomingReferences([](SVGElement& element) {
    if (auto* pattern = DynamicTo<SVGPatternElement>(element)) {
      pattern->InvalidatePattern(
          layout_invalidation_reason::kSvgResourceInvalidated);
    }
  });
}

LayoutObject* SVGPatternElement::CreateLayoutObject(const ComputedStyle&,
                                                    LegacyLayout) {
  return MakeGarbageCollected<LayoutSVGResourcePattern>(this);
}

static void SetPatternAttributes(const SVGPatternElement& element,
                                 PatternAttributes& attributes) {
  if (!attributes.HasX() && element.x()->IsSpecified())
    attributes.SetX(element.x()->CurrentValue());

  if (!attributes.HasY() && element.y()->IsSpecified())
    attributes.SetY(element.y()->CurrentValue());

  if (!attributes.HasWidth() && element.width()->IsSpecified())
    attributes.SetWidth(element.width()->CurrentValue());

  if (!attributes.HasHeight() && element.height()->IsSpecified())
    attributes.SetHeight(element.height()->CurrentValue());

  if (!attributes.HasViewBox() && element.HasValidViewBox())
    attributes.SetViewBox(element.viewBox()->CurrentValue()->Rect());

  if (!attributes.HasPreserveAspectRatio() &&
      element.preserveAspectRatio()->IsSpecified()) {
    attributes.SetPreserveAspectRatio(
        element.preserveAspectRatio()->CurrentValue());
  }

  if (!attributes.HasPatternUnits() && element.patternUnits()->IsSpecified()) {
    attributes.SetPatternUnits(element.patternUnits()->CurrentEnumValue());
  }

  if (!attributes.HasPatternContentUnits() &&
      element.patternContentUnits()->IsSpecified()) {
    attributes.SetPatternContentUnits(
        element.patternContentUnits()->CurrentEnumValue());
  }

  if (!attributes.HasPatternTransform() &&
      element.HasTransform(SVGElement::kExcludeMotionTransform)) {
    attributes.SetPatternTransform(
        element.CalculateTransform(SVGElement::kExcludeMotionTransform));
  }

  if (!attributes.HasPatternContentElement() &&
      ElementTraversal::FirstWithin(element))
    attributes.SetPatternContentElement(&element);
}

const SVGPatternElement* SVGPatternElement::ReferencedElement() const {
  return DynamicTo<SVGPatternElement>(
      TargetElementFromIRIString(HrefString(), GetTreeScope()));
}

void SVGPatternElement::CollectPatternAttributes(
    PatternAttributes& attributes) const {
  HeapHashSet<Member<const SVGPatternElement>> processed_patterns;
  const SVGPatternElement* current = this;

  while (true) {
    SetPatternAttributes(*current, attributes);
    processed_patterns.insert(current);

    // If (xlink:)href links to another SVGPatternElement, allow attributes
    // from that element to override values this pattern didn't set.
    current = current->ReferencedElement();

    // Ignore the referenced pattern element if it is not attached.
    if (!current || !current->GetLayoutObject())
      break;
    // Cycle detection.
    if (processed_patterns.Contains(current))
      break;
  }
}

AffineTransform SVGPatternElement::LocalCoordinateSpaceTransform(
    CTMScope) const {
  return CalculateTransform(SVGElement::kExcludeMotionTransform);
}

bool SVGPatternElement::SelfHasRelativeLengths() const {
  return x_->CurrentValue()->IsRelative() || y_->CurrentValue()->IsRelative() ||
         width_->CurrentValue()->IsRelative() ||
         height_->CurrentValue()->IsRelative();
}

}  // namespace blink

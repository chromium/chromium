/*
 * Copyright (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2010 Rob Buis <rwlbuis@gmail.com>
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

#include "third_party/blink/renderer/core/svg/svg_text_path_element.h"

#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/layout/svg/layout_svg_text_path.h"
#include "third_party/blink/renderer/core/svg/svg_a_element.h"
#include "third_party/blink/renderer/core/svg/svg_animated_length.h"
#include "third_party/blink/renderer/core/svg/svg_enumeration_map.h"
#include "third_party/blink/renderer/core/svg/svg_path_element.h"
#include "third_party/blink/renderer/core/svg/svg_text_element.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

template <>
const SVGEnumerationMap& GetEnumerationMap<SVGTextPathMethodType>() {
  static const SVGEnumerationMap::Entry enum_items[] = {
      {kSVGTextPathMethodAlign, "align"},
      {kSVGTextPathMethodStretch, "stretch"},
  };
  static const SVGEnumerationMap entries(enum_items);
  return entries;
}

template <>
const SVGEnumerationMap& GetEnumerationMap<SVGTextPathSpacingType>() {
  static const SVGEnumerationMap::Entry enum_items[] = {
      {kSVGTextPathSpacingAuto, "auto"},
      {kSVGTextPathSpacingExact, "exact"},
  };
  static const SVGEnumerationMap entries(enum_items);
  return entries;
}

SVGTextPathElement::SVGTextPathElement(Document& document)
    : SVGTextContentElement(svg_names::kTextPathTag, document),
      SVGURIReference(this),
      start_offset_(MakeGarbageCollected<SVGAnimatedLength>(
          this,
          svg_names::kStartOffsetAttr,
          SVGLengthMode::kWidth,
          SVGLength::Initial::kUnitlessZero)),
      method_(
          MakeGarbageCollected<SVGAnimatedEnumeration<SVGTextPathMethodType>>(
              this,
              svg_names::kMethodAttr,
              kSVGTextPathMethodAlign)),
      spacing_(
          MakeGarbageCollected<SVGAnimatedEnumeration<SVGTextPathSpacingType>>(
              this,
              svg_names::kSpacingAttr,
              kSVGTextPathSpacingExact)) {}

SVGTextPathElement::~SVGTextPathElement() = default;

void SVGTextPathElement::Trace(Visitor* visitor) const {
  visitor->Trace(start_offset_);
  visitor->Trace(method_);
  visitor->Trace(spacing_);
  visitor->Trace(target_id_observer_);
  SVGTextContentElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
}

void SVGTextPathElement::ClearResourceReferences() {
  UnobserveTarget(target_id_observer_);
  RemoveAllOutgoingReferences();
}

void SVGTextPathElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  const QualifiedName& attr_name = params.name;
  if (SVGURIReference::IsKnownAttribute(attr_name)) {
    BuildPendingResource();
    return;
  }

  if (attr_name == svg_names::kStartOffsetAttr)
    UpdateRelativeLengthsInformation();

  if (attr_name == svg_names::kStartOffsetAttr ||
      attr_name == svg_names::kMethodAttr ||
      attr_name == svg_names::kSpacingAttr) {
    if (LayoutObject* object = GetLayoutObject())
      MarkForLayoutAndParentResourceInvalidation(*object);

    return;
  }

  SVGTextContentElement::SvgAttributeChanged(params);
}

LayoutObject* SVGTextPathElement::CreateLayoutObject(const ComputedStyle&) {
  return MakeGarbageCollected<LayoutSVGTextPath>(this);
}

bool SVGTextPathElement::LayoutObjectIsNeeded(const DisplayStyle& style) const {
  if (parentNode() &&
      (IsA<SVGAElement>(*parentNode()) || IsA<SVGTextElement>(*parentNode())))
    return SVGElement::LayoutObjectIsNeeded(style);

  return false;
}

void SVGTextPathElement::BuildPendingResource() {
  ClearResourceReferences();
  if (!isConnected())
    return;
  Element* target = ObserveTarget(target_id_observer_, *this);
  if (IsA<SVGPathElement>(target)) {
    // Register us with the target in the dependencies map. Any change of
    // hrefElement that leads to relayout/repainting now informs us, so we can
    // react to it.
    AddReferenceTo(To<SVGElement>(target));
  }

  if (LayoutObject* layout_object = GetLayoutObject())
    MarkForLayoutAndParentResourceInvalidation(*layout_object);
}

Node::InsertionNotificationRequest SVGTextPathElement::InsertedInto(
    ContainerNode& root_parent) {
  SVGTextContentElement::InsertedInto(root_parent);
  BuildPendingResource();
  return kInsertionDone;
}

void SVGTextPathElement::RemovedFrom(ContainerNode& root_parent) {
  SVGTextContentElement::RemovedFrom(root_parent);
  if (root_parent.isConnected())
    ClearResourceReferences();
}

bool SVGTextPathElement::SelfHasRelativeLengths() const {
  return start_offset_->CurrentValue()->IsRelative() ||
         SVGTextContentElement::SelfHasRelativeLengths();
}

SVGAnimatedPropertyBase* SVGTextPathElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  if (attribute_name == svg_names::kStartOffsetAttr) {
    return start_offset_.Get();
  } else if (attribute_name == svg_names::kMethodAttr) {
    return method_.Get();
  } else if (attribute_name == svg_names::kSpacingAttr) {
    return spacing_.Get();
  } else {
    SVGAnimatedPropertyBase* ret =
        SVGURIReference::PropertyFromAttribute(attribute_name);
    if (ret) {
      return ret;
    } else {
      return SVGTextContentElement::PropertyFromAttribute(attribute_name);
    }
  }
}

void SVGTextPathElement::SynchronizeAllSVGAttributes() const {
  SVGAnimatedPropertyBase* attrs[]{start_offset_.Get(), method_.Get(),
                                   spacing_.Get()};
  SynchronizeListOfSVGAttributes(attrs);
  SVGURIReference::SynchronizeAllSVGAttributes();
  SVGTextContentElement::SynchronizeAllSVGAttributes();
}

}  // namespace blink

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

namespace blink {

template <>
const SVGEnumerationStringEntries&
GetStaticStringEntries<SVGTextPathMethodType>() {
  DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
  if (entries.IsEmpty()) {
    entries.push_back(std::make_pair(kSVGTextPathMethodAlign, "align"));
    entries.push_back(std::make_pair(kSVGTextPathMethodStretch, "stretch"));
  }
  return entries;
}

template <>
const SVGEnumerationStringEntries&
GetStaticStringEntries<SVGTextPathSpacingType>() {
  DEFINE_STATIC_LOCAL(SVGEnumerationStringEntries, entries, ());
  if (entries.IsEmpty()) {
    entries.push_back(std::make_pair(kSVGTextPathSpacingAuto, "auto"));
    entries.push_back(std::make_pair(kSVGTextPathSpacingExact, "exact"));
  }
  return entries;
}

inline SVGTextPathElement::SVGTextPathElement(Document& document)
    : SVGTextContentElement(svg_names::kTextPathTag, document),
      SVGURIReference(this),
      start_offset_(
          SVGAnimatedLength::Create(this,
                                    svg_names::kStartOffsetAttr,
                                    SVGLengthMode::kWidth,
                                    SVGLength::Initial::kUnitlessZero)),
      method_(SVGAnimatedEnumeration<SVGTextPathMethodType>::Create(
          this,
          svg_names::kMethodAttr,
          kSVGTextPathMethodAlign)),
      spacing_(SVGAnimatedEnumeration<SVGTextPathSpacingType>::Create(
          this,
          svg_names::kSpacingAttr,
          kSVGTextPathSpacingExact)) {
  AddToPropertyMap(start_offset_);
  AddToPropertyMap(method_);
  AddToPropertyMap(spacing_);
}

DEFINE_NODE_FACTORY(SVGTextPathElement)

SVGTextPathElement::~SVGTextPathElement() = default;

void SVGTextPathElement::Trace(blink::Visitor* visitor) {
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

void SVGTextPathElement::SvgAttributeChanged(const QualifiedName& attr_name) {
  if (SVGURIReference::IsKnownAttribute(attr_name)) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    BuildPendingResource();
    return;
  }

  if (attr_name == svg_names::kStartOffsetAttr)
    UpdateRelativeLengthsInformation();

  if (attr_name == svg_names::kStartOffsetAttr ||
      attr_name == svg_names::kMethodAttr ||
      attr_name == svg_names::kSpacingAttr) {
    SVGElement::InvalidationGuard invalidation_guard(this);
    if (LayoutObject* object = GetLayoutObject())
      MarkForLayoutAndParentResourceInvalidation(*object);

    return;
  }

  SVGTextContentElement::SvgAttributeChanged(attr_name);
}

LayoutObject* SVGTextPathElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutSVGTextPath(this);
}

bool SVGTextPathElement::LayoutObjectIsNeeded(
    const ComputedStyle& style) const {
  if (parentNode() &&
      (IsSVGAElement(*parentNode()) || IsSVGTextElement(*parentNode())))
    return SVGElement::LayoutObjectIsNeeded(style);

  return false;
}

void SVGTextPathElement::BuildPendingResource() {
  ClearResourceReferences();
  if (!isConnected())
    return;
  Element* target = ObserveTarget(target_id_observer_, *this);
  if (IsSVGPathElement(target)) {
    // Register us with the target in the dependencies map. Any change of
    // hrefElement that leads to relayout/repainting now informs us, so we can
    // react to it.
    AddReferenceTo(ToSVGElement(target));
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

}  // namespace blink

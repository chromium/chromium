/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "third_party/blink/renderer/core/svg/svg_mpath_element.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/id_target_observer.h"
#include "third_party/blink/renderer/core/svg/svg_animate_motion_element.h"
#include "third_party/blink/renderer/core/svg/svg_path_element.h"
#include "third_party/blink/renderer/core/svg_names.h"

namespace blink {

SVGMPathElement::SVGMPathElement(Document& document)
    : SVGElement(svg_names::kMPathTag, document), SVGURIReference(this) {}

void SVGMPathElement::Trace(Visitor* visitor) const {
  visitor->Trace(target_id_observer_);
  SVGElement::Trace(visitor);
  SVGURIReference::Trace(visitor);
}

SVGMPathElement::~SVGMPathElement() = default;

SVGAnimatedPropertyBase* SVGMPathElement::PropertyFromAttribute(
    const QualifiedName& attribute_name) const {
  SVGAnimatedPropertyBase* ret =
      SVGURIReference::PropertyFromAttribute(attribute_name);
  if (ret) {
    return ret;
  } else {
    return SVGElement::PropertyFromAttribute(attribute_name);
  }
}

void SVGMPathElement::SynchronizeAllSVGAttributes() const {
  SVGURIReference::SynchronizeAllSVGAttributes();
  SVGElement::SynchronizeAllSVGAttributes();
}

void SVGMPathElement::BuildPendingResource() {
  ClearResourceReferences();
  if (!isConnected())
    return;
  Element* target = ObserveTarget(target_id_observer_, *this);
  if (auto* path = DynamicTo<SVGPathElement>(target)) {
    // Register us with the target in the dependencies map. Any change of
    // hrefElement that leads to relayout/repainting now informs us, so we can
    // react to it.
    AddReferenceTo(path);
  }
  TargetPathChanged();
}

void SVGMPathElement::ClearResourceReferences() {
  UnobserveTarget(target_id_observer_);
  RemoveAllOutgoingReferences();
}

Node::InsertionNotificationRequest SVGMPathElement::InsertedInto(
    ContainerNode& root_parent) {
  SVGElement::InsertedInto(root_parent);
  if (root_parent.isConnected())
    BuildPendingResource();
  return kInsertionDone;
}

void SVGMPathElement::RemovedFrom(ContainerNode& root_parent) {
  SVGElement::RemovedFrom(root_parent);
  NotifyParentOfPathChange(&root_parent);
  if (root_parent.isConnected())
    ClearResourceReferences();
}

void SVGMPathElement::SvgAttributeChanged(
    const SvgAttributeChangedParams& params) {
  if (SVGURIReference::IsKnownAttribute(params.name)) {
    BuildPendingResource();
    return;
  }

  SVGElement::SvgAttributeChanged(params);
}

SVGPathElement* SVGMPathElement::PathElement() {
  Element* target = TargetElementFromIRIString(HrefString(), GetTreeScope());
  return DynamicTo<SVGPathElement>(target);
}

void SVGMPathElement::TargetPathChanged() {
  NotifyParentOfPathChange(parentNode());
}

void SVGMPathElement::NotifyParentOfPathChange(ContainerNode* parent) {
  if (auto* motion = DynamicTo<SVGAnimateMotionElement>(parent)) {
    motion->ChildMPathChanged();
  }
}

}  // namespace blink

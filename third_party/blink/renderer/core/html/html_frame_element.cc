/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2009 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/html_frame_element.h"

#include "third_party/blink/public/mojom/feature_policy/feature_policy.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/html/html_frame_set_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_frame.h"

namespace blink {

HTMLFrameElement::HTMLFrameElement(Document& document)
    : HTMLFrameElementBase(html_names::kFrameTag, document),
      frame_border_(true),
      frame_border_set_(false) {}

bool HTMLFrameElement::LayoutObjectIsNeeded(const ComputedStyle&) const {
  // For compatibility, frames render even when display: none is set.
  return ContentFrame();
}

LayoutObject* HTMLFrameElement::CreateLayoutObject(const ComputedStyle&,
                                                   LegacyLayout) {
  return new LayoutFrame(this);
}

bool HTMLFrameElement::NoResize() const {
  return FastHasAttribute(html_names::kNoresizeAttr);
}

void HTMLFrameElement::AttachLayoutTree(AttachContext& context) {
  HTMLFrameElementBase::AttachLayoutTree(context);

  if (HTMLFrameSetElement* frame_set_element =
          Traversal<HTMLFrameSetElement>::FirstAncestor(*this)) {
    if (!frame_border_set_)
      frame_border_ = frame_set_element->HasFrameBorder();
  }
}

void HTMLFrameElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kFrameborderAttr) {
    frame_border_ = params.new_value.ToInt();
    frame_border_set_ = !params.new_value.IsNull();
    // FIXME: If we are already attached, this has no effect.
  } else if (params.name == html_names::kNoresizeAttr) {
    if (GetLayoutObject())
      GetLayoutObject()->UpdateFromElement();
  } else {
    HTMLFrameElementBase::ParseAttribute(params);
  }
}

ParsedFeaturePolicy HTMLFrameElement::ConstructContainerPolicy(
    Vector<String>*) const {
  // Frame elements are not allowed to enable the fullscreen feature. Add an
  // empty allowlist for the fullscreen feature so that the framed content is
  // unable to use the API, regardless of origin.
  // https://fullscreen.spec.whatwg.org/#model
  ParsedFeaturePolicy container_policy;
  ParsedFeaturePolicyDeclaration allowlist(
      mojom::FeaturePolicyFeature::kFullscreen, mojom::PolicyValueType::kBool);
  container_policy.push_back(allowlist);
  return container_policy;
}

}  // namespace blink

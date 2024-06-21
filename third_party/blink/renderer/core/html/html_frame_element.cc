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

#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/policy_value.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/html/frame_edge_info.h"
#include "third_party/blink/renderer/core/html/html_frame_set_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_frame.h"

namespace blink {

HTMLFrameElement::HTMLFrameElement(Document& document)
    : HTMLFrameElementBase(html_names::kFrameTag, document),
      frame_border_(true),
      frame_border_set_(false) {}

bool HTMLFrameElement::LayoutObjectIsNeeded(const DisplayStyle&) const {
  // For compatibility, frames render even when display: none is set.
  return ContentFrame();
}

LayoutObject* HTMLFrameElement::CreateLayoutObject(const ComputedStyle& style) {
  if (IsA<HTMLFrameSetElement>(parentNode()))
    return MakeGarbageCollected<LayoutFrame>(this);
  return LayoutObject::CreateObject(this, style);
}

bool HTMLFrameElement::HasFrameBorder() const {
  if (!frame_border_set_) {
    if (const auto* frame_set = DynamicTo<HTMLFrameSetElement>(parentNode()))
      return frame_set->HasFrameBorder();
  }
  return frame_border_;
}

bool HTMLFrameElement::NoResize() const {
  return FastHasAttribute(html_names::kNoresizeAttr);
}

FrameEdgeInfo HTMLFrameElement::EdgeInfo() const {
  return FrameEdgeInfo(NoResize(), HasFrameBorder());
}

void HTMLFrameElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kFrameborderAttr) {
    frame_border_ = params.new_value.ToInt();
    frame_border_set_ = !params.new_value.IsNull();
    if (auto* frame_set = DynamicTo<HTMLFrameSetElement>(parentNode()))
      frame_set->DirtyEdgeInfoAndFullPaintInvalidation();
  } else if (params.name == html_names::kNoresizeAttr) {
    if (auto* frame_set = DynamicTo<HTMLFrameSetElement>(parentNode()))
      frame_set->DirtyEdgeInfo();
  } else {
    HTMLFrameElementBase::ParseAttribute(params);
  }
}

ParsedPermissionsPolicy HTMLFrameElement::ConstructContainerPolicy() const {
  return GetLegacyFramePolicies();
}

}  // namespace blink

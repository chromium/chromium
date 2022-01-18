// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc_context.h"

#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"

namespace blink {

namespace {

Element* ClosestInclusiveAncestorContainer(Element& element) {
  for (auto* container = &element; container;
       container = container->ParentOrShadowHostElement()) {
    if (container->GetContainerQueryEvaluator())
      return container;
  }
  return nullptr;
}

}  // namespace

StyleRecalcContext StyleRecalcContext::FromInclusiveAncestors(
    Element& element) {
  if (!RuntimeEnabledFeatures::CSSContainerQueriesEnabled())
    return StyleRecalcContext();

  return StyleRecalcContext{ClosestInclusiveAncestorContainer(element)};
}

StyleRecalcContext StyleRecalcContext::FromAncestors(Element& element) {
  if (!RuntimeEnabledFeatures::CSSContainerQueriesEnabled())
    return StyleRecalcContext();

  // TODO(crbug.com/1145970): Avoid this work if we're not inside a container
  if (Element* shadow_including_parent = element.ParentOrShadowHostElement())
    return FromInclusiveAncestors(*shadow_including_parent);
  return StyleRecalcContext();
}

StyleRecalcContext StyleRecalcContext::ForSlotChildren(
    const HTMLSlotElement& slot) const {
  // If the container is in a different tree scope, it is already in the shadow-
  // including inclusive ancestry of the host.
  if (!container || container->GetTreeScope() != slot.GetTreeScope())
    return *this;

  DCHECK(RuntimeEnabledFeatures::CSSContainerQueriesEnabled());

  // No assigned nodes means we will render the light tree children of the
  // slot as a fallback. Those children are in the same tree scope as the slot
  // which means the current container is the correct one.
  if (slot.AssignedNodes().IsEmpty())
    return *this;

  // The slot's flat tree children are children of the slot's shadow host, and
  // their container is in the shadow-including inclusive ancestors of the host.
  DCHECK(slot.IsInShadowTree());
  Element* host = slot.OwnerShadowHost();
  DCHECK(host);
  return StyleRecalcContext{FromInclusiveAncestors(*host)};
}

}  // namespace blink

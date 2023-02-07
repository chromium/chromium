// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc_context.h"

#include "third_party/blink/renderer/core/dom/layout_tree_builder_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"

namespace blink {

namespace {

Element* ClosestInclusiveAncestorContainer(Element& element,
                                           Element* stay_within = nullptr) {
  for (auto* container = &element; container && container != stay_within;
       container = container->ParentOrShadowHostElement()) {
    const ComputedStyle* style = container->GetComputedStyle();
    if (!style) {
      // TODO(crbug.com/1400631): Eliminate all invalid calls to
      // StyleRecalcContext::From[Inclusive]Ancestors, then either turn
      // if (!style) into CHECK(style) or simplify into checking:
      // container->GetComputedStyle()->IsContainerForSizeContainerQueries()
      //
      // This used to use base::debug::DumpWithoutCrashing() but generated too
      // many failures in the wild to keep around (would upload too many crash
      // reports). Consider adding UMA stats back if we want to track this or
      // land a strategy to figure it out and fix what's going on.
      return nullptr;
    }
    if (style->IsContainerForSizeContainerQueries()) {
      return container;
    }
  }
  return nullptr;
}

}  // namespace

StyleRecalcContext StyleRecalcContext::FromInclusiveAncestors(
    Element& element) {
  return StyleRecalcContext{ClosestInclusiveAncestorContainer(element)};
}

StyleRecalcContext StyleRecalcContext::FromAncestors(Element& element) {
  // TODO(crbug.com/1145970): Avoid this work if we're not inside a container
  if (Element* shadow_including_parent = element.ParentOrShadowHostElement()) {
    return FromInclusiveAncestors(*shadow_including_parent);
  }
  return StyleRecalcContext();
}

StyleRecalcContext StyleRecalcContext::ForSlotChildren(
    const HTMLSlotElement& slot) const {
  // If the container is in a different tree scope, it is already in the shadow-
  // including inclusive ancestry of the host.
  if (!container || container->GetTreeScope() != slot.GetTreeScope()) {
    return *this;
  }

  // No assigned nodes means we will render the light tree children of the
  // slot as a fallback. Those children are in the same tree scope as the slot
  // which means the current container is the correct one.
  if (slot.AssignedNodes().empty()) {
    return *this;
  }

  // The slot's flat tree children are children of the slot's shadow host, and
  // their container is in the shadow-including inclusive ancestors of the host.
  DCHECK(slot.IsInShadowTree());
  Element* host = slot.OwnerShadowHost();
  DCHECK(host);
  return StyleRecalcContext{FromInclusiveAncestors(*host)};
}

StyleRecalcContext StyleRecalcContext::ForSlottedRules(
    HTMLSlotElement& slot) const {
  // The current container is the shadow-including inclusive ancestors of the
  // host. When matching ::slotted rules, the closest container may be found in
  // the shadow-including inclusive ancestry of the slot. If we reach the host,
  // the current container is still the closest one.
  if (Element* shadow_container =
          ClosestInclusiveAncestorContainer(slot, slot.OwnerShadowHost())) {
    return StyleRecalcContext{shadow_container};
  }
  return *this;
}

StyleRecalcContext StyleRecalcContext::ForPartRules(Element& host) const {
  DCHECK(IsShadowHost(host));

  if (!container) {
    return *this;
  }

  // The closest container for matching ::part rules is the originating host.
  return StyleRecalcContext{ClosestInclusiveAncestorContainer(host)};
}

}  // namespace blink

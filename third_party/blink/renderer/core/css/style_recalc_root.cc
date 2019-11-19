// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc_root.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"

namespace blink {

Element& StyleRecalcRoot::RootElement() const {
  Node* root_node = GetRootNode();
  DCHECK(root_node);
  if (root_node->IsDocumentNode())
    return *root_node->GetDocument().documentElement();
  if (root_node->IsPseudoElement()) {
    // We could possibly have called UpdatePseudoElement, but start at the
    // originating element for simplicity.
    return *root_node->parentElement();
  }
  if (!RuntimeEnabledFeatures::FlatTreeStyleRecalcEnabled()) {
    if (root_node->IsInShadowTree()) {
      // Since we traverse in light tree order, we might need to traverse
      // slotted shadow host children for inheritance for which the recalc root
      // is not an ancestor. Since we might re-slot slots, we need to start at
      // the outermost shadow host.
      TreeScope* tree_scope = &root_node->GetTreeScope();
      while (!tree_scope->ParentTreeScope()->RootNode().IsDocumentNode())
        tree_scope = tree_scope->ParentTreeScope();
      return To<ShadowRoot>(tree_scope->RootNode()).host();
    }
  }
  if (root_node->IsTextNode())
    root_node = root_node->GetStyleRecalcParent();
  return To<Element>(*root_node);
}

#if DCHECK_IS_ON()
ContainerNode* StyleRecalcRoot::Parent(const Node& node) const {
  return node.GetStyleRecalcParent();
}
#endif  // DCHECK_IS_ON()

bool StyleRecalcRoot::IsDirty(const Node& node) const {
  return node.IsDirtyForStyleRecalc();
}

namespace {

base::Optional<Member<Element>> FirstFlatTreeAncestorForChildDirty(
    ContainerNode& parent) {
  DCHECK(RuntimeEnabledFeatures::FlatTreeStyleRecalcEnabled());
  if (!parent.IsElementNode()) {
    // The flat tree does not contain shadow roots or the document node. The
    // closest ancestor for dirty bits is the shadow host or nullptr.
    return parent.ParentOrShadowHostElement();
  }
  ShadowRoot* root = parent.GetShadowRoot();
  if (!root)
    return To<Element>(&parent);
  if (root->IsV0()) {
    // Fall back to use the parent as the new style recalc root for Shadow DOM
    // V0. It is too complicated to try to find the closest flat tree parent.
    return base::nullopt;
  }
  if (!root->HasSlotAssignment())
    return base::nullopt;
  // The child has already been removed, so we cannot look up its slot
  // assignment directly. Find the slot which was part of the ancestor chain
  // before the removal by checking the child-dirty bits. Since the recalc root
  // was removed, there is at most one such child-dirty slot.
  for (const auto slot : root->GetSlotAssignment().Slots()) {
    if (slot->ChildNeedsStyleRecalc())
      return slot;
  }
  // The slot has also been removed. Fall back to using the light tree parent as
  // the new recalc root.
  return base::nullopt;
}

}  // namespace

void StyleRecalcRoot::RootRemoved(ContainerNode& parent) {
  ContainerNode* ancestor = &parent;
  if (RuntimeEnabledFeatures::FlatTreeStyleRecalcEnabled()) {
    // We are notified with the light tree parent of the node(s) which were
    // removed from the DOM. If 'parent' is a shadow host, there are elements in
    // its shadow tree which are marked child-dirty which needs to be cleared in
    // order to clear the recalc root below. If we are not able to find the
    // closest flat tree ancestor for traversal, fall back to using the 'parent'
    // as the new recalc root to allow the child-dirty bits to be cleared on the
    // next style recalc.
    auto opt_ancestor = FirstFlatTreeAncestorForChildDirty(parent);
    if (!opt_ancestor) {
      Update(&parent, &parent);
      DCHECK(!IsSingleRoot());
      DCHECK_EQ(GetRootNode(), ancestor);
      return;
    }
    ancestor = opt_ancestor.value();
  }
  for (; ancestor; ancestor = ancestor->GetStyleRecalcParent()) {
    DCHECK(ancestor->ChildNeedsStyleRecalc());
    DCHECK(!ancestor->NeedsStyleRecalc());
    ancestor->ClearChildNeedsStyleRecalc();
  }
  Clear();
}

}  // namespace blink

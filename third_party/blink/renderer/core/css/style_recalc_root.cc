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
  if (!parent.IsElementNode()) {
    // The flat tree does not contain shadow roots or the document node. The
    // closest ancestor for dirty bits is the shadow host or nullptr.
    return parent.ParentOrShadowHostElement();
  }
  ShadowRoot* root = parent.GetShadowRoot();
  if (!root)
    return To<Element>(&parent);
  if (!root->HasSlotAssignment())
    return base::nullopt;
  // The child has already been removed, so we cannot look up its slot
  // assignment directly. Find the slot which was part of the ancestor chain
  // before the removal by checking the child-dirty bits. Since the recalc root
  // was removed, there is at most one such child-dirty slot.
  for (const auto& slot : root->GetSlotAssignment().Slots()) {
    if (slot->ChildNeedsStyleRecalc())
      return slot;
  }
  // The slot has also been removed. Fall back to using the light tree parent as
  // the new recalc root.
  return base::nullopt;
}

}  // namespace

void StyleRecalcRoot::RootRemoved(ContainerNode& parent) {
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
    DCHECK_EQ(GetRootNode(), &parent);
    return;
  }
  for (Element* ancestor = opt_ancestor.value(); ancestor;
       ancestor = ancestor->GetStyleRecalcParent()) {
    DCHECK(ancestor->ChildNeedsStyleRecalc());
    DCHECK(!ancestor->NeedsStyleRecalc());
    ancestor->ClearChildNeedsStyleRecalc();
  }
  Clear();
}

void StyleRecalcRoot::RemovedFromFlatTree(const Node& node) {
  if (!GetRootNode())
    return;
  if (GetRootNode()->IsDocumentNode())
    return;
  // If the recalc root is the removed node, or if it's a descendant of the root
  // node, the recalc flags will be cleared in DetachLayoutTree() since
  // performing_reattach=false. If that's the case, call RootRemoved() below to
  // make sure we don't have a recalc root outside the flat tree, which is not
  // allowed with FlatTreeStyleRecalc enabled.
  if (GetRootNode()->NeedsStyleRecalc() ||
      GetRootNode()->GetForceReattachLayoutTree() ||
      GetRootNode()->ChildNeedsStyleRecalc()) {
    return;
  }
  DCHECK(node.parentElement());
  RootRemoved(*node.parentElement());
}

}  // namespace blink

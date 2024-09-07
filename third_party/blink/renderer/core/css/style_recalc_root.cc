// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/style_recalc_root.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/slot_assignment.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"

namespace blink {

Element& StyleRecalcRoot::RootElement() const {
  Node* root_node = GetRootNode();
  DCHECK(root_node);
  if (root_node->IsDocumentNode()) {
    return *root_node->GetDocument().documentElement();
  }
  if (root_node->IsPseudoElement()) {
    // We could possibly have called UpdatePseudoElement, but start at the
    // originating element for simplicity.
    return *root_node->parentElement();
  }
  if (root_node->IsTextNode()) {
    root_node = root_node->GetStyleRecalcParent();
  }
  return To<Element>(*root_node);
}

#if DCHECK_IS_ON()
ContainerNode* StyleRecalcRoot::Parent(const Node& node) const {
  return node.GetStyleRecalcParent();
}

bool StyleRecalcRoot::IsChildDirty(const Node& node) const {
  return node.ChildNeedsStyleRecalc();
}
#endif  // DCHECK_IS_ON()

bool StyleRecalcRoot::IsDirty(const Node& node) const {
  return node.IsDirtyForStyleRecalc();
}

namespace {

// Returns a pair. The first element in the pair is a boolean representing
// whether finding an ancestor succeeded. The second element in the pair is a
// pointer to the ancestor.
std::pair<bool, Element*> FirstFlatTreeAncestorForChildDirty(
    ContainerNode& parent) {
  if (!parent.IsElementNode()) {
    // The flat tree does not contain shadow roots or the document node. The
    // closest ancestor for dirty bits is the shadow host or nullptr.
    return {true, parent.ParentOrShadowHostElement()};
  }
  ShadowRoot* root = parent.GetShadowRoot();
  if (!root) {
    return {true, To<Element>(&parent)};
  }
  if (!root->HasSlotAssignment()) {
    return {false, nullptr};
  }
  // The child has already been removed, so we cannot look up its slot
  // assignment directly. Find the slot which was part of the ancestor chain
  // before the removal by checking the child-dirty bits. Since the recalc root
  // was removed, there is at most one such child-dirty slot.
  for (const auto& slot : root->GetSlotAssignment().Slots()) {
    if (slot->ChildNeedsStyleRecalc()) {
      return {true, slot};
    }
  }
  // The slot has also been removed. Fall back to using the light tree parent as
  // the new recalc root.
  return {false, nullptr};
}

bool IsFlatTreeConnected(const Node& root) {
  if (!root.isConnected()) {
    return false;
  }
  // If the recalc root is removed from the flat tree because its assigned slot
  // is removed from the flat tree, the recalc flags will be cleared in
  // DetachLayoutTree() with performing_reattach=false. We use that to decide if
  // the root node is no longer part of the flat tree.
  return root.IsDirtyForStyleRecalc() || root.ChildNeedsStyleRecalc();
}

}  // namespace

void StyleRecalcRoot::SubtreeModified(ContainerNode& parent) {
  if (!GetRootNode()) {
    return;
  }
  if (GetRootNode()->IsDocumentNode()) {
    return;
  }
  if (IsFlatTreeConnected(*GetRootNode())) {
    return;
  }
  // We are notified with the light tree parent of the node(s) which were
  // removed from the DOM. If 'parent' is a shadow host, there are elements in
  // its shadow tree which are marked child-dirty which needs to be cleared in
  // order to clear the recalc root below. If we are not able to find the
  // closest flat tree ancestor for traversal, fall back to using the 'parent'
  // as the new recalc root to allow the child-dirty bits to be cleared on the
  // next style recalc.
  auto opt_ancestor = FirstFlatTreeAncestorForChildDirty(parent);
  if (!opt_ancestor.first) {
    ContainerNode* common_ancestor = &parent;
    ContainerNode* new_root = &parent;
    if (!IsFlatTreeConnected(parent)) {
      // Fall back to the document root element since the flat tree is in a
      // state where we do not know what a suitable common ancestor would be.
      common_ancestor = nullptr;
      new_root = parent.GetDocument().documentElement();
    }
    Update(common_ancestor, new_root);
    DCHECK(!IsSingleRoot());
    DCHECK_EQ(GetRootNode(), new_root);
    return;
  }
  for (Element* ancestor = opt_ancestor.second; ancestor;
       ancestor = ancestor->GetStyleRecalcParent()) {
    DCHECK(ancestor->ChildNeedsStyleRecalc());
    DCHECK(!ancestor->NeedsStyleRecalc());
    ancestor->ClearChildNeedsStyleRecalc();
  }
  Clear();
}

void StyleRecalcRoot::FlatTreePositionChanged(const Node& node) {
  if (!GetRootNode()) {
    return;
  }
  if (GetRootNode()->IsDocumentNode()) {
    return;
  }
  DCHECK(node.parentElement());
  SubtreeModified(*node.parentElement());
}

}  // namespace blink

// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/css/layout_tree_rebuild_root.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"

namespace blink {

Element& LayoutTreeRebuildRoot::RootElement() const {
  Node* root_node = GetRootNode();
  DCHECK(root_node);
  DCHECK(root_node->isConnected());
  DCHECK(root_node->GetDocument().documentElement());
  // We need to start from the closest non-dirty ancestor which has a
  // LayoutObject to make WhitespaceAttacher work correctly because text node
  // siblings of nodes being re-attached needs to be traversed to re-evaluate
  // the need for a LayoutText. Single roots are typically dirty, but we need an
  // extra check for IsSingleRoot() because we mark nodes which have siblings
  // removed with MarkAncestorsWithChildNeedsReattachLayoutTree() in
  // Element::RecalcStyle() if the LayoutObject is marked with
  // WhitespaceChildrenMayChange(). In that case we need to start from the
  // ancestor to traverse all whitespace siblings.
  if (IsSingleRoot() || root_node->IsDirtyForRebuildLayoutTree() ||
      !root_node->GetLayoutObject()) {
    Element* root_element = root_node->GetReattachParent();
    while (root_element && !root_element->GetLayoutObject()) {
      root_element = root_element->GetReattachParent();
    }
    if (root_element) {
      return *root_element;
    }
  }
  if (Element* element = DynamicTo<Element>(root_node)) {
    return *element;
  }
  return *root_node->GetDocument().documentElement();
}

#if DCHECK_IS_ON()
ContainerNode* LayoutTreeRebuildRoot::Parent(const Node& node) const {
  return node.GetReattachParent();
}

bool LayoutTreeRebuildRoot::IsChildDirty(const Node& node) const {
  return node.ChildNeedsReattachLayoutTree();
}
#endif  // DCHECK_IS_ON()

bool LayoutTreeRebuildRoot::IsDirty(const Node& node) const {
  return node.IsDirtyForRebuildLayoutTree();
}

void LayoutTreeRebuildRoot::SubtreeModified(ContainerNode& parent) {
  if (!GetRootNode()) {
    return;
  }
  if (GetRootNode()->isConnected()) {
    return;
  }
  // LayoutTreeRebuildRoot is only used for marking for layout tree rebuild
  // during style recalc. We do not allow DOM modifications during style recalc
  // or the layout tree rebuild that happens right after. The only time we
  // should end up here is when we find out that we need to remove generated
  // pseudo elements like ::first-letter or ::marker during layout tree rebuild.
  DCHECK(parent.isConnected());
  DCHECK(GetRootNode()->IsPseudoElement());
  Element* ancestor = DynamicTo<Element>(parent);
  if (!ancestor) {
    // The parent should be the pseudo element's originating element.
    NOTREACHED_IN_MIGRATION();
    ancestor = parent.ParentOrShadowHostElement();
  }
  for (; ancestor; ancestor = ancestor->GetReattachParent()) {
    DCHECK(ancestor->ChildNeedsReattachLayoutTree());
    DCHECK(!ancestor->IsDirtyForRebuildLayoutTree());
    ancestor->ClearChildNeedsReattachLayoutTree();
  }
  Clear();
}

}  // namespace blink

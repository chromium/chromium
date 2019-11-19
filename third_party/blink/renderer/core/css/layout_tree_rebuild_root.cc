// Copyright 2018 The Chromium Authors. All rights reserved.
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
  // StyleEngine::MarkForWhitespaceReattachment(). In that case we need to start
  // from the ancestor to traverse all whitespace siblings.
  if (IsSingleRoot() || root_node->NeedsReattachLayoutTree() ||
      !root_node->GetLayoutObject()) {
    Element* root_element = root_node->GetReattachParent();
    while (root_element && !root_element->GetLayoutObject())
      root_element = root_element->GetReattachParent();
    if (root_element)
      return *root_element;
  }
  if (Element* element = DynamicTo<Element>(root_node))
    return *element;
  return *root_node->GetDocument().documentElement();
}

#if DCHECK_IS_ON()
ContainerNode* LayoutTreeRebuildRoot::Parent(const Node& node) const {
  return node.GetReattachParent();
}
#endif  // DCHECK_IS_ON()

bool LayoutTreeRebuildRoot::IsDirty(const Node& node) const {
  return node.NeedsReattachLayoutTree();
}

void LayoutTreeRebuildRoot::RootRemoved(ContainerNode& parent) {
  Element* ancestor = DynamicTo<Element>(parent);
  if (!ancestor)
    ancestor = parent.ParentOrShadowHostElement();
  for (; ancestor; ancestor = ancestor->GetReattachParent()) {
    DCHECK(ancestor->ChildNeedsReattachLayoutTree());
    DCHECK(!ancestor->NeedsReattachLayoutTree());
    ancestor->ClearChildNeedsReattachLayoutTree();
  }
  Clear();
}

}  // namespace blink

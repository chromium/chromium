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
    do {
      root_node = root_node->GetReattachParent();
    } while (root_node && !root_node->GetLayoutObject());
  }
  if (!root_node || root_node->IsDocumentNode())
    return *GetRootNode()->GetDocument().documentElement();
  return ToElement(*root_node);
}

#if DCHECK_IS_ON()
ContainerNode* LayoutTreeRebuildRoot::Parent(const Node& node) const {
  return node.GetReattachParent();
}

bool LayoutTreeRebuildRoot::IsChildDirty(const ContainerNode& node) const {
  return node.ChildNeedsReattachLayoutTree();
}
#endif  // DCHECK_IS_ON()

bool LayoutTreeRebuildRoot::IsDirty(const Node& node) const {
  return node.NeedsReattachLayoutTree();
}

void LayoutTreeRebuildRoot::ClearChildDirtyForAncestors(
    ContainerNode& parent) const {
  for (ContainerNode* ancestor = &parent; ancestor;
       ancestor = ancestor->GetReattachParent()) {
    ancestor->ClearChildNeedsReattachLayoutTree();
    DCHECK(!ancestor->NeedsReattachLayoutTree());
  }
}

}  // namespace blink

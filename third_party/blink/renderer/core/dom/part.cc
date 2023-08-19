// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/part.h"
#include "third_party/blink/renderer/core/dom/child_node_part.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_part_root.h"
#include "third_party/blink/renderer/core/dom/node_move_scope.h"
#include "third_party/blink/renderer/core/dom/part_root.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

Part::Part(PartRoot& root, const Vector<String> metadata)
    : root_(root), metadata_(metadata) {
  root.AddPart(*this);
}

// When disconnecting a Node/Part, if the root() is the DocumentPartRoot,
// then we disconnect the Part from the PartRoot so that it can be attached
// to the new PartRoot when reconnected. For any Part type except a
// ChildNodePart, that's all we do. For ChildNodeParts, the node being moved
// could be either the `previous_sibling` or the `next_sibling`. If we're
// moving the parent Node, then both sibling nodes will get moved, and nothing
// needs to be done. Similarly if the previous sibling is being moved by itself
// then nothing needs to be done. However, if the next sibling is being moved
// by itself, that won't trigger any root changes, so we mark the PartRoot
// dirty in this case.
void Part::PartDisconnected(Node& node) {
  if (!root()) {
    return;
  }
  if (&node == NodeToSortBy()) {
    // If this part's root is the DocumentPartRoot, then disconnect it.
    if (root()->IsDocumentPartRoot()) {
      MoveToRoot(nullptr);
    }
  } else if (NodeMoveScope::CurrentNodeBeingRemoved() == &node) {
    // This is the case when a ChildNodePart is having its `next_sibling` node
    // moved directly. This must dirty the PartRoot since we're not also moving
    // the `previous_sibling` node.
    root()->MarkPartsDirty();
  }
}

// When connecting a Node/Part, if this isn't the main Node for the Part, do
// nothing. Similarly, if there's already a root(), do nothing. If there isn't
// a root(), then we were disconnected from our previous DocumentPartRoot, so
// we need to locate the new one and connect to it. If we're in a NodeMoveScope,
// then it will have the root container, otherwise we use the slow TreeRoot()
// walk.
void Part::PartConnected(Node& node, ContainerNode& insertion_point) {
  if (node != NodeToSortBy() || root()) {
    return;
  }
  Node* root_container = NodeMoveScope::GetDestinationTreeRoot();
  if (root_container) {
    DCHECK_EQ(root_container, &insertion_point.TreeRoot());
  } else {
    // If we're not in a NodeMoveScope, we'll need to potentially walk the
    // parent tree to find the TreeRoot, which can be slow.
    root_container = &insertion_point.TreeRoot();
  }
  PartRoot* new_root;
  if (auto* document_fragment = DynamicTo<DocumentFragment>(root_container)) {
    new_root = &document_fragment->getPartRoot();
  } else if (auto* document = DynamicTo<Document>(root_container)) {
    new_root = &document->getPartRoot();
  } else {
    // insertion_point is not located in a Document or DocumentFragment.
    new_root = nullptr;
  }
  MoveToRoot(new_root);
}

void Part::MoveToRoot(PartRoot* new_root) {
  if (root_) {
    root_->RemovePart(*this);
  }
  root_ = new_root;
  if (new_root) {
    new_root->AddPart(*this);
  }
}

void Part::Trace(Visitor* visitor) const {
  visitor->Trace(root_);
  ScriptWrappable::Trace(visitor);
}

void Part::disconnect() {
  CHECK(!disconnected_) << "disconnect should be overridden";
  if (root_) {
    root_->MarkPartsDirty();
    root_ = nullptr;
  }
  disconnected_ = true;
}

PartRootUnion* Part::rootForBindings() const {
  return PartRoot::GetUnionFromPartRoot(root_);
}

}  // namespace blink

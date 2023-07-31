// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/node_part.h"

#include "third_party/blink/renderer/core/dom/child_node_part.h"
#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/core/dom/part_root.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"

namespace blink {

// static
NodePart* NodePart::Create(PartRootUnion* root_union,
                           Node* node,
                           const PartInit* init,
                           ExceptionState& exception_state) {
  return MakeGarbageCollected<NodePart>(
      *PartRoot::GetPartRootFromUnion(root_union), *node, init);
}

NodePart::NodePart(PartRoot& root, Node& node, const Vector<String> metadata)
    : Part(root, metadata), node_(node) {
  node.AddDOMPart(*this);
}

void NodePart::disconnect() {
  if (disconnected_) {
    CHECK(!node_);
    return;
  }
  if (node_) {
    node_->RemoveDOMPart(*this);
  }
  node_ = nullptr;
  Part::disconnect();
}

void NodePart::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  Part::Trace(visitor);
}

bool NodePart::IsValid() const {
  // A NodePart is valid if the base Part is valid (has a root), and if there
  // is a node reference.
  return Part::IsValid() && node_;
}

Node* NodePart::NodeToSortBy() const {
  return node_;
}

Part* NodePart::ClonePart(NodeCloningData& data) const {
  DCHECK(IsValid());
  PartRoot* new_part_root = data.ClonedPartRootFor(*root());
  // TODO(crbug.com/1453291) Eventually it should *not* be possible to construct
  // Parts that get cloned without their PartRoots. But as-is, that can happen
  // if, for example, a ChildNodePart contains child Nodes that are part of
  // other ChildNodeParts or NodeParts whose `root` is not this ChildNodePart.
  if (!new_part_root) {
    return nullptr;
  }
  Node* new_node = data.ClonedNodeFor(*node_);
  CHECK(new_node);
  return MakeGarbageCollected<NodePart>(*new_part_root, *new_node, metadata());
}

Document& NodePart::GetDocument() const {
  DCHECK(IsValid());
  return node_->GetDocument();
}

}  // namespace blink

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

NodePart::NodePart(PartRoot& root,
                   Node& node,
                   bool add_to_parts_list,
                   const Vector<String> metadata)
    : Part(root, metadata), node_(node) {
  node.AddDOMPart(*this);
  if (add_to_parts_list) {
    root.AddPart(*this);
  }
}

void NodePart::disconnect() {
  if (!IsConnected()) {
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

Node* NodePart::NodeToSortBy() const {
  return node_.Get();
}

Part* NodePart::ClonePart(NodeCloningData& data, Node& node_clone) const {
  DCHECK(IsValid());
  return MakeGarbageCollected<NodePart>(data.CurrentPartRoot(), node_clone,
                                        metadata());
}

Document& NodePart::GetDocument() const {
  DCHECK(IsValid());
  return node_->GetDocument();
}

}  // namespace blink

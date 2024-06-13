// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/node_part.h"

#include "third_party/blink/renderer/core/dom/child_node_part.h"
#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/core/dom/part_root.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// static
NodePart* NodePart::Create(PartRootUnion* root_union,
                           Node* node,
                           const PartInit* init,
                           ExceptionState& exception_state) {
  if (!IsAcceptableNodeType(*node)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidNodeTypeError,
        "The provided node is not a valid node for a NodePart.");
    return nullptr;
  }
  return MakeGarbageCollected<NodePart>(
      *PartRoot::GetPartRootFromUnion(root_union), *node, init);
}

NodePart::NodePart(PartRoot& root,
                   Node& node,
                   Vector<String> metadata)
    : Part(root, std::move(metadata)), node_(node) {
  CHECK(IsAcceptableNodeType(node));
  if (RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled()) {
    node.SetHasNodePart();
  } else {
    node.AddDOMPart(*this);
    root.AddPart(*this);
  }
}

void NodePart::disconnect() {
  if (!IsConnected()) {
    CHECK(!node_);
    return;
  }
  if (node_) {
    if (RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled()) {
      // TODO(crbug.com/40271855): This assumes that each Node has at most one
      // NodePart attached. The consequence of that is that if you
      // (imperatively) construct multiple Parts attached to the same Node,
      // disconnecting one of them will disconnect all of them.
      node_->ClearHasNodePart();
    } else {
      node_->RemoveDOMPart(*this);
    }
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
                                        metadata().AsVector());
}

Document& NodePart::GetDocument() const {
  DCHECK(IsValid());
  return node_->GetDocument();
}

}  // namespace blink

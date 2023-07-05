// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/node_part.h"

namespace blink {

// static
NodePart* NodePart::Create(PartRoot* root,
                           Node* node,
                           const NodePartInit* init,
                           ExceptionState& exception_state) {
  if (!root->SupportsContainedParts()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The provided PartRoot does not support contained parts");
    return nullptr;
  }
  return MakeGarbageCollected<NodePart>(*root, node, init);
}

// TODO(crbug.com/1453291): Handle the init parameter.
NodePart::NodePart(PartRoot& root, Node* node, const NodePartInit* init)
    : Part(root), node_(node) {
  if (node) {
    node->AddDOMPart(*this);
  }
}

void NodePart::disconnect() {
  if (!root()) {
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

bool NodePart::IsValid() {
  // A NodePart is valid if there is a node reference.
  return node_;
}

Node* NodePart::NodeToSortBy() const {
  return node_;
}

Document* NodePart::GetDocument() const {
  return node_ ? &node_->GetDocument() : nullptr;
}

}  // namespace blink

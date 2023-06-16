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

void NodePart::Trace(Visitor* visitor) const {
  visitor->Trace(node_);
  Part::Trace(visitor);
}

bool NodePart::IsValid() {
  // A NodePart is valid if it has a |Node| that is connected.
  return node_ && node_->isConnected();
}

Node* NodePart::RelevantNode() const {
  return node_;
}

Document* NodePart::GetDocument() const {
  return node_ ? &node_->GetDocument() : nullptr;
}

String NodePart::ToString() const {
  return "NodePart for " + (node_ ? node_->ToString() : "nullptr");
}

}  // namespace blink

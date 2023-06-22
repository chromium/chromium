// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/child_node_part.h"

namespace blink {

// static
ChildNodePart* ChildNodePart::Create(PartRoot* root,
                                     Node* previous_sibling,
                                     Node* next_sibling,
                                     const NodePartInit* init,
                                     ExceptionState& exception_state) {
  if (!root->SupportsContainedParts()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "The provided PartRoot does not support contained parts");
    return nullptr;
  }
  return MakeGarbageCollected<ChildNodePart>(*root, *previous_sibling,
                                             *next_sibling, init);
}

// TODO(crbug.com/1453291): Handle the init parameter.
ChildNodePart::ChildNodePart(PartRoot& root,
                             Node& previous_sibling,
                             Node& next_sibling,
                             const NodePartInit* init)
    : Part(root),
      previous_sibling_(previous_sibling),
      next_sibling_(next_sibling) {
  if (previous_sibling.parentNode()) {
    previous_sibling.parentNode()->AddDOMPart(*this);
  }
  previous_sibling.AddDOMPart(*this);
  next_sibling.AddDOMPart(*this);
}

void ChildNodePart::disconnect() {
  if (!root()) {
    CHECK(!previous_sibling_ && !next_sibling_);
    return;
  }
  if (previous_sibling_->parentNode()) {
    previous_sibling_->parentNode()->RemoveDOMPart(*this);
  }
  previous_sibling_->RemoveDOMPart(*this);
  next_sibling_->RemoveDOMPart(*this);
  previous_sibling_ = nullptr;
  next_sibling_ = nullptr;
  Part::disconnect();
}

void ChildNodePart::Trace(Visitor* visitor) const {
  visitor->Trace(previous_sibling_);
  visitor->Trace(next_sibling_);
  Part::Trace(visitor);
}

// A ChildNodePart is valid if:
//  1. previous_sibling_ is connected to the document.
//  2. previous_sibling_ and next_sibling_ have the same (non-null) parent.
//  3. previous_sibling_ does not come after next_sibling_ in the tree.
bool ChildNodePart::IsValid() {
  ContainerNode* parent = previous_sibling_->parentNode();
  if (!parent || !parent->isConnected()) {
    return false;
  }
  if (next_sibling_->parentNode() != parent) {
    return false;
  }
  Node* left = previous_sibling_;
  while (left) {
    if (left == next_sibling_) {
      return true;
    }
    left = left->nextSibling();
  }
  return false;
}

Node* ChildNodePart::NodeToSortBy() const {
  return previous_sibling_->parentNode();
}

Document* ChildNodePart::GetDocument() const {
  return &previous_sibling_->GetDocument();
}

}  // namespace blink

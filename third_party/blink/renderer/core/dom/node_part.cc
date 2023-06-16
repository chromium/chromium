// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/node_part.h"

namespace blink {

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

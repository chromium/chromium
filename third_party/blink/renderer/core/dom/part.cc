// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/part.h"
#include "third_party/blink/renderer/core/dom/child_node_part.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_part_root.h"
#include "third_party/blink/renderer/core/dom/part_root.h"
#include "third_party/blink/renderer/core/dom/tree_scope.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

Part::Part(PartRoot& root, const Vector<String> metadata)
    : root_(root), metadata_(metadata) {
  root.AddPart(*this);
}

void Part::PartDisconnected() {
  if (root()) {
    root()->MarkPartsDirty();
    if (root()->IsDocumentPartRoot()) {
      // If this part's root is the DocumentPartRoot, then disconnect it.
      MoveToRoot(nullptr);
    }
  }
}

void Part::PartConnected(ContainerNode& insertion_point) {
  if (!root()) {
    Node* root_container = &insertion_point.TreeRoot();  // Potentially slow!
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
  if (root()) {
    root()->MarkPartsDirty();
  }
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
    root_->RemovePart(*this);
    root_ = nullptr;
  }
  disconnected_ = true;
}

PartRootUnion* Part::rootForBindings() const {
  return PartRoot::GetUnionFromPartRoot(root_);
}

}  // namespace blink

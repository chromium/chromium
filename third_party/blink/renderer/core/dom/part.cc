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
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

void Part::Trace(Visitor* visitor) const {
  visitor->Trace(root_);
  visitor->Trace(metadata_);
  ScriptWrappable::Trace(visitor);
}

void Part::disconnect() {
  DCHECK(connected_) << "disconnect should be overridden";
  if (root_) {
    if (!RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled()) {
      root_->MarkPartsDirty();
    }
    root_ = nullptr;
  }
  connected_ = false;
  is_valid_ = false;
}

PartRootUnion* Part::rootForBindings() const {
  return PartRoot::GetUnionFromPartRoot(root_.Get());
}

// static
bool Part::IsAcceptableNodeType(Node& node) {
  if (Element* element = DynamicTo<Element>(node)) {
    if (element->IsDocumentElement()) {
      return false;
    }
  }
  auto type = node.getNodeType();
  return type == Node::kElementNode || type == Node::kTextNode ||
         type == Node::kCommentNode;
}

}  // namespace blink

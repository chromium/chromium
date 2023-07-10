// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document_part_root.h"

#include "third_party/blink/renderer/bindings/core/v8/v8_union_document_documentfragment.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

void DocumentPartRoot::Trace(Visitor* visitor) const {
  visitor->Trace(root_container_);
  PartRoot::Trace(visitor);
}

DocumentPartRoot& DocumentPartRoot::clone() const {
  NodeCloningData data{CloneOption::kIncludeDescendants,
                       CloneOption::kPreserveDOMParts};
  Node* clone = GetRootContainer()->Clone(GetDocument(), data);
  if (clone->IsDocumentNode()) {
    return To<Document>(clone)->getPartRoot();
  }
  CHECK(clone->IsDocumentFragment());
  return To<DocumentFragment>(clone)->getPartRoot();
}

ContainerNode& DocumentPartRoot::root() const {
  return *root_container_;
}

}  // namespace blink

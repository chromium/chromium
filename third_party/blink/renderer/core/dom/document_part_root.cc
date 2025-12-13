// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/document_part_root.h"

#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

void DocumentPartRoot::Trace(Visitor* visitor) const {
  visitor->Trace(root_container_);
  ScriptWrappable::Trace(visitor);
  PartRoot::Trace(visitor);
}

PartRootUnion* DocumentPartRoot::clone(ExceptionState& exception_state) {
  NodeCloningData data{CloneOption::kIncludeDescendants,
                       RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled()
                           ? CloneOption::kPreserveDOMPartsMinimalAPI
                           : CloneOption::kPreserveDOMParts};

  Node* clone = rootContainer()->Clone(rootContainer()->GetDocument(), data,
                                       /*append_to*/ nullptr,
                                       /*fallback_registry*/ nullptr);
  if (!clone) {
    // Note we MUST throw if we can't return a non-null value.
    exception_state.ThrowDOMException(DOMExceptionCode::kDataCloneError,
                                      "Failed to clone a DOMPart");
    return nullptr;
  }
  DocumentPartRoot* new_part_root =
      clone->IsDocumentNode() ? &To<Document>(clone)->getPartRoot()
                              : &To<DocumentFragment>(clone)->getPartRoot();
  return PartRoot::GetUnionFromPartRoot(new_part_root);
}

}  // namespace blink

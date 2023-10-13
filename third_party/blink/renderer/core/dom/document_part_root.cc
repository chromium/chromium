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
#include "third_party/blink/renderer/core/dom/node_move_scope.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

void DocumentPartRoot::Trace(Visitor* visitor) const {
  visitor->Trace(root_container_);
  ScriptWrappable::Trace(visitor);
  PartRoot::Trace(visitor);
}

PartRootUnion* DocumentPartRoot::clone(PartRootCloneOptions* options,
                                       ExceptionState&) {
  NodeCloningData data{CloneOption::kIncludeDescendants,
                       CloneOption::kPreserveDOMParts};
  data.SetPartRootCloneOptions(options);
  Node* clone = rootContainer()->Clone(rootContainer()->GetDocument(), data,
                                       /*append_to*/ nullptr);
  // http://crbug.com/1467847: clone may be null and can be hit by clusterfuzz.
  if (!clone) {
    return nullptr;
  }
  DocumentPartRoot* new_part_root =
      clone->IsDocumentNode() ? &To<Document>(clone)->getPartRoot()
                              : &To<DocumentFragment>(clone)->getPartRoot();
  NodeMoveScope node_move_scope(*clone, NodeMoveScopeType::kClone);
  return PartRoot::GetUnionFromPartRoot(new_part_root);
}

}  // namespace blink

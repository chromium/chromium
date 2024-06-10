// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/attribute_part.h"

#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/core/dom/part_root.h"

namespace blink {

// static
AttributePart* AttributePart::Create(PartRootUnion* root_union,
                                     Node* node,
                                     AtomicString local_name,
                                     const PartInit* init,
                                     ExceptionState& exception_state) {
  Element* element = DynamicTo<Element>(node);
  if (!element) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "An AttributePart must be constructed on an Element.");
    return nullptr;
  }
  return MakeGarbageCollected<AttributePart>(
      *PartRoot::GetPartRootFromUnion(root_union), *element, local_name, init);
}

AttributePart::AttributePart(PartRoot& root,
                             Element& element,
                             AtomicString local_name,
                             Vector<String> metadata)
    : NodePart(root, element, std::move(metadata)), local_name_(local_name) {}

Part* AttributePart::ClonePart(NodeCloningData& data, Node& node_clone) const {
  DCHECK(IsValid());
  Element& element_clone = To<Element>(node_clone);
  Part* new_part =
      MakeGarbageCollected<AttributePart>(data.CurrentPartRoot(), element_clone,
                                          local_name_, metadata().AsVector());
  return new_part;
}

}  // namespace blink

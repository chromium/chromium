// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_NODE_PART_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_NODE_PART_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_node_part_init.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_node_string_trustedscript.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/node_part.h"
#include "third_party/blink/renderer/core/dom/part_root.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"

namespace blink {

// Implementation of the ChildNodePart class, which is part of the DOM Parts
// API. A ChildNodePart stores a reference to a range of nodes within the
// children of a single parent |Node| in the DOM tree.
class CORE_EXPORT ChildNodePart : public Part {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ChildNodePart* Create(PartRoot* root,
                               Node* previous_sibling,
                               Node* next_sibling,
                               const NodePartInit* init,
                               ExceptionState& exception_state);
  // TODO(crbug.com/1453291): Handle the init parameter.
  ChildNodePart(PartRoot& root,
                Node& previous_sibling,
                Node& next_sibling,
                const NodePartInit* init)
      : Part(root),
        previous_sibling_(previous_sibling),
        next_sibling_(next_sibling) {}
  ChildNodePart(const ChildNodePart&) = delete;
  ~ChildNodePart() override = default;

  void Trace(Visitor* visitor) const override;
  Node* RelevantNode() const override;
  String ToString() const override;
  bool SupportsContainedParts() const override { return true; }

  // ChildNodePart API
  Node* previousSibling() const { return previous_sibling_; }
  Node* nextSibling() const { return next_sibling_; }
  // TODO(crbug.com/1453291) Implement this method.
  HeapVector<Member<Node>> children() const {
    return HeapVector<Member<Node>>();
  }
  // TODO(crbug.com/1453291) Implement this method.
  void replaceChildren(const HeapVector<Member<V8UnionNodeOrString>>& nodes) {}

 protected:
  bool IsValid() override;
  Document* GetDocument() const override;

 private:
  // Checks if this ChildNodePart is valid. This should only be called if the
  // cached validity is dirty.
  bool CheckValidity();

  Member<Node> previous_sibling_;
  Member<Node> next_sibling_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_NODE_PART_H_

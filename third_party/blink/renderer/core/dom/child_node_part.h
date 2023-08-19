// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_NODE_PART_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_NODE_PART_H_

#include "third_party/blink/renderer/bindings/core/v8/v8_part_init.h"
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
class CORE_EXPORT ChildNodePart : public Part, public PartRoot {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static ChildNodePart* Create(PartRootUnion* root_union,
                               Node* previous_sibling,
                               Node* next_sibling,
                               const PartInit* init,
                               ExceptionState& exception_state);
  ChildNodePart(PartRoot& root,
                Node& previous_sibling,
                Node& next_sibling,
                const PartInit* init)
      : ChildNodePart(root,
                      previous_sibling,
                      next_sibling,
                      init && init->hasMetadata() ? init->metadata()
                                                  : Vector<String>()) {}
  ChildNodePart(PartRoot& root,
                Node& previous_sibling,
                Node& next_sibling,
                const Vector<String> metadata);
  ChildNodePart(const ChildNodePart&) = delete;
  ~ChildNodePart() override = default;

  void Trace(Visitor* visitor) const override;
  bool IsValid() const override;
  Node* NodeToSortBy() const override;
  Part* ClonePart(NodeCloningData&) const override;
  PartRoot* GetAsPartRoot() const override {
    return const_cast<ChildNodePart*>(this);
  }

  Document& GetDocument() const override;
  bool IsDocumentPartRoot() const override { return false; }
  Node* FirstIncludedChildNode() const override { return previous_sibling_; }
  Node* LastIncludedChildNode() const override { return next_sibling_; }

  // ChildNodePart API
  void disconnect() override;
  PartRootUnion* clone(ExceptionState& exception_state);
  ContainerNode* rootContainer() const override;
  ContainerNode* parentNode() const { return previous_sibling_->parentNode(); }
  Node* previousSibling() const { return previous_sibling_; }
  Node* nextSibling() const { return next_sibling_; }
  void setNextSibling(Node& next_sibling);
  HeapVector<Member<Node>> children() const;
  void replaceChildren(
      const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& nodes,
      ExceptionState& exception_state);

 protected:
  const PartRoot* GetParentPartRoot() const override { return root(); }

 private:
  // Checks if this ChildNodePart is valid. This should only be called if the
  // cached validity is dirty.
  bool CheckValidity();

  Member<Node> previous_sibling_;
  Member<Node> next_sibling_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_CHILD_NODE_PART_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/child_node_part.h"

#include "third_party/blink/renderer/core/dom/container_node.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_part_root.h"
#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// static
ChildNodePart* ChildNodePart::Create(PartRootUnion* root_union,
                                     Node* previous_sibling,
                                     Node* next_sibling,
                                     const PartInit* init,
                                     ExceptionState& exception_state) {
  if (!IsAcceptableNodeType(*previous_sibling) ||
      !IsAcceptableNodeType(*next_sibling)) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidNodeTypeError,
        "The provided previous_sibling and next_sibling nodes are not valid "
        "for a ChildNodePart.");
    return nullptr;
  }
  return MakeGarbageCollected<ChildNodePart>(*GetPartRootFromUnion(root_union),
                                             *previous_sibling, *next_sibling,
                                             init);
}

ChildNodePart::ChildNodePart(PartRoot& root,
                             Node& previous_sibling,
                             Node& next_sibling,
                             Vector<String> metadata)
    : Part(root, std::move(metadata)),
      previous_sibling_(previous_sibling),
      next_sibling_(next_sibling) {
  CHECK(IsAcceptableNodeType(previous_sibling));
  CHECK(IsAcceptableNodeType(next_sibling));
  if (RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled()) {
    previous_sibling.SetHasNodePart();
    next_sibling.SetHasNodePart();
  } else {
    previous_sibling.AddDOMPart(*this);
    if (previous_sibling != next_sibling) {
      next_sibling.AddDOMPart(*this);
    }
    root.AddPart(*this);
  }
}

void ChildNodePart::disconnect() {
  if (!IsConnected()) {
    CHECK(!previous_sibling_ && !next_sibling_);
    return;
  }
  if (RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled()) {
    // TODO(crbug.com/40271855): This assumes the endpoint nodes have exactly
    // one NodePart/ChildNodePart attached. The consequence of that is that if
    // you (imperatively) construct multiple Parts attached to the same Nodes,
    // disconnecting one of them will disconnect all of them.
    previous_sibling_->ClearHasNodePart();
    next_sibling_->ClearHasNodePart();
  } else {
    previous_sibling_->RemoveDOMPart(*this);
    if (next_sibling_ != previous_sibling_) {
      next_sibling_->RemoveDOMPart(*this);
    }
  }
  previous_sibling_ = nullptr;
  next_sibling_ = nullptr;
  Part::disconnect();
}

PartRootUnion* ChildNodePart::clone(ExceptionState& exception_state) {
  // Since we're only cloning a part of the tree, not including this
  // ChildNodePart's `root`, we use a temporary DocumentFragment and its
  // PartRoot during the clone.
  DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
  if (!IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "This ChildNodePart is not in a valid state. It must have "
        "previous_sibling before next_sibling, and both with the same parent.");
    return nullptr;
  }
  auto& document = GetDocument();
  auto* fragment = DocumentFragment::Create(document);
  NodeCloningData data{RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled()
                           ? CloneOption::kPreserveDOMPartsMinimalAPI
                           : CloneOption::kPreserveDOMParts};
  auto& fragment_part_root = fragment->getPartRoot();
  data.PushPartRoot(fragment_part_root);
  ContainerNode* new_parent = To<ContainerNode>(
      parentNode()->Clone(document, data, fragment, exception_state));
  if (exception_state.HadException()) {
    return nullptr;
  }
  data.Put(CloneOption::kIncludeDescendants);
  Node* node = previous_sibling_;
  ChildNodePart* part_root = nullptr;
  while (true) {
    bool final_node = node == next_sibling_;
    if (final_node) {
      part_root = static_cast<ChildNodePart*>(&data.CurrentPartRoot());
    }
    node->Clone(document, data, new_parent, exception_state);
    if (exception_state.HadException()) {
      return nullptr;
    }
    if (final_node) {
      break;
    }
    node = node->nextSibling();
    CHECK(node) << "IsValid should detect invalid siblings";
  }
  DCHECK_EQ(&data.CurrentPartRoot(), &fragment_part_root);
  return PartRoot::GetUnionFromPartRoot(part_root);
}

void ChildNodePart::setNextSibling(Node& next_sibling) {
  DCHECK(!RuntimeEnabledFeatures::DOMPartsAPIMinimalEnabled());
  if (next_sibling_ == &next_sibling) {
    return;
  }
  if (previous_sibling_ != next_sibling_) {
    // Unregister this part from the old |next_sibling_| node, unless previous
    // and next were the same before.
    if (next_sibling_ != parentNode()) {
      // TODO(crbug.com/40271855) It is currently possible to build
      // ChildNodeParts with `next_sibling === parentNode`. Eventually,
      // outlaw that in the appropriate place, and CHECK() here that it isn't
      // true. For now, in that case, don't remove the part.
      next_sibling_->RemoveDOMPart(*this);
    }
  }
  next_sibling.AddDOMPart(*this);
  next_sibling_ = &next_sibling;
}

HeapVector<Member<Node>> ChildNodePart::children() const {
  HeapVector<Member<Node>> child_list;
  Node* node = previous_sibling_->nextSibling();
  while (node && node != next_sibling_) {
    child_list.push_back(node);
    node = node->nextSibling();
  }
  if (!node) {
    // Invalid part.
    child_list.clear();
  }
  return child_list;
}

void ChildNodePart::replaceChildren(
    const HeapVector<Member<V8UnionNodeOrStringOrTrustedScript>>& nodes,
    ExceptionState& exception_state) {
  if (!IsValid()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        "This ChildNodePart is not in a valid state. It must have "
        "previous_sibling before next_sibling, and both with the same parent.");
    return;
  }
  ContainerNode* parent = parentNode();
  DCHECK(parent) << "Should be guaranteed by IsValid";
  // Remove existing children, leaving endpoints.
  Node* node = previous_sibling_->nextSibling();
  while (node != next_sibling_) {
    Node* to_remove = node;
    node = node->nextSibling();
    parent->RemoveChild(to_remove, exception_state);
    if (exception_state.HadException()) {
      return;
    }
  }
  // Insert new contents.
  Node* nodes_as_node = Node::ConvertNodeUnionsIntoNode(
      parent, nodes, parent->GetDocument(), "replaceChildren", exception_state);
  if (exception_state.HadException()) {
    return;
  }
  parent->InsertBefore(nodes_as_node, next_sibling_, exception_state);
}

void ChildNodePart::Trace(Visitor* visitor) const {
  visitor->Trace(previous_sibling_);
  visitor->Trace(next_sibling_);
  PartRoot::Trace(visitor);
  Part::Trace(visitor);
}

Node* ChildNodePart::NodeToSortBy() const {
  return previous_sibling_.Get();
}

ContainerNode* ChildNodePart::rootContainer() const {
  return IsValid() ? parentNode() : nullptr;
}

Part* ChildNodePart::ClonePart(NodeCloningData& data, Node& node_clone) const {
  DCHECK(IsValid());
  ChildNodePart* clone = MakeGarbageCollected<ChildNodePart>(
      data.CurrentPartRoot(), node_clone, node_clone, metadata().AsVector());
  data.PushPartRoot(*clone);
  return clone;
}

Document& ChildNodePart::GetDocument() const {
  DCHECK(IsValid());
  return previous_sibling_->GetDocument();
}

}  // namespace blink

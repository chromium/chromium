// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/child_node_part.h"

#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/document_part_root.h"
#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/core/dom/node_move_scope.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"

namespace blink {

// static
ChildNodePart* ChildNodePart::Create(PartRootUnion* root_union,
                                     Node* previous_sibling,
                                     Node* next_sibling,
                                     const PartInit* init,
                                     ExceptionState& exception_state) {
  return MakeGarbageCollected<ChildNodePart>(*GetPartRootFromUnion(root_union),
                                             *previous_sibling, *next_sibling,
                                             init);
}

ChildNodePart::ChildNodePart(PartRoot& root,
                             Node& previous_sibling,
                             Node& next_sibling,
                             const Vector<String> metadata)
    : Part(root, metadata),
      previous_sibling_(previous_sibling),
      next_sibling_(next_sibling) {
  previous_sibling.AddDOMPart(*this);
  if (previous_sibling != next_sibling) {
    next_sibling.AddDOMPart(*this);
  }
}

void ChildNodePart::disconnect() {
  if (disconnected_) {
    CHECK(!previous_sibling_ && !next_sibling_);
    return;
  }
  previous_sibling_->RemoveDOMPart(*this);
  if (next_sibling_ != previous_sibling_) {
    next_sibling_->RemoveDOMPart(*this);
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
  auto* fragment = To<DocumentFragment>(DocumentFragment::Create(document));
  NodeCloningData data{CloneOption::kPreserveDOMParts};
  data.ConnectPartRootToClone(*root(), fragment->getPartRoot());
  ContainerNode* new_parent = To<ContainerNode>(
      parentNode()->Clone(document, data, fragment, exception_state));
  if (exception_state.HadException()) {
    return nullptr;
  }
  data.Put(CloneOption::kIncludeDescendants);
  Node* node = previous_sibling_;
  while (true) {
    node->Clone(document, data, new_parent, exception_state);
    if (exception_state.HadException()) {
      return nullptr;
    }
    if (node == next_sibling_) {
      break;
    }
    node = node->nextSibling();
    CHECK(node) << "IsValid should detect invalid siblings";
  }
  NodeMoveScope node_move_scope(*new_parent, NodeMoveScopeType::kClone);
  data.Finalize();
  ChildNodePart* part_root =
      static_cast<ChildNodePart*>(data.ClonedPartRootFor(*this));
  return PartRoot::GetUnionFromPartRoot(part_root);
}

void ChildNodePart::setNextSibling(Node& next_sibling) {
  if (next_sibling_ == &next_sibling) {
    return;
  }
  if (previous_sibling_ != next_sibling_) {
    // Unregister this part from the old |next_sibling_| node, unless previous
    // and next were the same before.
    if (next_sibling_ != parentNode()) {
      // TODO(crbug.com/1453291) It is currently possible to build
      // ChildNodeParts with `next_sibling === parentNode`. Eventually,
      // outlaw that in the appropriate place, and CHECK() here that it isn't
      // true. For now, in that case, don't remove the part.
      next_sibling_->RemoveDOMPart(*this);
    }
  }
  next_sibling_ = &next_sibling;
  next_sibling.AddDOMPart(*this);
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
  // Remove existing children, leaving endpoints.
  Node* node = previous_sibling_->nextSibling();
  while (node != next_sibling_) {
    Node* remove = node;
    node = node->nextSibling();
    remove->remove();
  }
  // Insert new contents.
  next_sibling_->before(nodes, exception_state);
}

void ChildNodePart::Trace(Visitor* visitor) const {
  visitor->Trace(previous_sibling_);
  visitor->Trace(next_sibling_);
  PartRoot::Trace(visitor);
  Part::Trace(visitor);
}

// A ChildNodePart is valid if:
//  1. The base |Part| is valid (it has a |root|).
//  2. previous_sibling_ and next_sibling_ are non-null.
//  3. previous_sibling_ and next_sibling_ have the same (non-null) parent.
//  4. previous_sibling_ comes strictly before next_sibling_ in the tree.
bool ChildNodePart::IsValid() const {
  if (!Part::IsValid()) {
    return false;
  }
  if (!previous_sibling_ || !next_sibling_) {
    return false;
  }
  ContainerNode* parent = parentNode();
  if (!parent) {
    return false;
  }
  if (next_sibling_->parentNode() != parent) {
    return false;
  }
  if (previous_sibling_ == next_sibling_) {
    return false;
  }
  Node* left = previous_sibling_;
  do {
    left = left->nextSibling();
    if (left == next_sibling_) {
      return true;
    }
  } while (left);
  return false;
}

Node* ChildNodePart::NodeToSortBy() const {
  return previous_sibling_;
}

ContainerNode* ChildNodePart::rootContainer() const {
  return IsValid() ? parentNode() : nullptr;
}

Part* ChildNodePart::ClonePart(NodeCloningData& data) const {
  DCHECK(IsValid());
  PartRoot* new_part_root = data.ClonedPartRootFor(*root());
  // TODO(crbug.com/1453291) Eventually it should *not* be possible to construct
  // Parts that get cloned without their PartRoots. But as-is, that can happen
  // if, for example, a ChildNodePart contains child Nodes that are part of
  // other ChildNodeParts or NodeParts whose `root` is not this ChildNodePart.
  if (!new_part_root) {
    return nullptr;
  }
  Node* new_previous = data.ClonedNodeFor(*previous_sibling_);
  Node* new_next = data.ClonedNodeFor(*next_sibling_);
  CHECK(new_previous && new_next);
  ChildNodePart* clone = MakeGarbageCollected<ChildNodePart>(
      *new_part_root, *new_previous, *new_next, metadata());
  data.ConnectPartRootToClone(*this, *clone);
  return clone;
}

Document& ChildNodePart::GetDocument() const {
  DCHECK(IsValid());
  return previous_sibling_->GetDocument();
}

}  // namespace blink

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/child_node_part.h"

#include "third_party/blink/renderer/core/dom/node_cloning_data.h"

namespace blink {

// static
ChildNodePart* ChildNodePart::Create(PartRootUnion* root_union,
                                     Node* previous_sibling,
                                     Node* next_sibling,
                                     const NodePartInit* init,
                                     ExceptionState& exception_state) {
  return MakeGarbageCollected<ChildNodePart>(*GetPartRootFromUnion(root_union),
                                             *previous_sibling, *next_sibling,
                                             init);
}

// TODO(crbug.com/1453291): Handle the init parameter.
ChildNodePart::ChildNodePart(PartRoot& root,
                             Node& previous_sibling,
                             Node& next_sibling,
                             const NodePartInit* init)
    : Part(root),
      previous_sibling_(previous_sibling),
      next_sibling_(next_sibling) {
  if (previous_sibling.parentNode()) {
    previous_sibling.parentNode()->AddDOMPart(*this);
  }
  previous_sibling.AddDOMPart(*this);
  next_sibling.AddDOMPart(*this);
}

void ChildNodePart::disconnect() {
  if (!root()) {
    CHECK(!previous_sibling_ && !next_sibling_);
    return;
  }
  if (previous_sibling_->parentNode()) {
    previous_sibling_->parentNode()->RemoveDOMPart(*this);
  }
  previous_sibling_->RemoveDOMPart(*this);
  next_sibling_->RemoveDOMPart(*this);
  previous_sibling_ = nullptr;
  next_sibling_ = nullptr;
  Part::disconnect();
}

PartRootUnion* ChildNodePart::clone() const {
  // TODO(crbug.com/1453291) Implement ChildNodePart cloning.
  // NodeCloningData data{CloneOption::kIncludeChildren,
  //                      CloneOption::kPreserveDOMParts};
  // Node* clone = rootContainer()->Clone(GetDocument(), data);
  return nullptr;
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
//  4. previous_sibling_ does not come after next_sibling_ in the tree.
bool ChildNodePart::IsValid() const {
  if (!Part::IsValid()) {
    return false;
  }
  if (!previous_sibling_ || !next_sibling_) {
    return false;
  }
  ContainerNode* parent = previous_sibling_->parentNode();
  if (!parent) {
    return false;
  }
  if (next_sibling_->parentNode() != parent) {
    return false;
  }
  Node* left = previous_sibling_;
  while (left) {
    if (left == next_sibling_) {
      return true;
    }
    left = left->nextSibling();
  }
  return false;
}

Node* ChildNodePart::NodeToSortBy() const {
  return previous_sibling_->parentNode();
}

ContainerNode* ChildNodePart::rootContainer() const {
  CHECK(IsValid());
  return previous_sibling_->parentNode();
}

void ChildNodePart::Clone(NodeCloningData& data) const {
  CHECK(IsValid());
  PartRoot* new_part_root = data.ClonedPartRootFor(*root());
  Node* new_previous = data.ClonedNodeFor(*previous_sibling_);
  Node* new_next = data.ClonedNodeFor(*next_sibling_);
  CHECK(new_part_root && new_previous && new_next);
  data.ConnectPartRootToClone(
      *this, *MakeGarbageCollected<ChildNodePart>(*new_part_root, *new_previous,
                                                  *new_next));
}

Document& ChildNodePart::GetDocument() const {
  CHECK(IsValid());
  return previous_sibling_->GetDocument();
}

}  // namespace blink

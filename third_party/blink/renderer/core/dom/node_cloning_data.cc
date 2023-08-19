// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/node_cloning_data.h"
#include "third_party/blink/renderer/core/dom/node_move_scope.h"

namespace blink {

NodeCloningData::~NodeCloningData() {
  Finalize();
}

void NodeCloningData::Finalize() {
  if (!Has(CloneOption::kPreserveDOMParts) || finalized_) {
    return;
  }
  DCHECK(RuntimeEnabledFeatures::DOMPartsAPIEnabled());
  DCHECK(NodeMoveScope::InScope() || part_queue_.empty());
  for (auto part : part_queue_) {
    if (!part->IsValid()) {
      // Only valid parts are cloned.
      continue;
    }
    CHECK(part->root());
    part->ClonePart(*this);
  }
  finalized_ = true;
}

void NodeCloningData::ConnectNodeToClone(const Node& node, Node& clone) {
  CHECK(!finalized_);
  DCHECK(!cloned_node_map_.Contains(&node));
  cloned_node_map_.Set(&node, clone);
}

Node* NodeCloningData::ClonedNodeFor(const Node& node) const {
  auto it = cloned_node_map_.find(&node);
  if (it == cloned_node_map_.end()) {
    return nullptr;
  }
  return it->value;
}

void NodeCloningData::ConnectPartRootToClone(const PartRoot& part_root,
                                             PartRoot& clone) {
  CHECK(!finalized_);
  DCHECK(!cloned_part_root_map_.Contains(&part_root) ||
         cloned_part_root_map_.at(&part_root) == &clone);
  cloned_part_root_map_.Set(&part_root, clone);
}

PartRoot* NodeCloningData::ClonedPartRootFor(const PartRoot& part_root) const {
  auto it = cloned_part_root_map_.find(&part_root);
  if (it == cloned_part_root_map_.end()) {
    return nullptr;
  }
  return it->value;
}

void NodeCloningData::QueueForCloning(const Part& to_clone) {
  CHECK(!finalized_);
  DCHECK(!part_queue_.Contains(&to_clone));
  part_queue_.push_back(&to_clone);
}

}  // namespace blink

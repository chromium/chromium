// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/aom/accessible_node_list.h"

#include "third_party/blink/renderer/core/aom/accessible_node.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"

namespace blink {

// The spec doesn't give a limit, but there's no reason to allow relations
// between an arbitrarily large number of other accessible nodes.
static const unsigned kMaxItems = 65536;

// static
AccessibleNodeList* AccessibleNodeList::Create(
    const HeapVector<Member<AccessibleNode>>& nodes) {
  AccessibleNodeList* result = MakeGarbageCollected<AccessibleNodeList>();
  result->nodes_ = nodes;
  return result;
}

AccessibleNodeList::AccessibleNodeList() {
  DCHECK(RuntimeEnabledFeatures::AccessibilityObjectModelEnabled());
}

AccessibleNodeList::~AccessibleNodeList() = default;

void AccessibleNodeList::AddOwner(AOMRelationListProperty property,
                                  AccessibleNode* node) {
  owners_.push_back(std::make_pair(property, node));
}

void AccessibleNodeList::RemoveOwner(AOMRelationListProperty property,
                                     AccessibleNode* node) {
  for (wtf_size_t i = 0; i < owners_.size(); ++i) {
    auto& item = owners_[i];
    if (item.first == property && item.second == node) {
      owners_.EraseAt(i);
      return;
    }
  }
}

AccessibleNode* AccessibleNodeList::item(unsigned offset) const {
  if (offset < nodes_.size())
    return nodes_[offset];
  return nullptr;
}

void AccessibleNodeList::add(AccessibleNode* node, AccessibleNode* before) {
  if (nodes_.size() == kMaxItems)
    return;

  wtf_size_t index = nodes_.size();
  if (before) {
    for (index = 0; index < nodes_.size(); ++index) {
      if (nodes_[index] == before)
        break;
    }
    if (index == nodes_.size())
      return;
  }

  nodes_.insert(index, node);
}

void AccessibleNodeList::remove(int index) {
  if (index >= 0 && static_cast<wtf_size_t>(index) < nodes_.size())
    nodes_.EraseAt(index);
}

IndexedPropertySetterResult AccessibleNodeList::AnonymousIndexedSetter(
    unsigned index,
    AccessibleNode* node,
    ExceptionState& state) {
  if (!node) {
    remove(index);
    return IndexedPropertySetterResult::kIntercepted;
  }
  if (index >= kMaxItems)
    return IndexedPropertySetterResult::kDidNotIntercept;
  if (index >= nodes_.size()) {
    wtf_size_t old_size = nodes_.size();
    nodes_.resize(index + 1);
    for (wtf_size_t i = old_size; i < nodes_.size(); ++i)
      nodes_[i] = nullptr;
  }
  nodes_[index] = node;
  return IndexedPropertySetterResult::kIntercepted;
}

unsigned AccessibleNodeList::length() const {
  return nodes_.size();
}

void AccessibleNodeList::setLength(unsigned new_length) {
  if (new_length >= kMaxItems)
    return;
  nodes_.resize(new_length);
}

void AccessibleNodeList::NotifyChanged() {
  for (auto& owner : owners_)
    owner.second->OnRelationListChanged(owner.first);
}

void AccessibleNodeList::Trace(Visitor* visitor) const {
  visitor->Trace(nodes_);
  visitor->Trace(owners_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink

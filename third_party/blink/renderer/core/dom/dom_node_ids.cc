// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "third_party/blink/renderer/core/dom/node-inl.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

namespace {

DOMNodeId g_last_id = 0;

// See WeakIdentifierMap::Next().
DOMNodeId NextId() {
  if (g_last_id == std::numeric_limits<DOMNodeId>::max()) [[unlikely]] {
    g_last_id = 0;
  }
  return ++g_last_id;
}

GCedHeapHashMap<DOMNodeId, WeakMember<Node>>& IdToNodeMap() {
  DCHECK(IsMainThread());
  using RefType = GCedHeapHashMap<DOMNodeId, WeakMember<Node>>;
  static base::NoDestructor<Persistent<RefType>> map_instance(
      MakeGarbageCollected<RefType>());
  return **map_instance;
}

}  // namespace

// static
DOMNodeId DOMNodeIds::ExistingIdForNode(Node* node) {
  return node ? node->NodeID(base::PassKey<DOMNodeIds>()) : kInvalidDOMNodeId;
}

// static
DOMNodeId DOMNodeIds::ExistingIdForNode(const Node* node) {
  return ExistingIdForNode(const_cast<Node*>(node));
}

// static
DOMNodeId DOMNodeIds::IdForNode(Node* node) {
  if (!node) {
    return kInvalidDOMNodeId;
  }

  DOMNodeId& id = node->EnsureNodeID(base::PassKey<DOMNodeIds>());
  if (id == kInvalidDOMNodeId) {
    id = NextId();
    while (!IdToNodeMap().insert(id, node).is_new_entry) [[unlikely]] {
      id = NextId();
    }
  }
  return id;
}

// static
void DOMNodeIds::SetLastIdForTesting(DOMNodeId id) {
  g_last_id = id;
}

// static
Node* DOMNodeIds::NodeForId(DOMNodeId id) {
  if (id == kInvalidDOMNodeId) {
    return nullptr;
  }
  auto it = IdToNodeMap().find(id);
  if (it == IdToNodeMap().end()) {
    return nullptr;
  }
  return it->value.Get();
}

}  // namespace blink

// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/dom_node_ids.h"

#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"

namespace blink {

static GCedHeapHashMap<DOMNodeId, WeakMember<Node>>& IdToNodeMap() {
  using RefType = GCedHeapHashMap<DOMNodeId, WeakMember<Node>>;
  DEFINE_STATIC_LOCAL(Persistent<RefType>, map_instance,
                      (MakeGarbageCollected<RefType>()));
  return *map_instance;
}

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
    // See WeakIdentifierMap::Next().
    static DOMNodeId last_id = 0;
    if (last_id == std::numeric_limits<DOMNodeId>::max()) [[unlikely]] {
      last_id = 0;
    }
    id = ++last_id;
    IdToNodeMap().Set(id, node);
  }
  return id;
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

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

// IdToNodeMap is a mapping of DOM node ID (an integer) to the Node that has
// been assigned that ID. When accessibility or something else that needs IDs
// is on, there is very heavy traffic to this map, which is why we use a
// vector instead of a HashMap. (The vector never shrinks.)
//
// Freeing is handled through a simple single-linked freelist, where one global
// ID points to the first unused ID. When we note that a Node with an ID
// no longer is alive (through a custom weakness callback), we clear its
// corresponding node pointer in the vector, point the unused index at it,
// and then populate next_free_id with the previous value of the unused index.
// We never shrink the vector, which is suboptimal but keeps things simple.
//
// Note that these members are untraced; it is the custom weakness callback that
// is responsible for freeing the slot when the Node no longer is in use.
// This saves on GC time, as this map can get pretty big (>100k elements) before
// a GC actually happens.
//
// Indexing into the vector is off-by-one, in that the first valid ID is 1
// (because 0 is kInvalidDOMNodeId) and that goes into the first slot of the
// array. So e.g. ID 1000 goes into map_[999].
class IdToNodeMap : public GarbageCollected<IdToNodeMap> {
 public:
  static IdToNodeMap& Instance() {
    DEFINE_STATIC_LOCAL(Persistent<IdToNodeMap>, map_instance,
                        (MakeGarbageCollected<IdToNodeMap>()));
    return *map_instance;
  }

  DOMNodeId AllocateID(Node* node);
  Node* NodeForId(DOMNodeId id) const;

  void Trace(Visitor* visitor) const {
    visitor->Trace(map_);
    visitor->RegisterWeakCallbackMethod<IdToNodeMap,
                                        &IdToNodeMap::CleanDOMNodeIds>(this);
  }

  void CleanDOMNodeIds(const LivenessBroker& info);

 private:
  struct DOMNodeIdSlot {
    // Only one of these can be active at the same time. If next_free_id == 0,
    // then node is valid and there's a node with the ID corresponding to
    // this slot in the vector (plus one, as described above). However, we
    // cannot use a union, because NodeForId() is allowed to ask for any valid
    // or invalid ID and we wouldn't know which is which just by looking at the
    // node. (We could in theory solve this with a separate bitset or similar.)
    DOMNodeId next_free_id;
    UntracedMember<Node> node;
  };
  HeapVector<DOMNodeIdSlot> map_;
  DOMNodeId first_free_id_ = 0;
};

DOMNodeId IdToNodeMap::AllocateID(Node* node) {
  if (first_free_id_ == 0) {
    // No unused slots in the map, so we need to add a new one.
    map_.emplace_back(0, node);
    return map_.size();  // NOTE: Starts from 1.
  } else {
    // Use the first unused slot, then re-point first_free_id_
    // to the next one in the linked list.
    DOMNodeId next_free_id = map_[first_free_id_ - 1].next_free_id;
    map_[first_free_id_ - 1].next_free_id = 0;
    map_[first_free_id_ - 1].node = node;
    DOMNodeId id = first_free_id_;
    first_free_id_ = next_free_id;
    if (first_free_id_ != 0) {
      // We're highly likely to want this pointer soon
      // (for the next created node), so we can just as well
      // make sure it's in the cache; this function has
      // a lot of cache misses.
      __builtin_prefetch(&map_[first_free_id_ - 1]);
    }
    return id;
  }
}

Node* IdToNodeMap::NodeForId(DOMNodeId id) const {
  if (id <= 0 || static_cast<size_t>(id - 1) >= map_.size() ||
      map_[id - 1].next_free_id != 0) {
    // ID out of range, or it points to a node that no longer exists,
    // or it points to a node that was at some point freed.
    return nullptr;
  }

  // NOTE: If the browser received an ID for a node, the node was destroyed
  // and then later its ID reused, we'll return the wrong node here.
  // This is something the client will need to handle non-catastrophically.
  return map_[id - 1].node;
}

void IdToNodeMap::CleanDOMNodeIds(const LivenessBroker& info) {
  for (unsigned i = 0; i < map_.size(); ++i) {
    if (map_[i].node && !info.IsHeapObjectAlive(map_[i].node)) {
      map_[i].next_free_id = first_free_id_;
      map_[i].node = nullptr;
      first_free_id_ = i + 1;
    }
  }
}

// static
DOMNodeId DOMNodeIds::ExistingIdForNode(Node* node) {
  return node ? node->NodeID() : kInvalidDOMNodeId;
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

  DOMNodeId& id = node->EnsureNodeID();
  if (id == kInvalidDOMNodeId) {
    id = IdToNodeMap::Instance().AllocateID(node);
  }
  return id;
}

// static
Node* DOMNodeIds::NodeForId(DOMNodeId id) {
  return IdToNodeMap::Instance().NodeForId(id);
}

}  // namespace blink

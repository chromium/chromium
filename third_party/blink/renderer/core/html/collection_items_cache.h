/*
 * Copyright (C) 2012,2013 Google Inc. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_COLLECTION_ITEMS_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_COLLECTION_ITEMS_CACHE_H_

#include "third_party/blink/renderer/core/dom/collection_index_cache.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

template <typename Collection, typename NodeType>
class CollectionItemsCache {
  DISALLOW_NEW();

 public:
  CollectionItemsCache();
  ~CollectionItemsCache();

  void Trace(Visitor* visitor) const {
    visitor->Trace(index_cache_);
    visitor->Trace(cached_list_);
  }

  bool IsEmpty(const Collection& collection) {
    return index_cache_.IsEmpty(collection);
  }
  bool HasExactlyOneNode(const Collection& collection) {
    return index_cache_.HasExactlyOneNode(collection);
  }
  unsigned NodeCount(const Collection&);
  NodeType* NodeAt(const Collection&, unsigned index);
  void Invalidate();

 private:
  CollectionIndexCache<Collection, NodeType> index_cache_;
  bool list_valid_;
  HeapVector<Member<NodeType>> cached_list_;
};

template <typename Collection, typename NodeType>
CollectionItemsCache<Collection, NodeType>::CollectionItemsCache()
    : list_valid_(false) {}

template <typename Collection, typename NodeType>
CollectionItemsCache<Collection, NodeType>::~CollectionItemsCache() = default;

template <typename Collection, typename NodeType>
void CollectionItemsCache<Collection, NodeType>::Invalidate() {
  index_cache_.Invalidate();
  if (list_valid_) {
    cached_list_.Shrink(0);
    list_valid_ = false;
  }
}

template <class Collection, class NodeType>
unsigned CollectionItemsCache<Collection, NodeType>::NodeCount(
    const Collection& collection) {
  if (index_cache_.IsCachedNodeCountValid())
    return index_cache_.CachedNodeCount();

  NodeType* current_node = collection.TraverseToFirst();
  unsigned current_index = 0;
  while (current_node) {
    cached_list_.push_back(current_node);
    current_node = collection.TraverseForwardToOffset(
        current_index + 1, *current_node, current_index);
  }

  index_cache_.SetCachedNodeCount(cached_list_.size());
  list_valid_ = true;
  return index_cache_.CachedNodeCount();
}

template <typename Collection, typename NodeType>
inline NodeType* CollectionItemsCache<Collection, NodeType>::NodeAt(
    const Collection& collection,
    unsigned index) {
  if (list_valid_) {
    DCHECK(index_cache_.IsCachedNodeCountValid());
    return index < index_cache_.CachedNodeCount() ? cached_list_[index]
                                                  : nullptr;
  }
  return index_cache_.NodeAt(collection, index);
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_COLLECTION_ITEMS_CACHE_H_

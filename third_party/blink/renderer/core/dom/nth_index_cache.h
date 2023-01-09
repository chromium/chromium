// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Cache element indices for :nth-child and :nth-last-child selectors,
// and similar for :nth-of-type and :nth-last-of-type.
//
// In order to avoid n^2 for :nth-selectors, we introduce a cache where we
// store the index of every kth child of a parent node P the first time the
// nth-count is queried for one of its children. The number k is given by
// the "spread" constant, currently 3. (The number 3 was chosen after some
// kind of testing, but the details have been lost to the mists of time.)
//
// After the cache has been populated for the children of P, the nth-index
// for an element will be found by walking the siblings from the element
// queried for and leftwards until a cached element/index pair is found.
// So populating the cache for P is O(n). Subsequent lookups are best case
// O(1), worst case O(k).
//
// The cache is created on the stack when we do operations where we know we
// can benefit from having it. Currently, those are querySelector,
// querySelectorAll, and updating style. Also, we need to see at least 32
// children for the given node, which is a rough cutoff for when the cost of
// building the cache is outweighed by the gains of faster queries.
// We are throwing away the cache after each operation to avoid holding on
// to potentially large caches in memory.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NTH_INDEX_CACHE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NTH_INDEX_CACHE_H_

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_map.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"

namespace blink {

class Document;

// The cache for a given :nth-* selector; maps from each child element of
// a given node (modulo spread; see file comment) to its correct child index.
// The owner needs to key by parent and potentially tag name; we receive them
// to do the actual query, but do not store them.
class CORE_EXPORT NthIndexData final : public GarbageCollected<NthIndexData> {
 public:
  explicit NthIndexData(ContainerNode&);
  NthIndexData(ContainerNode&, const QualifiedName& type);
  NthIndexData(const NthIndexData&) = delete;
  NthIndexData& operator=(const NthIndexData&) = delete;

  unsigned NthIndex(Element&) const;
  unsigned NthLastIndex(Element&) const;
  unsigned NthOfTypeIndex(Element&) const;
  unsigned NthLastOfTypeIndex(Element&) const;

  void Trace(Visitor*) const;

 private:
  HeapHashMap<Member<Element>, unsigned> element_index_map_;

  // Number of total elements under the given node, so that we know
  // what to search for when doing nth-last-child.
  // (element_index_map_.size() is not correct, since we do not store the
  // indices for all children.)
  unsigned count_ = 0;
};

// The singleton cache, usually allocated at the stack on-demand.
// Caches for all nodes in the entire document.
//
// This class also has a dual role of RAII on Document; when constructed,
// it sets Document's NthIndexCache member to ourselves (so that NthChildIndex
// etc. can be static, and we don't need to send the cache through to
// selector matching), and when destroyed, unsets that member.
class CORE_EXPORT NthIndexCache final {
  STACK_ALLOCATED();

 public:
  explicit NthIndexCache(Document&);
  NthIndexCache(const NthIndexCache&) = delete;
  NthIndexCache& operator=(const NthIndexCache&) = delete;
  ~NthIndexCache();

  static unsigned NthChildIndex(Element&);
  static unsigned NthLastChildIndex(Element&);
  static unsigned NthOfTypeIndex(Element&);
  static unsigned NthLastOfTypeIndex(Element&);

 private:
  // Key in the top-level cache; identifies the parent and the type of query.
  struct Key : public GarbageCollected<Key> {
    explicit Key(Node* parent_arg) : parent(parent_arg) {}
    Key(Node* parent_arg, String child_tag_name_arg)
        : parent(parent_arg), child_tag_name(child_tag_name_arg) {}

    Member<Node> parent;
    String child_tag_name;  // Empty if not :nth-of-type.

    void Trace(Visitor* visitor) const { visitor->Trace(parent); }
    unsigned GetHash() const;
    bool operator==(const Key& other) const {
      return parent == other.parent && child_tag_name == other.child_tag_name;
    }
  };

  // Helper needed to make sure Key is compared by value and not by pointer,
  // even though the hash map key is a Member<> (which Oilpan forces us to).
  struct KeyHash {
    STATIC_ONLY(KeyHash);

    static unsigned GetHash(const Member<Key>& key) { return key->GetHash(); }
    static bool Equal(const Member<Key>& a, const Member<Key>& b) {
      return (!a && !b) || (a && b && *a == *b);
    }

    static const bool safe_to_compare_to_empty_or_deleted = true;
  };

  // Helper needed to allow calling Find() with a Key instead of Member<Key>
  // (note that find() does not allow this).
  struct KeyHashTranslator {
    STATIC_ONLY(KeyHashTranslator);

    static unsigned GetHash(const Key& key) { return key.GetHash(); }
    static bool Equal(const Member<Key>& a, const Key& b) {
      return a && *a == b;
    }
  };

  void CacheNthIndexDataForParent(Element&);
  void CacheNthOfTypeIndexDataForParent(Element&);
  void EnsureCache();

  Document* document_ = nullptr;

  // Effectively maps (parent, optional tag name, child) → index.
  // (The child part of the key is in NthIndexData.)
  HeapHashMap<Member<Key>, Member<NthIndexData>, KeyHash>* cache_ = nullptr;

#if DCHECK_IS_ON()
  uint64_t dom_tree_version_;
#endif
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_NTH_INDEX_CACHE_H_

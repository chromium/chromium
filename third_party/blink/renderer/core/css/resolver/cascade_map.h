// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_MAP_H_

#include <memory>

#include "base/check_op.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/css/css_property_name.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/css_bitset.h"
#include "third_party/blink/renderer/core/css/resolver/cascade_priority.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"

namespace blink {

// Optimized map from CSSPropertyNames to CascadePriority.
//
// Because using a HashMap for everything is quite expensive in terms of
// performance, this class stores standard (non-custom) properties in a fixed-
// size array, and only custom properties are stored in a HashMap.
//
// For each property, we ultimately just want one winning declaration
// to apply to ComputeStyle(Builder). However, since a CSS declaration
// can refer to a weaker declaration of the same property through various
// "revert" mechanisms (revert-rule, revert-layer, etc), we maintain
// a sorted list of all declarations seen per property.
class CORE_EXPORT CascadeMap {
  STACK_ALLOCATED();

 public:
  class CascadePriorityList;

  // Get the CascadePriority for the given CSSPropertyName. If there is no
  // entry for the given name, CascadePriority() is returned.
  CascadePriority At(const CSSPropertyName&) const;
  // Find the CascadePriority location for a given name, if present. If there
  // is no entry for the given name, nullptr is returned. If a CascadeOrigin
  // is provided, returns the CascadePriority for that origin.
  //
  // Note that the returned pointer may accessed to change the stored value.
  //
  // Note also that calling Add() invalidates the pointer.
  CascadePriority* Find(const CSSPropertyName&);
  const CascadePriority* Find(const CSSPropertyName&) const;
  const CascadePriority* Find(const CSSPropertyName&, CascadeOrigin) const;
  CascadePriority* FindKnownToExist(const CSSPropertyID id) {
    DCHECK(native_properties_.Bits().Has(id));
    return UNSAFE_BUFFERS(
        &native_properties_.Buffer()[static_cast<size_t>(id)].Top(
            backing_vector_));
  }
  const CascadePriority* FindKnownToExist(const CSSPropertyID id) const {
    DCHECK(native_properties_.Bits().Has(id));
    return UNSAFE_BUFFERS(
        &native_properties_.Buffer()[static_cast<size_t>(id)].Top(
            backing_vector_));
  }
  // Similar to Find(name, origin), but returns the CascadePriority from cascade
  // layers below the given priority. The uint64_t is presumed to come from
  // CascadePriority::ForLayerComparison().
  const CascadePriority* FindRevertLayer(const CSSPropertyName&,
                                         uint64_t) const;
  // Find that CascadePriority in the cascade map weaker than `revert_from`
  // that originates from a different rule.
  //
  // https://drafts.csswg.org/css-cascade-5/#revert-rule-keyword
  const CascadePriority* FindRevertRule(const CSSPropertyName&,
                                        CascadePriority revert_from) const;
  // Similar to Find(), if you already have the right CascadePriorityList.
  CascadePriority& Top(CascadePriorityList&);
  // Adds an entry to the map, keeping each bucket sorted.
  void Add(const AtomicString& custom_property_name, CascadePriority);
  void Add(CSSPropertyID, CascadePriority);
  // Added properties with CSSPropertyPriority::kHighPropertyPriority cause the
  // corresponding high_priority_-bit to be set. This provides a fast way to
  // check which high-priority properties have been added (if any).
  uint64_t HighPriorityBits() const {
    return native_properties_.Bits().HighPriorityBits();
  }
  // Returns the set of (native) properties that have !important set.
  // Can only be called once unless you do Reset().
  std::unique_ptr<CSSBitset> ReleaseImportantSet() {
#if DCHECK_IS_ON()
    DCHECK(!important_set_released_);
    important_set_released_ = true;
#endif
    return std::move(important_set_);
  }
  // True if any inline style declaration lost the cascade to something
  // else. This is rare, but if it happens, we need to turn off incremental
  // style calculation (see CanApplyInlineStyleIncrementally() and related
  // functions). This information is propagated up to ComputedStyle after
  // the cascade and stored there.
  bool InlineStyleLost() const { return inline_style_lost_; }
  const CSSBitset& NativeBitset() const { return native_properties_.Bits(); }
  // Remove all properties (both native and custom) from the CascadeMap.
  void Reset();
  // Clear all the already_applied_ flags on declarations.
  void ClearAppliedFlags();

  // A sorted list storing all declarations (CascadePriority objects) seen
  // for a specific property, with the strongest CascadePriority appearing
  // first.
  //
  // We generally only care about the strongest entry when computing values,
  // except for "reverts" (revert-rule, etc) that explicitly target
  // some weaker entry.
  //
  // To avoid constructor and destructor calls on a large number of lists, the
  // list is implemented as a linked stack where nodes are backed by a vector.
  class CascadePriorityList {
    DISALLOW_NEW();

    struct Node {
      DISALLOW_NEW();
      Node(CascadePriority priority, wtf_size_t next_index)
          : priority(priority), next_index(next_index) {}

      CascadePriority priority;
      // Index in the backing vector, or kNotFound for "none".
      wtf_size_t next_index;
    };

    // The inline size is set to avoid re-allocations in common cases. The
    // following allows a UA and author declaration on every property without
    // re-allocation.
    using BackingVector = Vector<Node, kNumCSSProperties * 2>;

   public:
    CascadePriorityList() = default;
    inline CascadePriorityList(BackingVector& backing_vector,
                               CascadePriority priority)
        : head_index_(backing_vector.size()) {
      backing_vector.emplace_back(priority, kNotFound);
    }

    class Iterator {
      STACK_ALLOCATED();

     public:
      inline Iterator(const BackingVector*, const Node*);
      inline bool operator!=(const Iterator&) const;
      inline const CascadePriority& operator*() const;
      inline const CascadePriority* operator->() const;
      inline Iterator& operator++();
      Iterator& operator++(int) = delete;

     private:
      using BackingVector = CascadePriorityList::BackingVector;
      using Node = CascadePriorityList::Node;

      const BackingVector* backing_vector_;
      const Node* backing_node_;
    };

    inline bool IsEmpty() const;

    // For performance reasons, we don't store the BackingVector reference in
    // each list, but pass it as a parameter.
    inline Iterator Begin(const BackingVector&) const;
    inline Iterator End(const BackingVector&) const;
    inline const CascadePriority& Top(const BackingVector&) const;
    inline CascadePriority& Top(BackingVector&);
    inline void Push(BackingVector&, CascadePriority priority);
    inline void InsertKeepingSorted(BackingVector&, CascadePriority priority);

   private:
    friend class Iterator;

    wtf_size_t head_index_ = kNotFound;
  };

  class NativeMap {
    STACK_ALLOCATED();

   public:
    CSSBitset& Bits() { return bits_; }
    const CSSBitset& Bits() const { return bits_; }

    CascadePriorityList* Buffer() {
      return reinterpret_cast<CascadePriorityList*>(properties_);
    }
    const CascadePriorityList* Buffer() const {
      return reinterpret_cast<const CascadePriorityList*>(properties_);
    }

   private:
    // For performance reasons, a char-array is used to prevent construction of
    // CascadePriorityList objects. A companion bitset keeps track of which
    // properties are initialized.
    CSSBitset bits_;
    alignas(CascadePriorityList) char properties_[kNumCSSProperties *
                                                  sizeof(CascadePriorityList)];
  };

  using CustomMap = HashMap<AtomicString, CascadePriorityList>;

  const CustomMap& GetCustomMap() const { return custom_properties_; }
  CustomMap& GetCustomMap() { return custom_properties_; }

 private:
  ALWAYS_INLINE void Add(CascadePriorityList* list, CascadePriority);

  bool inline_style_lost_ = false;
  NativeMap native_properties_;
  CustomMap custom_properties_;
  CascadePriorityList::BackingVector backing_vector_;
  std::unique_ptr<CSSBitset> important_set_;
#if DCHECK_IS_ON()
  bool important_set_released_ = false;
#endif
};

// CascadePriorityList implementation is inlined for performance reasons.

inline CascadeMap::CascadePriorityList::Iterator::Iterator(
    const BackingVector* backing_vector,
    const Node* node)
    : backing_vector_(backing_vector), backing_node_(node) {}

inline const CascadePriority&
CascadeMap::CascadePriorityList::Iterator::operator*() const {
  return backing_node_->priority;
}

inline const CascadePriority*
CascadeMap::CascadePriorityList::Iterator::operator->() const {
  return &backing_node_->priority;
}

inline CascadeMap::CascadePriorityList::Iterator&
CascadeMap::CascadePriorityList::Iterator::operator++() {
  if (backing_node_->next_index == kNotFound) {
    backing_node_ = nullptr;
  } else {
    backing_node_ = &backing_vector_->at(backing_node_->next_index);
  }
  return *this;
}

inline bool CascadeMap::CascadePriorityList::Iterator::operator!=(
    const Iterator& other) const {
  // We should never compare two iterators backed by different vectors.
  DCHECK_EQ(backing_vector_, other.backing_vector_);
  return backing_node_ != other.backing_node_;
}

inline CascadeMap::CascadePriorityList::Iterator
CascadeMap::CascadePriorityList ::Begin(
    const BackingVector& backing_vector) const {
  if (head_index_ == kNotFound) {
    return Iterator(&backing_vector, nullptr);
  }
  return Iterator(&backing_vector, &backing_vector[head_index_]);
}

inline CascadeMap::CascadePriorityList::Iterator
CascadeMap::CascadePriorityList::End(
    const BackingVector& backing_vector) const {
  return Iterator(&backing_vector, nullptr);
}

inline const CascadePriority& CascadeMap::CascadePriorityList::Top(
    const BackingVector& backing_vector) const {
  DCHECK(!IsEmpty());
  return *Begin(backing_vector);
}

inline CascadePriority& CascadeMap::CascadePriorityList::Top(
    BackingVector& backing_vector) {
  DCHECK(!IsEmpty());
  return const_cast<CascadePriority&>(*Begin(backing_vector));
}

inline void CascadeMap::CascadePriorityList::Push(BackingVector& backing_vector,
                                                  CascadePriority priority) {
  backing_vector.emplace_back(priority, head_index_);
  head_index_ = backing_vector.size() - 1;
}

inline void CascadeMap::CascadePriorityList::InsertKeepingSorted(
    BackingVector& backing_vector,
    CascadePriority priority) {
  // We're inserting `priority` into an already-sorted linked list
  // (higher properties appear first in the list).
  wtf_size_t prev_index = kNotFound;
  wtf_size_t curr_index = head_index_;
  while (curr_index != kNotFound) {
    Node& curr_node = backing_vector[curr_index];
    if (priority >= curr_node.priority) {
      // The incoming priority is bigger (or equal) to this node;
      // we'll insert before that node.
      break;
    }
    prev_index = curr_index;
    curr_index = curr_node.next_index;
  }

  // The new node exists at `new_index`, and points to the node
  // at `curr_index`.
  wtf_size_t new_index = backing_vector.size();
  backing_vector.emplace_back(priority, curr_index);

  // Now link the preceding node to our new node.
  if (prev_index == kNotFound) {
    // The new node had no preceding node (it was inserted at the front).
    //
    // While this is supported by this function for completeness, it should
    // not really happen in normal circumstances; the call site should prefer
    // to call Push() in this scenario.
    head_index_ = new_index;
  } else {
    // Link the previous node to the new node.
    backing_vector[prev_index].next_index = new_index;
  }
}

inline bool CascadeMap::CascadePriorityList::IsEmpty() const {
  return head_index_ == kNotFound;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_CSS_RESOLVER_CASCADE_MAP_H_

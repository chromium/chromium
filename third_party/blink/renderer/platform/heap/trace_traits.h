// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_TRACE_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_TRACE_TRAITS_H_

#include "base/optional.h"
#include "third_party/blink/renderer/platform/heap/gc_info.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_table.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/list_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

template <typename ValueArg, wtf_size_t inlineCapacity>
class HeapListHashSetAllocator;
template <typename T>
struct TraceTrait;
template <typename T>
class WeakMember;

template <typename T, bool = NeedsAdjustPointer<T>::value>
struct AdjustPointerTrait;

template <typename T>
struct AdjustPointerTrait<T, false> {
  STATIC_ONLY(AdjustPointerTrait);

  static TraceDescriptor GetTraceDescriptor(void* self) {
    return {self, TraceTrait<T>::Trace};
  }

  static HeapObjectHeader* GetHeapObjectHeader(void* self) {
    return HeapObjectHeader::FromPayload(self);
  }
};

template <typename T>
struct AdjustPointerTrait<T, true> {
  STATIC_ONLY(AdjustPointerTrait);

  static TraceDescriptor GetTraceDescriptor(void* self) {
    return static_cast<T*>(self)->GetTraceDescriptor();
  }

  static HeapObjectHeader* GetHeapObjectHeader(void* self) {
    return static_cast<T*>(self)->GetHeapObjectHeader();
  }
};

template <typename T, bool = WTF::IsTraceable<T>::value>
struct TraceIfNeeded;

template <typename T>
struct TraceIfNeeded<T, false> {
  STATIC_ONLY(TraceIfNeeded);
  static void Trace(blink::Visitor*, T&) {}
};

template <typename T>
struct TraceIfNeeded<T, true> {
  STATIC_ONLY(TraceIfNeeded);
  static void Trace(blink::Visitor* visitor, T& t) { visitor->Trace(t); }
};

template <WTF::WeakHandlingFlag weakness,
          typename T,
          typename Traits,
          bool = WTF::IsTraceableInCollectionTrait<Traits>::value,
          WTF::WeakHandlingFlag = WTF::WeakHandlingTrait<T>::value>
struct TraceCollectionIfEnabled;

template <WTF::WeakHandlingFlag weakness, typename T, typename Traits>
struct TraceCollectionIfEnabled<weakness,
                                T,
                                Traits,
                                false,
                                WTF::kNoWeakHandling> {
  STATIC_ONLY(TraceCollectionIfEnabled);

  static bool IsAlive(T&) { return true; }

  static bool Trace(blink::Visitor*, void*) {
    static_assert(!WTF::IsTraceableInCollectionTrait<Traits>::value,
                  "T should not be traced");
    return false;
  }
};

template <typename T, typename Traits>
struct TraceCollectionIfEnabled<WTF::kNoWeakHandling,
                                T,
                                Traits,
                                false,
                                WTF::kWeakHandling> {
  STATIC_ONLY(TraceCollectionIfEnabled);

  static bool Trace(blink::Visitor* visitor, void* t) {
    return WTF::TraceInCollectionTrait<WTF::kNoWeakHandling, T, Traits>::Trace(
        visitor, *reinterpret_cast<T*>(t));
  }
};

template <WTF::WeakHandlingFlag weakness,
          typename T,
          typename Traits,
          bool,
          WTF::WeakHandlingFlag>
struct TraceCollectionIfEnabled {
  STATIC_ONLY(TraceCollectionIfEnabled);

  static bool IsAlive(T& traceable) {
    return WTF::TraceInCollectionTrait<weakness, T, Traits>::IsAlive(traceable);
  }

  static bool Trace(blink::Visitor* visitor, void* t) {
    static_assert(WTF::IsTraceableInCollectionTrait<Traits>::value ||
                      weakness == WTF::kWeakHandling,
                  "Traits should be traced");
    return WTF::TraceInCollectionTrait<weakness, T, Traits>::Trace(
        visitor, *reinterpret_cast<T*>(t));
  }
};

// The TraceTrait is used to specify how to trace and object for Oilpan and
// wrapper tracing.
//
//
// By default, the 'Trace' method implemented on an object itself is
// used to trace the pointers to other heap objects inside the object.
//
// However, the TraceTrait can be specialized to use a different
// implementation. A common case where a TraceTrait specialization is
// needed is when multiple inheritance leads to pointers that are not
// to the start of the object in the Blink garbage-collected heap. In
// that case the pointer has to be adjusted before marking.
template <typename T>
struct TraceTrait {
  STATIC_ONLY(TraceTrait);

 public:
  static TraceDescriptor GetTraceDescriptor(void* self) {
    return AdjustPointerTrait<T>::GetTraceDescriptor(static_cast<T*>(self));
  }

  static TraceDescriptor GetWeakTraceDescriptor(void* self) {
    return {self, nullptr};
  }

  static HeapObjectHeader* GetHeapObjectHeader(void* self) {
    return AdjustPointerTrait<T>::GetHeapObjectHeader(static_cast<T*>(self));
  }

  static void Trace(Visitor*, void* self);
};

template <typename T>
struct TraceTrait<const T> : public TraceTrait<T> {};

template <typename T>
void TraceTrait<T>::Trace(Visitor* visitor, void* self) {
  static_assert(WTF::IsTraceable<T>::value, "T should not be traced");
  static_cast<T*>(self)->Trace(visitor);
}

template <typename T, typename Traits>
struct TraceTrait<HeapVectorBacking<T, Traits>> {
  STATIC_ONLY(TraceTrait);
  using Backing = HeapVectorBacking<T, Traits>;

 public:
  static TraceDescriptor GetTraceDescriptor(void* self) {
    return {self, TraceTrait<Backing>::Trace};
  }

  static void Trace(blink::Visitor* visitor, void* self) {
    static_assert(!WTF::IsWeak<T>::value,
                  "Weakness is not supported in HeapVector and HeapDeque");
    if (WTF::IsTraceableInCollectionTrait<Traits>::value) {
      WTF::TraceInCollectionTrait<WTF::kNoWeakHandling,
                                  HeapVectorBacking<T, Traits>,
                                  void>::Trace(visitor, self);
    }
  }
};

// The trace trait for the heap hashtable backing is used when we find a
// direct pointer to the backing from the conservative stack scanner. This
// normally indicates that there is an ongoing iteration over the table, and so
// we disable weak processing of table entries. When the backing is found
// through the owning hash table we mark differently, in order to do weak
// processing.
template <typename Table>
struct TraceTrait<HeapHashTableBacking<Table>> {
  STATIC_ONLY(TraceTrait);
  using Backing = HeapHashTableBacking<Table>;
  using ValueType = typename Table::ValueTraits::TraitType;
  using Traits = typename Table::ValueTraits;

 public:
  static TraceDescriptor GetTraceDescriptor(void* self) {
    return {self, Trace<WTF::kNoWeakHandling>};
  }

  static TraceDescriptor GetWeakTraceDescriptor(void* self) {
    return GetWeakTraceDescriptorImpl<ValueType>::GetWeakTraceDescriptor(self);
  }

  template <WTF::WeakHandlingFlag WeakHandling = WTF::kNoWeakHandling>
  static void Trace(Visitor* visitor, void* self) {
    static_assert(WTF::IsTraceableInCollectionTrait<Traits>::value ||
                      WTF::IsWeak<ValueType>::value,
                  "T should not be traced");
    WTF::TraceInCollectionTrait<WeakHandling, Backing, void>::Trace(visitor,
                                                                    self);
  }

 private:
  template <typename ValueType>
  struct GetWeakTraceDescriptorImpl {
    static TraceDescriptor GetWeakTraceDescriptor(void* backing) {
      return {backing, nullptr};
    }
  };

  template <typename K, typename V>
  struct GetWeakTraceDescriptorImpl<WTF::KeyValuePair<K, V>> {
    static TraceDescriptor GetWeakTraceDescriptor(void* backing) {
      return GetWeakTraceDescriptorKVPImpl<K, V>::GetWeakTraceDescriptor(
          backing);
    }

    template <typename KeyType,
              typename ValueType,
              bool ephemeron_semantics = (WTF::IsWeak<KeyType>::value &&
                                          !WTF::IsWeak<ValueType>::value &&
                                          WTF::IsTraceable<ValueType>::value) ||
                                         (WTF::IsWeak<ValueType>::value &&
                                          !WTF::IsWeak<KeyType>::value &&
                                          WTF::IsTraceable<KeyType>::value)>
    struct GetWeakTraceDescriptorKVPImpl {
      static TraceDescriptor GetWeakTraceDescriptor(void* backing) {
        return {backing, nullptr};
      }
    };

    template <typename KeyType, typename ValueType>
    struct GetWeakTraceDescriptorKVPImpl<KeyType, ValueType, true> {
      static TraceDescriptor GetWeakTraceDescriptor(void* backing) {
        return {backing, Trace<WTF::kWeakHandling>};
      }
    };
  };
};

// This trace trait for std::pair will null weak members if their referent is
// collected. If you have a collection that contain weakness it does not remove
// entries from the collection that contain nulled weak members.
template <typename T, typename U>
struct TraceTrait<std::pair<T, U>> {
  STATIC_ONLY(TraceTrait);

 public:
  static void Trace(blink::Visitor* visitor, std::pair<T, U>* pair) {
    TraceIfNeeded<T>::Trace(visitor, pair->first);
    TraceIfNeeded<U>::Trace(visitor, pair->second);
  }
};

// While using base::Optional<T> with garbage-collected types is generally
// disallowed by the OptionalGarbageCollected check in blink_gc_plugin,
// garbage-collected containers such as HeapVector are allowed and need to be
// traced.
template <typename T>
struct TraceTrait<base::Optional<T>> {
  STATIC_ONLY(TraceTrait);

 public:
  static void Trace(blink::Visitor* visitor, base::Optional<T>* optional) {
    if (*optional != base::nullopt) {
      TraceIfNeeded<T>::Trace(visitor, optional->value());
    }
  }
};

// Reorders parameters for use in blink::Visitor::VisitEphemeronKeyValuePair.
template <typename _KeyType,
          typename _ValueType,
          typename _KeyTraits,
          typename _ValueTraits,
          bool = WTF::IsWeak<_ValueType>::value>
struct EphemeronKeyValuePair {
  using KeyType = _KeyType;
  using ValueType = _ValueType;
  using KeyTraits = _KeyTraits;
  using ValueTraits = _ValueTraits;

  EphemeronKeyValuePair(KeyType* k, ValueType* v) : key(k), value(v) {}
  KeyType* key;
  ValueType* value;
};

template <typename _KeyType,
          typename _ValueType,
          typename _KeyTraits,
          typename _ValueTraits>
struct EphemeronKeyValuePair<_KeyType,
                             _ValueType,
                             _KeyTraits,
                             _ValueTraits,
                             true> : EphemeronKeyValuePair<_ValueType,
                                                           _KeyType,
                                                           _ValueTraits,
                                                           _KeyTraits,
                                                           false> {
  EphemeronKeyValuePair(_KeyType* k, _ValueType* v)
      : EphemeronKeyValuePair<_ValueType,
                              _KeyType,
                              _ValueTraits,
                              _KeyTraits,
                              false>(v, k) {}
};

}  // namespace blink

namespace WTF {

// Catch-all for types that have a way to trace that don't have special
// handling for weakness in collections.  This means that if this type
// contains WeakMember fields, they will simply be zeroed, but the entry
// will not be removed from the collection.  This always happens for
// things in vectors, which don't currently support special handling of
// weak elements.
template <typename T, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling, T, Traits> {
  static bool IsAlive(T& t) { return true; }

  static bool Trace(blink::Visitor* visitor, T& t) {
    static_assert(IsTraceableInCollectionTrait<Traits>::value,
                  "T should not be traced");
    visitor->Trace(t);
    return false;
  }
};

template <typename T, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling, blink::WeakMember<T>, Traits> {
  static bool Trace(blink::Visitor* visitor, blink::WeakMember<T>& t) {
    // Extract raw pointer to avoid using the WeakMember<> overload in Visitor.
    visitor->Trace(t.Get());
    return false;
  }
};

// Catch-all for types that have HashTrait support for tracing with weakness.
// Empty to enforce specialization.
template <typename T, typename Traits>
struct TraceInCollectionTrait<kWeakHandling, T, Traits> {};

template <typename T, typename Traits>
struct TraceInCollectionTrait<kWeakHandling, blink::WeakMember<T>, Traits> {
  static bool IsAlive(blink::WeakMember<T>& value) {
    return blink::ThreadHeap::IsHeapObjectAlive(value);
  }

  static bool Trace(blink::Visitor* visitor, blink::WeakMember<T>& value) {
    return !blink::ThreadHeap::IsHeapObjectAlive(value);
  }
};

// This trace method is used only for on-stack HeapVectors found in
// conservative scanning. On-heap HeapVectors are traced by Vector::trace.
template <typename T, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling,
                              blink::HeapVectorBacking<T, Traits>,
                              void> {
  static bool Trace(blink::Visitor* visitor, void* self) {
    // HeapVectorBacking does not know the exact size of the vector
    // and just knows the capacity of the vector. Due to the constraint,
    // HeapVectorBacking can support only the following objects:
    //
    // - An object that has a vtable. In this case, HeapVectorBacking
    //   traces only slots that are not zeroed out. This is because if
    //   the object has a vtable, the zeroed slot means that it is
    //   an unused slot (Remember that the unused slots are guaranteed
    //   to be zeroed out by VectorUnusedSlotClearer).
    //
    // - An object that can be initialized with memset. In this case,
    //   HeapVectorBacking traces all slots including unused slots.
    //   This is fine because the fact that the object can be initialized
    //   with memset indicates that it is safe to treat the zerod slot
    //   as a valid object.
    static_assert(!IsTraceableInCollectionTrait<Traits>::value ||
                      Traits::kCanClearUnusedSlotsWithMemset ||
                      std::is_polymorphic<T>::value,
                  "HeapVectorBacking doesn't support objects that cannot be "
                  "cleared as unused with memset.");

    // This trace method is instantiated for vectors where
    // IsTraceableInCollectionTrait<Traits>::value is false, but the trace
    // method should not be called. Thus we cannot static-assert
    // IsTraceableInCollectionTrait<Traits>::value but should runtime-assert it.
    DCHECK(IsTraceableInCollectionTrait<Traits>::value);

    T* array = reinterpret_cast<T*>(self);
    blink::HeapObjectHeader* header =
        blink::HeapObjectHeader::FromPayload(self);
    // Use the payload size as recorded by the heap to determine how many
    // elements to trace.
    size_t length = header->PayloadSize() / sizeof(T);
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
    // As commented above, HeapVectorBacking can trace unused slots
    // (which are already zeroed out).
    ANNOTATE_CHANGE_SIZE(array, length, 0, length);
#endif
    if (std::is_polymorphic<T>::value) {
      char* pointer = reinterpret_cast<char*>(array);
      for (unsigned i = 0; i < length; ++i) {
        char* element = pointer + i * sizeof(T);
        if (blink::VTableInitialized(element))
          blink::TraceIfNeeded<
              T, IsTraceableInCollectionTrait<Traits>::value>::Trace(visitor,
                                                                     array[i]);
      }
    } else {
      for (size_t i = 0; i < length; ++i)
        blink::TraceIfNeeded<
            T, IsTraceableInCollectionTrait<Traits>::value>::Trace(visitor,
                                                                   array[i]);
    }
    return false;
  }
};

// This trace method is for tracing a HashTableBacking either through regular
// tracing (via the relevant TraceTraits) or when finding a HashTableBacking
// through conservative stack scanning (which will treat all references in the
// backing strongly).
template <WTF::WeakHandlingFlag WeakHandling, typename Table>
struct TraceHashTableBackingInCollectionTrait {
  using Value = typename Table::ValueType;
  using Traits = typename Table::ValueTraits;

  static bool Trace(blink::Visitor* visitor, void* self) {
    static_assert(IsTraceableInCollectionTrait<Traits>::value ||
                      WTF::IsWeak<Value>::value,
                  "Table should not be traced");
    Value* array = reinterpret_cast<Value*>(self);
    blink::HeapObjectHeader* header =
        blink::HeapObjectHeader::FromPayload(self);
    // Use the payload size as recorded by the heap to determine how many
    // elements to trace.
    size_t length = header->PayloadSize() / sizeof(Value);
    for (size_t i = 0; i < length; ++i) {
      if (!HashTableHelper<Value, typename Table::ExtractorType,
                           typename Table::KeyTraitsType>::
              IsEmptyOrDeletedBucket(array[i])) {
        blink::TraceCollectionIfEnabled<WeakHandling, Value, Traits>::Trace(
            visitor, &array[i]);
      }
    }
    return false;
  }
};

template <typename Table>
struct TraceInCollectionTrait<kNoWeakHandling,
                              blink::HeapHashTableBacking<Table>,
                              void> {
  static bool Trace(blink::Visitor* visitor, void* self) {
    return TraceHashTableBackingInCollectionTrait<kNoWeakHandling,
                                                  Table>::Trace(visitor, self);
  }
};

template <typename Table>
struct TraceInCollectionTrait<kWeakHandling,
                              blink::HeapHashTableBacking<Table>,
                              void> {
  static bool Trace(blink::Visitor* visitor, void* self) {
    return TraceHashTableBackingInCollectionTrait<kWeakHandling, Table>::Trace(
        visitor, self);
  }
};

// This specialization of TraceInCollectionTrait is for the backing of
// HeapListHashSet.  This is for the case that we find a reference to the
// backing from the stack.  That probably means we have a GC while we are in a
// ListHashSet method since normal API use does not put pointers to the backing
// on the stack.
template <typename NodeContents,
          size_t inlineCapacity,
          typename T,
          typename U,
          typename V,
          typename W,
          typename X,
          typename Y>
struct TraceInCollectionTrait<
    kNoWeakHandling,
    blink::HeapHashTableBacking<HashTable<
        ListHashSetNode<NodeContents,
                        blink::HeapListHashSetAllocator<T, inlineCapacity>>*,
        U,
        V,
        W,
        X,
        Y,
        blink::HeapAllocator>>,
    void> {
  using Node =
      ListHashSetNode<NodeContents,
                      blink::HeapListHashSetAllocator<T, inlineCapacity>>;
  using Table = HashTable<Node*, U, V, W, X, Y, blink::HeapAllocator>;

  static bool Trace(blink::Visitor* visitor, void* self) {
    Node** array = reinterpret_cast<Node**>(self);
    blink::HeapObjectHeader* header =
        blink::HeapObjectHeader::FromPayload(self);
    size_t length = header->PayloadSize() / sizeof(Node*);
    for (size_t i = 0; i < length; ++i) {
      if (!HashTableHelper<Node*, typename Table::ExtractorType,
                           typename Table::KeyTraitsType>::
              IsEmptyOrDeletedBucket(array[i])) {
        visitor->Trace(array[i]);
      }
    }
    return false;
  }
};

// Key value pairs, as used in HashMap.  To disambiguate template choice we have
// to have two versions, first the one with no special weak handling, then the
// one with weak handling.
template <typename Key, typename Value, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling,
                              KeyValuePair<Key, Value>,
                              Traits> {
  using EphemeronHelper =
      blink::EphemeronKeyValuePair<Key,
                                   Value,
                                   typename Traits::KeyTraits,
                                   typename Traits::ValueTraits>;

  static bool Trace(blink::Visitor* visitor, KeyValuePair<Key, Value>& self) {
    if (WTF::IsWeak<Key>::value != WTF::IsWeak<Value>::value) {
      // Strongification of Weak/Strong and Strong/Weak.
      EphemeronHelper helper(&self.key, &self.value);
      visitor->VisitEphemeronKeyValuePair(
          helper.key, helper.value,
          blink::TraceCollectionIfEnabled<
              kNoWeakHandling, typename EphemeronHelper::KeyType,
              typename EphemeronHelper::KeyTraits>::Trace,
          blink::TraceCollectionIfEnabled<
              kNoWeakHandling, typename EphemeronHelper::ValueType,
              typename EphemeronHelper::ValueTraits>::Trace);
    } else {
      // Strongification of Strong/Strong or Weak/Weak. Order does not matter
      // here.
      blink::TraceCollectionIfEnabled<
          kNoWeakHandling, Key, typename Traits::KeyTraits>::Trace(visitor,
                                                                   &self.key);
      blink::TraceCollectionIfEnabled<
          kNoWeakHandling, Value,
          typename Traits::ValueTraits>::Trace(visitor, &self.value);
    }
    return false;
  }
};

template <typename Key, typename Value, typename Traits>
struct TraceInCollectionTrait<kWeakHandling, KeyValuePair<Key, Value>, Traits> {
  using EphemeronHelper =
      blink::EphemeronKeyValuePair<Key,
                                   Value,
                                   typename Traits::KeyTraits,
                                   typename Traits::ValueTraits>;

  static bool IsAlive(KeyValuePair<Key, Value>& self) {
    // Needed for Weak/Weak, Strong/Weak (reverse ephemeron), and Weak/Strong
    // (ephemeron). Order of invocation does not matter as tracing weak key or
    // value does not have any side effects.
    return blink::TraceCollectionIfEnabled<
               WeakHandlingTrait<Key>::value, Key,
               typename Traits::KeyTraits>::IsAlive(self.key) &&
           blink::TraceCollectionIfEnabled<
               WeakHandlingTrait<Value>::value, Value,
               typename Traits::ValueTraits>::IsAlive(self.value);
  }

  static bool Trace(blink::Visitor* visitor, KeyValuePair<Key, Value>& self) {
    EphemeronHelper helper(&self.key, &self.value);
    return visitor->VisitEphemeronKeyValuePair(
        helper.key, helper.value,
        blink::TraceCollectionIfEnabled<
            WeakHandlingTrait<typename EphemeronHelper::KeyType>::value,
            typename EphemeronHelper::KeyType,
            typename EphemeronHelper::KeyTraits>::Trace,
        blink::TraceCollectionIfEnabled<
            WeakHandlingTrait<typename EphemeronHelper::ValueType>::value,
            typename EphemeronHelper::ValueType,
            typename EphemeronHelper::ValueTraits>::Trace);
  }
};

// Nodes used by LinkedHashSet.  Again we need two versions to disambiguate the
// template.
template <typename Value, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling,
                              LinkedHashSetNode<Value>,
                              Traits> {
  static bool IsAlive(LinkedHashSetNode<Value>& self) {
    return TraceInCollectionTrait<
        kNoWeakHandling, Value,
        typename Traits::ValueTraits>::IsAlive(self.value_);
  }

  static bool Trace(blink::Visitor* visitor, LinkedHashSetNode<Value>& self) {
    static_assert(
        IsTraceableInCollectionTrait<Traits>::value || IsWeak<Value>::value,
        "T should not be traced");
    return TraceInCollectionTrait<
        kNoWeakHandling, Value,
        typename Traits::ValueTraits>::Trace(visitor, self.value_);
  }
};

template <typename Value, typename Traits>
struct TraceInCollectionTrait<kWeakHandling, LinkedHashSetNode<Value>, Traits> {
  static bool IsAlive(LinkedHashSetNode<Value>& self) {
    return TraceInCollectionTrait<
        kWeakHandling, Value,
        typename Traits::ValueTraits>::IsAlive(self.value_);
  }

  static bool Trace(blink::Visitor* visitor, LinkedHashSetNode<Value>& self) {
    return TraceInCollectionTrait<
        kWeakHandling, Value, typename Traits::ValueTraits>::Trace(visitor,
                                                                   self.value_);
  }
};

// ListHashSetNode pointers (a ListHashSet is implemented as a hash table of
// these pointers).
template <typename Value, size_t inlineCapacity, typename Traits>
struct TraceInCollectionTrait<
    kNoWeakHandling,
    ListHashSetNode<Value,
                    blink::HeapListHashSetAllocator<Value, inlineCapacity>>*,
    Traits> {
  using Node =
      ListHashSetNode<Value,
                      blink::HeapListHashSetAllocator<Value, inlineCapacity>>;

  static bool Trace(blink::Visitor* visitor, Node* node) {
    static_assert(!IsWeak<Node>::value,
                  "ListHashSet does not support weakness");
    static_assert(IsTraceableInCollectionTrait<Traits>::value,
                  "T should not be traced");
    visitor->Trace(node);
    return false;
  }
};

}  // namespace WTF

#endif

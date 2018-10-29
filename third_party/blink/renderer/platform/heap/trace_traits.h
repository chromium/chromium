// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_TRACE_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_TRACE_TRAITS_H_

#include "base/optional.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable_visitor.h"
#include "third_party/blink/renderer/platform/heap/gc_info.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/stack_frame_depth.h"
#include "third_party/blink/renderer/platform/heap/visitor.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_table.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/list_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

template <typename T>
class CrossThreadPersistent;
template <typename T>
class CrossThreadWeakPersistent;
template <typename T>
class HeapDoublyLinkedList;
template <typename T>
class HeapTerminatedArray;
template <typename T>
class Member;
template <typename T>
class TraceEagerlyTrait;
template <typename T>
class TraceTrait;
template <typename T>
class WeakMember;
template <typename T>
class Persistent;
template <typename T>
class WeakPersistent;

template <typename T, bool = NeedsAdjustPointer<T>::value>
class AdjustPointerTrait;

template <typename T>
class AdjustPointerTrait<T, false> {
  STATIC_ONLY(AdjustPointerTrait);

 public:
  static TraceDescriptor GetTraceDescriptor(void* self) {
    return {self, TraceTrait<T>::Trace, TraceEagerlyTrait<T>::value};
  }

  static HeapObjectHeader* GetHeapObjectHeader(void* self) {
#if DCHECK_IS_ON()
    HeapObjectHeader::CheckFromPayload(self);
#endif
    return HeapObjectHeader::FromPayload(self);
  }
};

template <typename T>
class AdjustPointerTrait<T, true> {
  STATIC_ONLY(AdjustPointerTrait);

 public:
  static TraceDescriptor GetTraceDescriptor(const T* self) {
    DCHECK(self);
    return self->GetTraceDescriptor();
  }

  static HeapObjectHeader* GetHeapObjectHeader(const T* self) {
    DCHECK(self);
    return self->GetHeapObjectHeader();
  }
};

template <typename T, bool isTraceable>
struct TraceIfEnabled;

template <typename T>
struct TraceIfEnabled<T, false> {
  STATIC_ONLY(TraceIfEnabled);
  template <typename VisitorDispatcher>
  static void Trace(VisitorDispatcher, T&) {
    static_assert(!WTF::IsTraceable<T>::value, "T should not be traced");
  }
};

template <typename T>
struct TraceIfEnabled<T, true> {
  STATIC_ONLY(TraceIfEnabled);
  template <typename VisitorDispatcher>
  static void Trace(VisitorDispatcher visitor, T& t) {
    static_assert(WTF::IsTraceable<T>::value, "T should not be traced");
    visitor->Trace(t);
  }
};

template <WTF::WeakHandlingFlag weakness,
          typename T,
          typename Traits,
          bool isTraceableInCollection =
              WTF::IsTraceableInCollectionTrait<Traits>::value,
          WTF::WeakHandlingFlag traitWeakHandling = Traits::kWeakHandlingFlag>
struct TraceCollectionIfEnabled;

template <WTF::WeakHandlingFlag weakness, typename T, typename Traits>
struct TraceCollectionIfEnabled<weakness,
                                T,
                                Traits,
                                false,
                                WTF::kNoWeakHandling> {
  STATIC_ONLY(TraceCollectionIfEnabled);

  static bool IsAlive(T&) { return true; }

  template <typename VisitorDispatcher>
  static bool Trace(VisitorDispatcher, T&) {
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
  template <typename VisitorDispatcher>
  static bool Trace(VisitorDispatcher visitor, T& t) {
    return WTF::TraceInCollectionTrait<WTF::kNoWeakHandling, T, Traits>::Trace(
        visitor, t);
  }
};

template <WTF::WeakHandlingFlag weakness,
          typename T,
          typename Traits,
          bool isTraceableInCollection,
          WTF::WeakHandlingFlag traitWeakHandling>
struct TraceCollectionIfEnabled {
  STATIC_ONLY(TraceCollectionIfEnabled);

  static bool IsAlive(T& traceable) {
    return WTF::TraceInCollectionTrait<weakness, T, Traits>::IsAlive(traceable);
  }

  template <typename VisitorDispatcher>
  static bool Trace(VisitorDispatcher visitor, T& t) {
    static_assert(WTF::IsTraceableInCollectionTrait<Traits>::value ||
                      weakness == WTF::kWeakHandling,
                  "Traits should be traced");
    return WTF::TraceInCollectionTrait<weakness, T, Traits>::Trace(visitor, t);
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
class TraceTrait {
  STATIC_ONLY(TraceTrait);

 public:
  static TraceDescriptor GetTraceDescriptor(void* self) {
    return AdjustPointerTrait<T>::GetTraceDescriptor(static_cast<T*>(self));
  }

  static HeapObjectHeader* GetHeapObjectHeader(void* self) {
    return AdjustPointerTrait<T>::GetHeapObjectHeader(static_cast<T*>(self));
  }

  static void Trace(Visitor*, void* self);
};

template <typename T>
class TraceTrait<const T> : public TraceTrait<T> {};

template <typename T>
void TraceTrait<T>::Trace(Visitor* visitor, void* self) {
  static_assert(WTF::IsTraceable<T>::value, "T should not be traced");
  static_cast<T*>(self)->Trace(visitor);
}

template <typename T, typename Traits>
struct TraceTrait<HeapVectorBacking<T, Traits>> {
  STATIC_ONLY(TraceTrait);
  using Backing = HeapVectorBacking<T, Traits>;

  static TraceDescriptor GetTraceDescriptor(void* self) {
    return {self, TraceTrait<Backing>::Trace,
            TraceEagerlyTrait<Backing>::value};
  }

  template <typename VisitorDispatcher>
  static void Trace(VisitorDispatcher visitor, void* self) {
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
// direct pointer to the backing from the conservative stack scanner.  This
// normally indicates that there is an ongoing iteration over the table, and so
// we disable weak processing of table entries.  When the backing is found
// through the owning hash table we mark differently, in order to do weak
// processing.
template <typename Table>
struct TraceTrait<HeapHashTableBacking<Table>> {
  STATIC_ONLY(TraceTrait);
  using Backing = HeapHashTableBacking<Table>;
  using Traits = typename Table::ValueTraits;

  static TraceDescriptor GetTraceDescriptor(void* self) {
    return {self, TraceTrait<Backing>::Trace,
            TraceEagerlyTrait<Backing>::value};
  }

  template <typename VisitorDispatcher>
  static void Trace(VisitorDispatcher visitor, void* self) {
    if (WTF::IsTraceableInCollectionTrait<Traits>::value ||
        Traits::kWeakHandlingFlag == WTF::kWeakHandling) {
      WTF::TraceInCollectionTrait<WTF::kNoWeakHandling, Backing, void>::Trace(
          visitor, self);
    }
  }
};

// This trace trait for std::pair will null weak members if their referent is
// collected. If you have a collection that contain weakness it does not remove
// entries from the collection that contain nulled weak members.
template <typename T, typename U>
class TraceTrait<std::pair<T, U>> {
  STATIC_ONLY(TraceTrait);

 public:
  static const bool kFirstIsTraceable = WTF::IsTraceable<T>::value;
  static const bool kSecondIsTraceable = WTF::IsTraceable<U>::value;
  template <typename VisitorDispatcher>
  static void Trace(VisitorDispatcher visitor, std::pair<T, U>* pair) {
    TraceIfEnabled<T, kFirstIsTraceable>::Trace(visitor, pair->first);
    TraceIfEnabled<U, kSecondIsTraceable>::Trace(visitor, pair->second);
  }
};

// While using base::Optional<T> with garbage-collected types is generally
// disallowed by the OptionalGarbageCollected check in blink_gc_plugin,
// garbage-collected containers such as HeapVector are allowed and need to be
// traced.
template <typename T>
class TraceTrait<base::Optional<T>> {
  STATIC_ONLY(TraceTrait);

 public:
  template <typename VisitorDispatcher>
  static void Trace(VisitorDispatcher visitor, base::Optional<T>* optional) {
    if (*optional != base::nullopt) {
      TraceIfEnabled<T, WTF::IsTraceable<T>::value>::Trace(visitor,
                                                           optional->value());
    }
  }
};

// If eager tracing leads to excessively deep |trace()| call chains (and
// the system stack usage that this brings), the marker implementation will
// switch to using an explicit mark stack. Recursive and deep object graphs
// are uncommon for Blink objects.
//
// A class type can opt out of eager tracing by declaring a TraceEagerlyTrait<>
// specialization, mapping the trait's |value| to |false| (see the
// WILL_NOT_BE_EAGERLY_TRACED_CLASS() macros below.) For Blink, this is done for
// the small set of GCed classes that are directly recursive.
//
// The TraceEagerlyTrait<T> trait controls whether or not a class
// (and its subclasses) should be eagerly traced or not.
//
// If |TraceEagerlyTrait<T>::value| is |true|, then the marker thread
// should invoke |trace()| on not-yet-marked objects deriving from class T
// right away, and not queue their trace callbacks on its marker stack,
// which it will do if |value| is |false|.
//
// The trait can be declared to enable/disable eager tracing for a class T
// and any of its subclasses, or just to the class T, but none of its
// subclasses.
//
template <typename T>
class TraceEagerlyTrait {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = true;
};

// Disable eager tracing for TYPE, but not any of its subclasses.
#define WILL_NOT_BE_EAGERLY_TRACED_CLASS(TYPE) \
  template <>                                  \
  class TraceEagerlyTrait<TYPE> {              \
    STATIC_ONLY(TraceEagerlyTrait);            \
                                               \
   public:                                     \
    static const bool value = false;           \
  }

template <typename T>
class TraceEagerlyTrait<Member<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<SameThreadCheckedMember<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<TraceWrapperMember<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<WeakMember<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<Persistent<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<WeakPersistent<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<CrossThreadPersistent<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<CrossThreadWeakPersistent<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<HeapTerminatedArray<T>> {
 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename T>
class TraceEagerlyTrait<HeapDoublyLinkedList<T>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = TraceEagerlyTrait<T>::value;
};

template <typename ValueArg, wtf_size_t inlineCapacity>
class HeapListHashSetAllocator;
template <typename T, wtf_size_t inlineCapacity>
class TraceEagerlyTrait<
    WTF::ListHashSetNode<T, HeapListHashSetAllocator<T, inlineCapacity>>> {
  STATIC_ONLY(TraceEagerlyTrait);

 public:
  static const bool value = false;
};

template <typename T>
struct TraceIfNeeded : public TraceIfEnabled<T, WTF::IsTraceable<T>::value> {
  STATIC_ONLY(TraceIfNeeded);
};

}  // namespace blink

namespace WTF {

// Helper used for tracing without weak handling in collections that support
// weak handling. It dispatches to |TraceInCollection| if the provided trait
// supports weak handling, and to |Trace| otherwise.
template <typename T,
          typename Traits,
          WTF::WeakHandlingFlag traitsWeakness = Traits::kWeakHandlingFlag>
struct TraceNoWeakHandlingInCollectionHelper;

template <typename T, typename Traits>
struct TraceNoWeakHandlingInCollectionHelper<T, Traits, kWeakHandling> {
  template <typename VisitorDispatcher>
  static bool Trace(VisitorDispatcher visitor, T& t) {
    return Traits::TraceInCollection(visitor, t, kNoWeakHandling);
  }
};

template <typename T, typename Traits>
struct TraceNoWeakHandlingInCollectionHelper<T, Traits, kNoWeakHandling> {
  template <typename VisitorDispatcher>
  static bool Trace(VisitorDispatcher visitor, T& t) {
    static_assert(IsTraceableInCollectionTrait<Traits>::value,
                  "T should not be traced");
    visitor->Trace(t);
    return false;
  }
};

// Catch-all for types that have a way to trace that don't have special
// handling for weakness in collections.  This means that if this type
// contains WeakMember fields, they will simply be zeroed, but the entry
// will not be removed from the collection.  This always happens for
// things in vectors, which don't currently support special handling of
// weak elements.
template <typename T, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling, T, Traits> {
  static bool IsAlive(T& t) { return true; }

  template <typename VisitorDispatcher>
  static bool Trace(VisitorDispatcher visitor, T& t) {
    return TraceNoWeakHandlingInCollectionHelper<T, Traits>::Trace(visitor, t);
  }
};

// Catch-all for types that have HashTrait support for tracing with weakness.
template <typename T, typename Traits>
struct TraceInCollectionTrait<kWeakHandling, T, Traits> {
  static bool IsAlive(T& value) { return Traits::IsAlive(value); }

  template <typename VisitorDispatcher>
  static bool Trace(VisitorDispatcher visitor, T& t) {
    return Traits::TraceInCollection(visitor, t, kWeakHandling);
  }
};

// This trace method is used only for on-stack HeapVectors found in
// conservative scanning. On-heap HeapVectors are traced by Vector::trace.
template <typename T, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling,
                              blink::HeapVectorBacking<T, Traits>,
                              void> {
  template <typename VisitorDispatcher>
  static bool Trace(VisitorDispatcher visitor, void* self) {
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
          blink::TraceIfEnabled<
              T, IsTraceableInCollectionTrait<Traits>::value>::Trace(visitor,
                                                                     array[i]);
      }
    } else {
      for (size_t i = 0; i < length; ++i)
        blink::TraceIfEnabled<
            T, IsTraceableInCollectionTrait<Traits>::value>::Trace(visitor,
                                                                   array[i]);
    }
    return false;
  }
};

// This trace method is used only for on-stack HeapHashTables found in
// conservative scanning. On-heap HeapHashTables are traced by HashTable::trace.
template <typename Table>
struct TraceInCollectionTrait<kNoWeakHandling,
                              blink::HeapHashTableBacking<Table>,
                              void> {
  using Value = typename Table::ValueType;
  using Traits = typename Table::ValueTraits;

  template <typename VisitorDispatcher>
  static bool Trace(VisitorDispatcher visitor, void* self) {
    static_assert(IsTraceableInCollectionTrait<Traits>::value ||
                      Traits::kWeakHandlingFlag == kWeakHandling,
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
        blink::TraceCollectionIfEnabled<kNoWeakHandling, Value, Traits>::Trace(
            visitor, array[i]);
      }
    }
    return false;
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

  template <typename VisitorDispatcher>
  static bool Trace(VisitorDispatcher visitor, void* self) {
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
  template <typename VisitorDispatcher>
  static bool Trace(VisitorDispatcher visitor, KeyValuePair<Key, Value>& self) {
    static_assert(IsTraceableInCollectionTrait<Traits>::value ||
                      Traits::kWeakHandlingFlag == WTF::kWeakHandling,
                  "T should not be traced");
    blink::TraceCollectionIfEnabled<
        kNoWeakHandling, Key, typename Traits::KeyTraits>::Trace(visitor,
                                                                 self.key);
    blink::TraceCollectionIfEnabled<
        kNoWeakHandling, Value,
        typename Traits::ValueTraits>::Trace(visitor, self.value);
    return false;
  }
};

template <typename Key, typename Value, typename Traits>
struct TraceInCollectionTrait<kWeakHandling, KeyValuePair<Key, Value>, Traits> {
  static constexpr bool kKeyIsWeak =
      Traits::KeyTraits::kWeakHandlingFlag == kWeakHandling;
  static constexpr bool kValueIsWeak =
      Traits::ValueTraits::kWeakHandlingFlag == kWeakHandling;
  static const bool kKeyHasStrongRefs =
      IsTraceableInCollectionTrait<typename Traits::KeyTraits>::value;
  static const bool kValueHasStrongRefs =
      IsTraceableInCollectionTrait<typename Traits::ValueTraits>::value;

  static bool IsAlive(KeyValuePair<Key, Value>& self) {
    static_assert(!kKeyIsWeak || !kValueIsWeak || !kKeyHasStrongRefs ||
                      !kValueHasStrongRefs,
                  "this configuration is disallowed to avoid unexpected leaks");
    if ((kValueIsWeak && !kKeyIsWeak) ||
        (kValueIsWeak && kKeyIsWeak && !kValueHasStrongRefs)) {
      // Check value first.
      bool value_side_alive = blink::TraceCollectionIfEnabled<
          Traits::ValueTraits::kWeakHandlingFlag, Value,
          typename Traits::ValueTraits>::IsAlive(self.value);
      if (!value_side_alive)
        return false;
      return blink::TraceCollectionIfEnabled<
          Traits::KeyTraits::kWeakHandlingFlag, Key,
          typename Traits::KeyTraits>::IsAlive(self.key);
    }
    // Check key first.
    bool key_side_alive = blink::TraceCollectionIfEnabled<
        Traits::KeyTraits::kWeakHandlingFlag, Key,
        typename Traits::KeyTraits>::IsAlive(self.key);
    if (!key_side_alive)
      return false;
    return blink::TraceCollectionIfEnabled<
        Traits::ValueTraits::kWeakHandlingFlag, Value,
        typename Traits::ValueTraits>::IsAlive(self.value);
  }

  template <typename VisitorDispatcher>
  static bool Trace(VisitorDispatcher visitor, KeyValuePair<Key, Value>& self) {
    // This is the core of the ephemeron-like functionality.  If there is
    // weakness on the key side then we first check whether there are
    // dead weak pointers on that side, and if there are we don't mark the
    // value side (yet).  Conversely if there is weakness on the value side
    // we check that first and don't mark the key side yet if we find dead
    // weak pointers.
    // Corner case: If there is weakness on both the key and value side,
    // and there are also strong pointers on the both sides then we could
    // unexpectedly leak.  The scenario is that the weak pointer on the key
    // side is alive, which causes the strong pointer on the key side to be
    // marked.  If that then results in the object pointed to by the weak
    // pointer on the value side being marked live, then the whole
    // key-value entry is leaked.  To avoid unexpected leaking, we disallow
    // this case, but if you run into this assert, please reach out to Blink
    // reviewers, and we may relax it.
    static_assert(!kKeyIsWeak || !kValueIsWeak || !kKeyHasStrongRefs ||
                      !kValueHasStrongRefs,
                  "this configuration is disallowed to avoid unexpected leaks");
    if ((kValueIsWeak && !kKeyIsWeak) ||
        (kValueIsWeak && kKeyIsWeak && !kValueHasStrongRefs)) {
      // Check value first.
      bool dead_weak_objects_found_on_value_side =
          blink::TraceCollectionIfEnabled<
              Traits::ValueTraits::kWeakHandlingFlag, Value,
              typename Traits::ValueTraits>::Trace(visitor, self.value);
      if (dead_weak_objects_found_on_value_side)
        return true;
      return blink::TraceCollectionIfEnabled<
          Traits::KeyTraits::kWeakHandlingFlag, Key,
          typename Traits::KeyTraits>::Trace(visitor, self.key);
    }
    // Check key first.
    bool dead_weak_objects_found_on_key_side = blink::TraceCollectionIfEnabled<
        Traits::KeyTraits::kWeakHandlingFlag, Key,
        typename Traits::KeyTraits>::Trace(visitor, self.key);
    if (dead_weak_objects_found_on_key_side)
      return true;
    return blink::TraceCollectionIfEnabled<
        Traits::ValueTraits::kWeakHandlingFlag, Value,
        typename Traits::ValueTraits>::Trace(visitor, self.value);
  }
};

// Nodes used by LinkedHashSet.  Again we need two versions to disambiguate the
// template.
template <typename Value, typename Allocator, typename Traits>
struct TraceInCollectionTrait<kNoWeakHandling,
                              LinkedHashSetNode<Value, Allocator>,
                              Traits> {
  static bool IsAlive(LinkedHashSetNode<Value, Allocator>& self) {
    return TraceInCollectionTrait<
        kNoWeakHandling, Value,
        typename Traits::ValueTraits>::IsAlive(self.value_);
  }

  template <typename VisitorDispatcher>
  static bool Trace(VisitorDispatcher visitor,
                    LinkedHashSetNode<Value, Allocator>& self) {
    static_assert(IsTraceableInCollectionTrait<Traits>::value ||
                      Traits::kWeakHandlingFlag == WTF::kWeakHandling,
                  "T should not be traced");
    return TraceInCollectionTrait<
        kNoWeakHandling, Value,
        typename Traits::ValueTraits>::Trace(visitor, self.value_);
  }
};

template <typename Value, typename Allocator, typename Traits>
struct TraceInCollectionTrait<kWeakHandling,
                              LinkedHashSetNode<Value, Allocator>,
                              Traits> {
  static bool IsAlive(LinkedHashSetNode<Value, Allocator>& self) {
    return TraceInCollectionTrait<
        kWeakHandling, Value,
        typename Traits::ValueTraits>::IsAlive(self.value_);
  }

  template <typename VisitorDispatcher>
  static bool Trace(VisitorDispatcher visitor,
                    LinkedHashSetNode<Value, Allocator>& self) {
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

  template <typename VisitorDispatcher>
  static bool Trace(VisitorDispatcher visitor, Node* node) {
    static_assert(IsTraceableInCollectionTrait<Traits>::value ||
                      Traits::kWeakHandlingFlag == WTF::kWeakHandling,
                  "T should not be traced");
    visitor->Trace(node);
    return false;
  }
};

}  // namespace WTF

#endif

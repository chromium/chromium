// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_ALLOCATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_ALLOCATOR_H_

#include <type_traits>

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_buildflags.h"
#include "third_party/blink/renderer/platform/heap/marking_visitor.h"
#include "third_party/blink/renderer/platform/heap/thread_state_scopes.h"
#include "third_party/blink/renderer/platform/heap/trace_traits.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/construct_traits.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/doubly_linked_list.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/hash_table.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/list_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

#define DISALLOW_IN_CONTAINER()              \
 public:                                     \
  using IsDisallowedInContainerMarker = int; \
                                             \
 private:                                    \
  friend class ::WTF::internal::__thisIsHereToForceASemicolonAfterThisMacro

// IsAllowedInContainer returns true if some type T supports being nested
// arbitrarily in other containers. This is relevant for collections where some
// collections assume that they are placed on a non-moving arena.
template <typename T, typename = int>
struct IsAllowedInContainer : std::true_type {};
template <typename T>
struct IsAllowedInContainer<T, typename T::IsDisallowedInContainerMarker>
    : std::false_type {};

template <typename T, typename Traits = WTF::VectorTraits<T>>
class HeapVectorBacking {
  DISALLOW_NEW();
  IS_GARBAGE_COLLECTED_TYPE();

 public:
  static void Finalize(void* pointer);
  void FinalizeGarbageCollectedObject() { Finalize(this); }
};

template <typename Table>
class HeapHashTableBacking {
  DISALLOW_NEW();
  IS_GARBAGE_COLLECTED_TYPE();

 public:
  static void Finalize(void* pointer);
  void FinalizeGarbageCollectedObject() { Finalize(this); }
};

// This is a static-only class used as a trait on collections to make them heap
// allocated.  However see also HeapListHashSetAllocator.
class PLATFORM_EXPORT HeapAllocator {
  STATIC_ONLY(HeapAllocator);

 public:
  using WeakCallbackInfo = blink::WeakCallbackInfo;
  using Visitor = blink::Visitor;
  static constexpr bool kIsGarbageCollected = true;

  template <typename T>
  static size_t MaxElementCountInBackingStore() {
    return kMaxHeapObjectSize / sizeof(T);
  }

  template <typename T>
  static size_t QuantizedSize(size_t count) {
    CHECK(count <= MaxElementCountInBackingStore<T>());
    return ThreadHeap::AllocationSizeFromSize(count * sizeof(T)) -
           sizeof(HeapObjectHeader);
  }
  template <typename T>
  static T* AllocateVectorBacking(size_t size) {
    ThreadState* state =
        ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
    DCHECK(state->IsAllocationAllowed());
    uint32_t gc_info_index = GCInfoTrait<HeapVectorBacking<T>>::Index();
    const char* type_name =
        WTF_HEAP_PROFILER_TYPE_NAME(HeapHashTableBacking<HeapVectorBacking<T>>);
    return reinterpret_cast<T*>(
        MarkAsConstructed(state->Heap().AllocateOnArenaIndex(
            state, ThreadHeap::AllocationSizeFromSize(size),
            BlinkGC::kVectorArenaIndex, gc_info_index, type_name)));
  }
  static void FreeVectorBacking(void*);
  static bool ExpandVectorBacking(void*, size_t);
  static bool ShrinkVectorBacking(void* address,
                                  size_t quantized_current_size,
                                  size_t quantized_shrunk_size);

  template <typename T, typename HashTable>
  static T* AllocateHashTableBacking(size_t size) {
    uint32_t gc_info_index =
        GCInfoTrait<HeapHashTableBacking<HashTable>>::Index();
    ThreadState* state =
        ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState();
    const char* type_name =
        WTF_HEAP_PROFILER_TYPE_NAME(HeapHashTableBacking<HashTable>);
    return reinterpret_cast<T*>(
        MarkAsConstructed(state->Heap().AllocateOnArenaIndex(
            state, size, BlinkGC::kHashTableArenaIndex, gc_info_index,
            type_name)));
  }
  template <typename T, typename HashTable>
  static T* AllocateZeroedHashTableBacking(size_t size) {
    return AllocateHashTableBacking<T, HashTable>(size);
  }
  static void FreeHashTableBacking(void* address);
  static bool ExpandHashTableBacking(void*, size_t);

  static void TraceMarkedBackingStore(void* address) {
    MarkingVisitor::TraceMarkedBackingStore(address);
  }

  static void BackingWriteBarrier(void* address) {
    MarkingVisitor::WriteBarrier(address);
  }

  template <typename HashTable>
  static void BackingWriteBarrierForHashTable(void* address) {
    if (MarkingVisitor::WriteBarrier(address)) {
      AddMovingCallback<HashTable>(
          static_cast<typename HashTable::ValueType*>(address));
    }
  }

  template <typename Return, typename Metadata>
  static Return Malloc(size_t size, const char* type_name) {
    return reinterpret_cast<Return>(
        MarkAsConstructed(ThreadHeap::Allocate<Metadata>(size)));
  }

  // Compilers sometimes eagerly instantiates the unused 'operator delete', so
  // we provide a version that asserts and fails at run-time if used.
  static void Free(void*) { NOTREACHED(); }

  template <typename T>
  static void* NewArray(size_t bytes) {
    NOTREACHED();
    return nullptr;
  }

  static void DeleteArray(void* ptr) { NOTREACHED(); }

  static bool IsAllocationAllowed() {
    return ThreadState::Current()->IsAllocationAllowed();
  }

  static bool IsSweepForbidden() {
    return ThreadState::Current()->SweepForbidden();
  }

  template <typename T>
  static bool IsHeapObjectAlive(T* object) {
    return ThreadHeap::IsHeapObjectAlive(object);
  }

  template <typename T, typename Traits>
  static void Trace(blink::Visitor* visitor, T& t) {
    TraceCollectionIfEnabled<WTF::WeakHandlingTrait<T>::value, T,
                             Traits>::Trace(visitor, &t);
  }

  template <typename T, typename VisitorDispatcher>
  static void RegisterBackingStoreCallback(VisitorDispatcher visitor,
                                           T* backing,
                                           MovingObjectCallback callback) {
    visitor->RegisterBackingStoreCallback(backing, callback);
  }

  static void EnterGCForbiddenScope() {
    ThreadState::Current()->EnterGCForbiddenScope();
  }

  static void LeaveGCForbiddenScope() {
    ThreadState::Current()->LeaveGCForbiddenScope();
  }

  template <typename T, typename Traits>
  static void NotifyNewObject(T* object) {
    if (!ThreadState::IsAnyIncrementalMarking())
      return;
    // The object may have been in-place constructed as part of a large object.
    // It is not safe to retrieve the page from the object here.
    ThreadState* const thread_state = ThreadState::Current();
    if (thread_state->IsIncrementalMarking()) {
      // Eagerly trace the object ensuring that the object and all its children
      // are discovered by the marker.
      ThreadState::NoAllocationScope no_allocation_scope(thread_state);
      DCHECK(thread_state->CurrentVisitor());
      // No weak handling for write barriers. Modifying weakly reachable objects
      // strongifies them for the current cycle.
      DCHECK(!Traits::kCanHaveDeletedValue || !Traits::IsDeletedValue(*object));
      TraceCollectionIfEnabled<WTF::kNoWeakHandling, T, Traits>::Trace(
          thread_state->CurrentVisitor(), object);
    }
  }

  template <typename T, typename Traits>
  static void NotifyNewObjects(T* array, size_t len) {
    if (!ThreadState::IsAnyIncrementalMarking())
      return;
    // The object may have been in-place constructed as part of a large object.
    // It is not safe to retrieve the page from the object here.
    ThreadState* const thread_state = ThreadState::Current();
    if (thread_state->IsIncrementalMarking()) {
      // See |NotifyNewObject| for details.
      ThreadState::NoAllocationScope no_allocation_scope(thread_state);
      DCHECK(thread_state->CurrentVisitor());
      // No weak handling for write barriers. Modifying weakly reachable objects
      // strongifies them for the current cycle.
      while (len-- > 0) {
        DCHECK(!Traits::kCanHaveDeletedValue ||
               !Traits::IsDeletedValue(*array));
        TraceCollectionIfEnabled<WTF::kNoWeakHandling, T, Traits>::Trace(
            thread_state->CurrentVisitor(), array);
        array++;
      }
    }
  }

  template <typename T>
  static void TraceVectorBacking(Visitor* visitor,
                                 T* backing,
                                 T** backing_slot) {
    visitor->TraceBackingStoreStrongly(
        reinterpret_cast<HeapVectorBacking<T>*>(backing),
        reinterpret_cast<HeapVectorBacking<T>**>(backing_slot));
  }

  template <typename T, typename HashTable>
  static void TraceHashTableBackingStrongly(Visitor* visitor,
                                            T* backing,
                                            T** backing_slot) {
    visitor->TraceBackingStoreStrongly(
        reinterpret_cast<HeapHashTableBacking<HashTable>*>(backing),
        reinterpret_cast<HeapHashTableBacking<HashTable>**>(backing_slot));
  }

  template <typename T, typename HashTable>
  static void TraceHashTableBackingWeakly(Visitor* visitor,
                                          T* backing,
                                          T** backing_slot,
                                          WeakCallback callback,
                                          void* parameter) {
    visitor->TraceBackingStoreWeakly<HashTable>(
        reinterpret_cast<HeapHashTableBacking<HashTable>*>(backing),
        reinterpret_cast<HeapHashTableBacking<HashTable>**>(backing_slot),
        callback, parameter);
  }

  template <typename T, typename HashTable>
  static void TraceHashTableBackingOnly(Visitor* visitor,
                                        T* backing,
                                        T** backing_slot) {
    visitor->TraceBackingStoreOnly(
        reinterpret_cast<HeapHashTableBacking<HashTable>*>(backing),
        reinterpret_cast<HeapHashTableBacking<HashTable>**>(backing_slot));
  }

 private:
  static Address MarkAsConstructed(Address address) {
    HeapObjectHeader::FromPayload(reinterpret_cast<void*>(address))
        ->MarkFullyConstructed<HeapObjectHeader::AccessMode::kAtomic>();
    return address;
  }

  template <
      typename HashTable,
      std::enable_if_t<HashTable::ValueTraits::kHasMovingCallback>* = nullptr>
  static void AddMovingCallback(typename HashTable::ValueType* memory) {
    ThreadState* thread_state = ThreadState::Current();
    auto* visitor = thread_state->CurrentVisitor();
    DCHECK(visitor);
    HashTable::ValueTraits::template RegisterMovingCallback<HashTable>(visitor,
                                                                       memory);
  }

  template <
      typename HashTable,
      std::enable_if_t<!HashTable::ValueTraits::kHasMovingCallback>* = nullptr>
  static void AddMovingCallback(typename HashTable::ValueType*) {}

  static void BackingFree(void*);
  static bool BackingExpand(void*, size_t);
  static bool BackingShrink(void*,
                            size_t quantized_current_size,
                            size_t quantized_shrunk_size);

  template <typename T, wtf_size_t u, typename V>
  friend class WTF::Vector;
  template <typename T, typename U, typename V, typename W>
  friend class WTF::HashSet;
  template <typename T,
            typename U,
            typename V,
            typename W,
            typename X,
            typename Y>
  friend class WTF::HashMap;
};

template <typename VisitorDispatcher, typename Value>
static void TraceListHashSetValue(VisitorDispatcher visitor, Value& value) {
  // We use the default hash traits for the value in the node, because
  // ListHashSet does not let you specify any specific ones.
  // We don't allow ListHashSet of WeakMember, so we set that one false
  // (there's an assert elsewhere), but we have to specify some value for the
  // strongify template argument, so we specify WTF::WeakPointersActWeak,
  // arbitrarily.
  TraceCollectionIfEnabled<WTF::kNoWeakHandling, Value,
                           WTF::HashTraits<Value>>::Trace(visitor, &value);
}

// The inline capacity is just a dummy template argument to match the off-heap
// allocator.
// This inherits from the static-only HeapAllocator trait class, but we do
// declare pointers to instances.  These pointers are always null, and no
// objects are instantiated.
template <typename ValueArg, wtf_size_t inlineCapacity>
class HeapListHashSetAllocator : public HeapAllocator {
  DISALLOW_NEW();

 public:
  using TableAllocator = HeapAllocator;
  using Node = WTF::ListHashSetNode<ValueArg, HeapListHashSetAllocator>;

  class AllocatorProvider {
    DISALLOW_NEW();

   public:
    // For the heap allocation we don't need an actual allocator object, so
    // we just return null.
    HeapListHashSetAllocator* Get() const { return nullptr; }

    // No allocator object is needed.
    void CreateAllocatorIfNeeded() {}
    void ReleaseAllocator() {}

    // There is no allocator object in the HeapListHashSet (unlike in the
    // regular ListHashSet) so there is nothing to swap.
    void Swap(AllocatorProvider& other) {}
  };

  void Deallocate(void* dummy) {}

  // This is not a static method even though it could be, because it needs to
  // match the one that the (off-heap) ListHashSetAllocator has.  The 'this'
  // pointer will always be null.
  void* AllocateNode() {
    // Consider using a LinkedHashSet instead if this compile-time assert fails:
    static_assert(!WTF::IsWeak<ValueArg>::value,
                  "weak pointers in a ListHashSet will result in null entries "
                  "in the set");

    return Malloc<void*, Node>(
        sizeof(Node),
        nullptr /* Oilpan does not use the heap profiler at the moment. */);
  }

  template <typename VisitorDispatcher>
  static void TraceValue(VisitorDispatcher visitor, Node* node) {
    TraceListHashSetValue(visitor, node->value_);
  }
};

template <typename T, typename Traits>
void HeapVectorBacking<T, Traits>::Finalize(void* pointer) {
  static_assert(Traits::kNeedsDestruction,
                "Only vector buffers with items requiring destruction should "
                "be finalized");
  // See the comment in HeapVectorBacking::trace.
  static_assert(
      Traits::kCanClearUnusedSlotsWithMemset || std::is_polymorphic<T>::value,
      "HeapVectorBacking doesn't support objects that cannot be cleared as "
      "unused with memset or don't have a vtable");

  static_assert(
      !std::is_trivially_destructible<T>::value,
      "Finalization of trivially destructible classes should not happen.");
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(pointer);
  // Use the payload size as recorded by the heap to determine how many
  // elements to finalize.
  size_t length = header->PayloadSize() / sizeof(T);
  char* payload = static_cast<char*>(pointer);
#ifdef ANNOTATE_CONTIGUOUS_CONTAINER
  ANNOTATE_CHANGE_SIZE(payload, length * sizeof(T), 0, length * sizeof(T));
#endif
  // As commented above, HeapVectorBacking calls finalizers for unused slots
  // (which are already zeroed out).
  if (std::is_polymorphic<T>::value) {
    for (unsigned i = 0; i < length; ++i) {
      char* element = payload + i * sizeof(T);
      if (blink::VTableInitialized(element))
        reinterpret_cast<T*>(element)->~T();
    }
  } else {
    T* buffer = reinterpret_cast<T*>(payload);
    for (unsigned i = 0; i < length; ++i)
      buffer[i].~T();
  }
}

template <typename Table>
void HeapHashTableBacking<Table>::Finalize(void* pointer) {
  using Value = typename Table::ValueType;
  static_assert(
      !std::is_trivially_destructible<Value>::value,
      "Finalization of trivially destructible classes should not happen.");
  HeapObjectHeader* header = HeapObjectHeader::FromPayload(pointer);
  // Use the payload size as recorded by the heap to determine how many
  // elements to finalize.
  size_t length = header->PayloadSize() / sizeof(Value);
  Value* table = reinterpret_cast<Value*>(pointer);
  for (unsigned i = 0; i < length; ++i) {
    if (!Table::IsEmptyOrDeletedBucket(table[i]))
      table[i].~Value();
  }
}

template <typename KeyArg,
          typename MappedArg,
          typename HashArg = typename DefaultHash<KeyArg>::Hash,
          typename KeyTraitsArg = HashTraits<KeyArg>,
          typename MappedTraitsArg = HashTraits<MappedArg>>
class HeapHashMap : public HashMap<KeyArg,
                                   MappedArg,
                                   HashArg,
                                   KeyTraitsArg,
                                   MappedTraitsArg,
                                   HeapAllocator> {
  IS_GARBAGE_COLLECTED_CONTAINER_TYPE();
  DISALLOW_NEW();

  static void CheckType() {
    static_assert(std::is_trivially_destructible<HeapHashMap>::value,
                  "HeapHashMap must be trivially destructible.");
    static_assert(
        IsAllowedInContainer<KeyArg>::value,
        "Not allowed to directly nest type. Use Member<> indirection instead.");
    static_assert(
        IsAllowedInContainer<MappedArg>::value,
        "Not allowed to directly nest type. Use Member<> indirection instead.");
    static_assert(
        WTF::IsTraceable<KeyArg>::value || WTF::IsTraceable<MappedArg>::value,
        "For hash maps without traceable elements, use HashMap<> "
        "instead of HeapHashMap<>.");
  }

 public:
  template <typename>
  static void* AllocateObject(size_t size) {
    return ThreadHeap::Allocate<
        HeapHashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg>>(
        size);
  }

  HeapHashMap() { CheckType(); }
};

template <typename ValueArg,
          typename HashArg = typename DefaultHash<ValueArg>::Hash,
          typename TraitsArg = HashTraits<ValueArg>>
class HeapHashSet
    : public HashSet<ValueArg, HashArg, TraitsArg, HeapAllocator> {
  IS_GARBAGE_COLLECTED_CONTAINER_TYPE();
  DISALLOW_NEW();

  static void CheckType() {
    static_assert(std::is_trivially_destructible<HeapHashSet>::value,
                  "HeapHashSet must be trivially destructible.");
    static_assert(
        IsAllowedInContainer<ValueArg>::value,
        "Not allowed to directly nest type. Use Member<> indirection instead.");
    static_assert(WTF::IsTraceable<ValueArg>::value,
                  "For hash sets without traceable elements, use HashSet<> "
                  "instead of HeapHashSet<>.");
  }

 public:
  template <typename>
  static void* AllocateObject(size_t size) {
    return ThreadHeap::Allocate<HeapHashSet<ValueArg, HashArg, TraitsArg>>(
        size);
  }

  HeapHashSet() { CheckType(); }
};

template <typename ValueArg,
          typename HashArg = typename DefaultHash<ValueArg>::Hash,
          typename TraitsArg = HashTraits<ValueArg>>
class HeapLinkedHashSet
    : public LinkedHashSet<ValueArg, HashArg, TraitsArg, HeapAllocator> {
  IS_GARBAGE_COLLECTED_CONTAINER_TYPE();
  DISALLOW_NEW();
  // HeapLinkedHashSet is using custom callbacks for compaction that rely on the
  // fact that the container itself does not move.
  DISALLOW_IN_CONTAINER();

  static void CheckType() {
    static_assert(
        IsAllowedInContainer<ValueArg>::value,
        "Not allowed to directly nest type. Use Member<> indirection instead.");
    static_assert(WTF::IsTraceable<ValueArg>::value,
                  "For sets without traceable elements, use LinkedHashSet<> "
                  "instead of HeapLinkedHashSet<>.");
  }

 public:
  template <typename>
  static void* AllocateObject(size_t size) {
    return ThreadHeap::Allocate<
        HeapLinkedHashSet<ValueArg, HashArg, TraitsArg>>(size);
  }

  HeapLinkedHashSet() { CheckType(); }
};

template <typename ValueArg,
          wtf_size_t inlineCapacity =
              0,  // The inlineCapacity is just a dummy to
                  // match ListHashSet (off-heap).
          typename HashArg = typename DefaultHash<ValueArg>::Hash>
class HeapListHashSet
    : public ListHashSet<ValueArg,
                         inlineCapacity,
                         HashArg,
                         HeapListHashSetAllocator<ValueArg, inlineCapacity>> {
  IS_GARBAGE_COLLECTED_CONTAINER_TYPE();
  DISALLOW_NEW();

  static void CheckType() {
    static_assert(std::is_trivially_destructible<HeapListHashSet>::value,
                  "HeapListHashSet must be trivially destructible.");
    static_assert(
        IsAllowedInContainer<ValueArg>::value,
        "Not allowed to directly nest type. Use Member<> indirection instead.");
    static_assert(WTF::IsTraceable<ValueArg>::value,
                  "For sets without traceable elements, use ListHashSet<> "
                  "instead of HeapListHashSet<>.");
  }

 public:
  template <typename>
  static void* AllocateObject(size_t size) {
    return ThreadHeap::Allocate<
        HeapListHashSet<ValueArg, inlineCapacity, HashArg>>(size);
  }

  HeapListHashSet() { CheckType(); }
};

template <typename Value,
          typename HashFunctions = typename DefaultHash<Value>::Hash,
          typename Traits = HashTraits<Value>>
class HeapHashCountedSet
    : public HashCountedSet<Value, HashFunctions, Traits, HeapAllocator> {
  IS_GARBAGE_COLLECTED_CONTAINER_TYPE();
  DISALLOW_NEW();

  static void CheckType() {
    static_assert(std::is_trivially_destructible<HeapHashCountedSet>::value,
                  "HeapHashCountedSet must be trivially destructible.");
    static_assert(
        IsAllowedInContainer<Value>::value,
        "Not allowed to directly nest type. Use Member<> indirection instead.");
    static_assert(WTF::IsTraceable<Value>::value,
                  "For counted sets without traceable elements, use "
                  "HashCountedSet<> instead of HeapHashCountedSet<>.");
  }

 public:
  template <typename>
  static void* AllocateObject(size_t size) {
    return ThreadHeap::Allocate<
        HeapHashCountedSet<Value, HashFunctions, Traits>>(size);
  }

  HeapHashCountedSet() { CheckType(); }
};

template <typename T, wtf_size_t inlineCapacity = 0>
class HeapVector : public Vector<T, inlineCapacity, HeapAllocator> {
  IS_GARBAGE_COLLECTED_CONTAINER_TYPE();
  DISALLOW_NEW();

  static void CheckType() {
    static_assert(
        std::is_trivially_destructible<HeapVector>::value || inlineCapacity,
        "HeapVector must be trivially destructible.");
    static_assert(
        IsAllowedInContainer<T>::value,
        "Not allowed to directly nest type. Use Member<> indirection instead.");
    static_assert(WTF::IsTraceable<T>::value,
                  "For vectors without traceable elements, use Vector<> "
                  "instead of HeapVector<>.");
  }

 public:
  template <typename>
  static void* AllocateObject(size_t size) {
    // On-heap HeapVectors generally should not have inline capacity, but it is
    // hard to avoid when using a type alias. Hence we only disallow the
    // VectorTraits<T>::kNeedsDestruction case for now.
    static_assert(inlineCapacity == 0 || !VectorTraits<T>::kNeedsDestruction,
                  "on-heap HeapVector<> should not have an inline capacity");
    return ThreadHeap::Allocate<HeapVector<T, inlineCapacity>>(size);
  }

  HeapVector() { CheckType(); }

  explicit HeapVector(wtf_size_t size)
      : Vector<T, inlineCapacity, HeapAllocator>(size) {
    CheckType();
  }

  HeapVector(wtf_size_t size, const T& val)
      : Vector<T, inlineCapacity, HeapAllocator>(size, val) {
    CheckType();
  }

  template <wtf_size_t otherCapacity>
  HeapVector(const HeapVector<T, otherCapacity>& other)
      : Vector<T, inlineCapacity, HeapAllocator>(other) {
    CheckType();
  }

  HeapVector(std::initializer_list<T> elements)
      : Vector<T, inlineCapacity, HeapAllocator>(elements) {
    CheckType();
  }
};

template <typename T, wtf_size_t inlineCapacity = 0>
class HeapDeque : public Deque<T, inlineCapacity, HeapAllocator> {
  IS_GARBAGE_COLLECTED_CONTAINER_TYPE();
  DISALLOW_NEW();

  static void CheckType() {
    static_assert(
        std::is_trivially_destructible<HeapDeque>::value || inlineCapacity,
        "HeapDeque must be trivially destructible.");
    static_assert(
        IsAllowedInContainer<T>::value,
        "Not allowed to directly nest type. Use Member<> indirection instead.");
    static_assert(WTF::IsTraceable<T>::value,
                  "For vectors without traceable elements, use Deque<> instead "
                  "of HeapDeque<>");
  }

 public:
  template <typename>
  static void* AllocateObject(size_t size) {
    // On-heap HeapDeques generally should not have inline capacity, but it is
    // hard to avoid when using a type alias. Hence we only disallow the
    // VectorTraits<T>::kNeedsDestruction case for now.
    static_assert(inlineCapacity == 0 || !VectorTraits<T>::kNeedsDestruction,
                  "on-heap HeapDeque<> should not have an inline capacity");
    return ThreadHeap::Allocate<HeapDeque<T, inlineCapacity>>(size);
  }

  HeapDeque() { CheckType(); }

  explicit HeapDeque(wtf_size_t size)
      : Deque<T, inlineCapacity, HeapAllocator>(size) {
    CheckType();
  }

  HeapDeque(wtf_size_t size, const T& val)
      : Deque<T, inlineCapacity, HeapAllocator>(size, val) {
    CheckType();
  }

  HeapDeque& operator=(const HeapDeque& other) {
    HeapDeque<T> copy(other);
    Deque<T, inlineCapacity, HeapAllocator>::Swap(copy);
    return *this;
  }

  template <wtf_size_t otherCapacity>
  HeapDeque(const HeapDeque<T, otherCapacity>& other)
      : Deque<T, inlineCapacity, HeapAllocator>(other) {}
};

}  // namespace blink

namespace WTF {

template <typename T>
struct VectorTraits<blink::Member<T>> : VectorTraitsBase<blink::Member<T>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanCopyWithMemcpy = true;
  static const bool kCanMoveWithMemcpy = true;
};

template <typename T>
struct VectorTraits<blink::WeakMember<T>>
    : VectorTraitsBase<blink::WeakMember<T>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
};

template <typename T>
struct VectorTraits<blink::UntracedMember<T>>
    : VectorTraitsBase<blink::UntracedMember<T>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
};

template <typename T>
struct VectorTraits<blink::HeapVector<T, 0>>
    : VectorTraitsBase<blink::HeapVector<T, 0>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
};

template <typename T>
struct VectorTraits<blink::HeapDeque<T, 0>>
    : VectorTraitsBase<blink::HeapDeque<T, 0>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
};

template <typename T, wtf_size_t inlineCapacity>
struct VectorTraits<blink::HeapVector<T, inlineCapacity>>
    : VectorTraitsBase<blink::HeapVector<T, inlineCapacity>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = VectorTraits<T>::kNeedsDestruction;
  static const bool kCanInitializeWithMemset =
      VectorTraits<T>::kCanInitializeWithMemset;
  static const bool kCanClearUnusedSlotsWithMemset =
      VectorTraits<T>::kCanClearUnusedSlotsWithMemset;
  static const bool kCanMoveWithMemcpy = VectorTraits<T>::kCanMoveWithMemcpy;
};

template <typename T, wtf_size_t inlineCapacity>
struct VectorTraits<blink::HeapDeque<T, inlineCapacity>>
    : VectorTraitsBase<blink::HeapDeque<T, inlineCapacity>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = VectorTraits<T>::kNeedsDestruction;
  static const bool kCanInitializeWithMemset =
      VectorTraits<T>::kCanInitializeWithMemset;
  static const bool kCanClearUnusedSlotsWithMemset =
      VectorTraits<T>::kCanClearUnusedSlotsWithMemset;
  static const bool kCanMoveWithMemcpy = VectorTraits<T>::kCanMoveWithMemcpy;
};

template <typename T>
struct HashTraits<blink::Member<T>> : SimpleClassHashTraits<blink::Member<T>> {
  STATIC_ONLY(HashTraits);
  // FIXME: Implement proper const'ness for iterator types. Requires support
  // in the marking Visitor.
  using PeekInType = T*;
  using IteratorGetType = blink::Member<T>*;
  using IteratorConstGetType = const blink::Member<T>*;
  using IteratorReferenceType = blink::Member<T>&;
  using IteratorConstReferenceType = const blink::Member<T>&;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

  using PeekOutType = T*;

  template <typename U>
  static void Store(const U& value, blink::Member<T>& storage) {
    storage = value;
  }

  static PeekOutType Peek(const blink::Member<T>& value) { return value; }

  static void ConstructDeletedValue(blink::Member<T>& slot, bool) {
    slot = WTF::kHashTableDeletedValue;
  }
  static bool IsDeletedValue(const blink::Member<T>& value) {
    return value.IsHashTableDeletedValue();
  }
};

template <typename T>
struct HashTraits<blink::WeakMember<T>>
    : SimpleClassHashTraits<blink::WeakMember<T>> {
  STATIC_ONLY(HashTraits);
  static const bool kNeedsDestruction = false;
  // FIXME: Implement proper const'ness for iterator types. Requires support
  // in the marking Visitor.
  using PeekInType = T*;
  using IteratorGetType = blink::WeakMember<T>*;
  using IteratorConstGetType = const blink::WeakMember<T>*;
  using IteratorReferenceType = blink::WeakMember<T>&;
  using IteratorConstReferenceType = const blink::WeakMember<T>&;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

  using PeekOutType = T*;

  template <typename U>
  static void Store(const U& value, blink::WeakMember<T>& storage) {
    storage = value;
  }

  static PeekOutType Peek(const blink::WeakMember<T>& value) { return value; }
};

template <typename T>
struct HashTraits<blink::UntracedMember<T>>
    : SimpleClassHashTraits<blink::UntracedMember<T>> {
  STATIC_ONLY(HashTraits);
  static const bool kNeedsDestruction = false;
  // FIXME: Implement proper const'ness for iterator types.
  using PeekInType = T*;
  using IteratorGetType = blink::UntracedMember<T>*;
  using IteratorConstGetType = const blink::UntracedMember<T>*;
  using IteratorReferenceType = blink::UntracedMember<T>&;
  using IteratorConstReferenceType = const blink::UntracedMember<T>&;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }
  using PeekOutType = T*;

  template <typename U>
  static void Store(const U& value, blink::UntracedMember<T>& storage) {
    storage = value;
  }

  static PeekOutType Peek(const blink::UntracedMember<T>& value) {
    return value;
  }
};

template <typename T, wtf_size_t inlineCapacity>
struct IsTraceable<
    ListHashSetNode<T, blink::HeapListHashSetAllocator<T, inlineCapacity>>*> {
  STATIC_ONLY(IsTraceable);
  static_assert(sizeof(T), "T must be fully defined");
  // All heap allocated node pointers need visiting to keep the nodes alive,
  // regardless of whether they contain pointers to other heap allocated
  // objects.
  static const bool value = true;
};

template <typename T, wtf_size_t inlineCapacity>
struct IsGarbageCollectedType<
    ListHashSetNode<T, blink::HeapListHashSetAllocator<T, inlineCapacity>>> {
  static const bool value = true;
};

template <typename Set>
struct IsGarbageCollectedType<ListHashSetIterator<Set>> {
  static const bool value = IsGarbageCollectedType<Set>::value;
};

template <typename Set>
struct IsGarbageCollectedType<ListHashSetConstIterator<Set>> {
  static const bool value = IsGarbageCollectedType<Set>::value;
};

template <typename Set>
struct IsGarbageCollectedType<ListHashSetReverseIterator<Set>> {
  static const bool value = IsGarbageCollectedType<Set>::value;
};

template <typename Set>
struct IsGarbageCollectedType<ListHashSetConstReverseIterator<Set>> {
  static const bool value = IsGarbageCollectedType<Set>::value;
};

template <typename T, typename H>
struct HandleHashTraits : SimpleClassHashTraits<H> {
  STATIC_ONLY(HandleHashTraits);
  // TODO: Implement proper const'ness for iterator types. Requires support
  // in the marking Visitor.
  using PeekInType = T*;
  using IteratorGetType = H*;
  using IteratorConstGetType = const H*;
  using IteratorReferenceType = H&;
  using IteratorConstReferenceType = const H&;
  static IteratorReferenceType GetToReferenceConversion(IteratorGetType x) {
    return *x;
  }
  static IteratorConstReferenceType GetToReferenceConstConversion(
      IteratorConstGetType x) {
    return *x;
  }

  using PeekOutType = T*;

  template <typename U>
  static void Store(const U& value, H& storage) {
    storage = value;
  }

  static PeekOutType Peek(const H& value) { return value; }
};

template <typename Value,
          typename HashFunctions,
          typename Traits,
          typename VectorType>
inline void CopyToVector(
    const blink::HeapHashCountedSet<Value, HashFunctions, Traits>& set,
    VectorType& vector) {
  CopyToVector(static_cast<const HashCountedSet<Value, HashFunctions, Traits,
                                                blink::HeapAllocator>&>(set),
               vector);
}

}  // namespace WTF

#endif

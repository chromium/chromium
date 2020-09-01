// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_ALLOCATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_HEAP_ALLOCATOR_H_

#include <type_traits>

#include "build/build_config.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_hash_table_backing.h"
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector_backing.h"
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

// This is a static-only class used as a trait on collections to make them heap
// allocated.  However see also HeapListHashSetAllocator.
class PLATFORM_EXPORT HeapAllocator {
  STATIC_ONLY(HeapAllocator);

 public:
  using LivenessBroker = blink::LivenessBroker;
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
    return reinterpret_cast<T*>(
        MakeGarbageCollected<HeapVectorBacking<T>>(size / sizeof(T)));
  }
  static void FreeVectorBacking(void*);
  static bool ExpandVectorBacking(void*, size_t);
  static bool ShrinkVectorBacking(void* address,
                                  size_t quantized_current_size,
                                  size_t quantized_shrunk_size);

  template <typename T, typename HashTable>
  static T* AllocateHashTableBacking(size_t size) {
    return reinterpret_cast<T*>(
        MakeGarbageCollected<HeapHashTableBacking<HashTable>>(
            size / sizeof(typename HashTable::ValueType)));
  }
  template <typename T, typename HashTable>
  static T* AllocateZeroedHashTableBacking(size_t size) {
    return AllocateHashTableBacking<T, HashTable>(size);
  }
  static void FreeHashTableBacking(void* address);
  static bool ExpandHashTableBacking(void*, size_t);

  static void TraceBackingStoreIfMarked(const void* address) {
    // Trace backing store elements only if backing store was marked. The
    // sweeper may be active on the backing store which requires atomic mark bit
    // access. A precise filter is performed in
    // MarkingVisitor::TraceMarkedBackingStore.
    if (HeapObjectHeader::FromPayload(address)
            ->IsMarked<HeapObjectHeader::AccessMode::kAtomic>()) {
      MarkingVisitor::TraceMarkedBackingStore(address);
    }
  }

  template <typename T>
  static void BackingWriteBarrier(T** slot) {
    MarkingVisitor::WriteBarrier(slot);
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

  static bool IsIncrementalMarking() {
    return ThreadState::IsAnyIncrementalMarking() &&
           ThreadState::Current()->IsIncrementalMarking();
  }

  template <typename T, typename Traits>
  static void Trace(Visitor* visitor, const T& t) {
    TraceCollectionIfEnabled<WTF::WeakHandlingTrait<T>::value, T,
                             Traits>::Trace(visitor, &t);
  }

  static void EnterGCForbiddenScope() {
    ThreadState::Current()->EnterGCForbiddenScope();
  }

  static void LeaveGCForbiddenScope() {
    ThreadState::Current()->LeaveGCForbiddenScope();
  }

  template <typename T, typename Traits>
  static void NotifyNewObject(T* object) {
#if BUILDFLAG(BLINK_HEAP_YOUNG_GENERATION)
    ThreadState* const thread_state = ThreadState::Current();
    if (!thread_state->IsIncrementalMarking()) {
      MarkingVisitor::GenerationalBarrier(reinterpret_cast<Address>(object),
                                          thread_state);
      return;
    }
#else
    if (!ThreadState::IsAnyIncrementalMarking())
      return;
    // The object may have been in-place constructed as part of a large object.
    // It is not safe to retrieve the page from the object here.
    ThreadState* const thread_state = ThreadState::Current();
    if (!thread_state->IsIncrementalMarking()) {
      return;
    }
#endif  // BLINK_HEAP_YOUNG_GENERATION
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

  template <typename T, typename Traits>
  static void NotifyNewObjects(T* array, size_t len) {
#if BUILDFLAG(BLINK_HEAP_YOUNG_GENERATION)
    ThreadState* const thread_state = ThreadState::Current();
    if (!thread_state->IsIncrementalMarking()) {
      MarkingVisitor::GenerationalBarrier(reinterpret_cast<Address>(array),
                                          thread_state);
      return;
    }
#else
    if (!ThreadState::IsAnyIncrementalMarking())
      return;
    // The object may have been in-place constructed as part of a large object.
    // It is not safe to retrieve the page from the object here.
    ThreadState* const thread_state = ThreadState::Current();
    if (!thread_state->IsIncrementalMarking()) {
      return;
    }
#endif  // BLINK_HEAP_YOUNG_GENERATION
    // See |NotifyNewObject| for details.
    ThreadState::NoAllocationScope no_allocation_scope(thread_state);
    DCHECK(thread_state->CurrentVisitor());
    // No weak handling for write barriers. Modifying weakly reachable objects
    // strongifies them for the current cycle.
    while (len-- > 0) {
      DCHECK(!Traits::kCanHaveDeletedValue || !Traits::IsDeletedValue(*array));
      TraceCollectionIfEnabled<WTF::kNoWeakHandling, T, Traits>::Trace(
          thread_state->CurrentVisitor(), array);
      array++;
    }
  }

  template <typename T>
  static void TraceVectorBacking(Visitor* visitor,
                                 const T* backing,
                                 const T* const* backing_slot) {
    visitor->TraceMovablePointer(backing_slot);
    visitor->Trace(reinterpret_cast<const HeapVectorBacking<T>*>(backing));
  }

  template <typename T, typename HashTable>
  static void TraceHashTableBackingStrongly(Visitor* visitor,
                                            const T* backing,
                                            const T* const* backing_slot) {
    visitor->TraceMovablePointer(backing_slot);
    visitor->Trace(
        reinterpret_cast<const HeapHashTableBacking<HashTable>*>(backing));
  }

  template <typename T, typename HashTable>
  static void TraceHashTableBackingWeakly(Visitor* visitor,
                                          const T* backing,
                                          const T* const* backing_slot,
                                          WeakCallback callback,
                                          const void* parameter) {
    visitor->TraceMovablePointer(backing_slot);
    visitor->TraceWeakContainer(
        reinterpret_cast<const HeapHashTableBacking<HashTable>*>(backing),
        reinterpret_cast<const HeapHashTableBacking<HashTable>* const*>(
            backing_slot),
        TraceTrait<HeapHashTableBacking<HashTable>>::GetTraceDescriptor(
            backing),
        TraceTrait<HeapHashTableBacking<HashTable>>::GetWeakTraceDescriptor(
            backing),
        callback, parameter);
  }

 private:
  static Address MarkAsConstructed(Address address) {
    HeapObjectHeader::FromPayload(reinterpret_cast<void*>(address))
        ->MarkFullyConstructed<HeapObjectHeader::AccessMode::kAtomic>();
    return address;
  }

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
static void TraceListHashSetValue(VisitorDispatcher visitor,
                                  const Value& value) {
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
  static void TraceValue(VisitorDispatcher visitor, const Node* node) {
    TraceListHashSetValue(visitor, node->value_);
  }
};

namespace internal {

template <typename T>
constexpr bool IsMember = WTF::IsSubclassOfTemplate<T, Member>::value;

template <typename T>
constexpr bool IsMemberOrWeakMemberType =
    WTF::IsSubclassOfTemplate<T, Member>::value ||
    WTF::IsSubclassOfTemplate<T, WeakMember>::value;

}  // namespace internal

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
    static_assert(internal::IsMemberOrWeakMemberType<KeyArg> ||
                      !WTF::IsTraceable<KeyArg>::value,
                  "HeapHashMap supports only Member, WeakMember and "
                  "non-traceable types as keys.");
    static_assert(internal::IsMemberOrWeakMemberType<MappedArg> ||
                      !WTF::IsTraceable<MappedArg>::value ||
                      WTF::IsSubclassOfTemplate<MappedArg,
                                                TraceWrapperV8Reference>::value,
                  "HeapHashMap supports only Member, WeakMember, "
                  "TraceWrapperV8Reference and "
                  "non-traceable types as values.");
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

template <typename T, typename U, typename V, typename W, typename X>
struct GCInfoTrait<HeapHashMap<T, U, V, W, X>>
    : public GCInfoTrait<HashMap<T, U, V, W, X, HeapAllocator>> {};

template <typename ValueArg,
          typename HashArg = typename DefaultHash<ValueArg>::Hash,
          typename TraitsArg = HashTraits<ValueArg>>
class HeapHashSet
    : public HashSet<ValueArg, HashArg, TraitsArg, HeapAllocator> {
  IS_GARBAGE_COLLECTED_CONTAINER_TYPE();
  DISALLOW_NEW();

  static void CheckType() {
    static_assert(internal::IsMemberOrWeakMemberType<ValueArg>,
                  "HeapHashSet supports only Member and WeakMember.");
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

template <typename T, typename U, typename V>
struct GCInfoTrait<HeapHashSet<T, U, V>>
    : public GCInfoTrait<HashSet<T, U, V, HeapAllocator>> {};

template <typename ValueArg, typename TraitsArg = HashTraits<ValueArg>>
class HeapLinkedHashSet
    : public LinkedHashSet<ValueArg, TraitsArg, HeapAllocator> {
  IS_GARBAGE_COLLECTED_CONTAINER_TYPE();
  DISALLOW_NEW();

  static void CheckType() {
    static_assert(internal::IsMemberOrWeakMemberType<ValueArg>,
                  "HeapLinkedHashSet supports only Member and WeakMember.");
    // If not trivially destructible, we have to add a destructor which will
    // hinder performance.
    static_assert(std::is_trivially_destructible<HeapLinkedHashSet>::value,
                  "HeapLinkedHashSet must be trivially destructible.");
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
    return ThreadHeap::Allocate<HeapLinkedHashSet<ValueArg, TraitsArg>>(size);
  }

  HeapLinkedHashSet() { CheckType(); }
};

template <typename T, typename U>
struct GCInfoTrait<HeapLinkedHashSet<T, U>>
    : public GCInfoTrait<LinkedHashSet<T, U, HeapAllocator>> {};

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
    static_assert(internal::IsMemberOrWeakMemberType<ValueArg>,
                  "HeapListHashSet supports only Member and WeakMember.");
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

template <typename T, wtf_size_t inlineCapacity, typename U>
struct GCInfoTrait<HeapListHashSet<T, inlineCapacity, U>>
    : public GCInfoTrait<
          ListHashSet<T,
                      inlineCapacity,
                      U,
                      HeapListHashSetAllocator<T, inlineCapacity>>> {};

template <typename Value,
          typename HashFunctions = typename DefaultHash<Value>::Hash,
          typename Traits = HashTraits<Value>>
class HeapHashCountedSet
    : public HashCountedSet<Value, HashFunctions, Traits, HeapAllocator> {
  IS_GARBAGE_COLLECTED_CONTAINER_TYPE();
  DISALLOW_NEW();

  static void CheckType() {
    static_assert(internal::IsMemberOrWeakMemberType<Value>,
                  "HeapHashCountedSet supports only Member and WeakMember.");
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

template <typename T, typename U, typename V>
struct GCInfoTrait<HeapHashCountedSet<T, U, V>>
    : public GCInfoTrait<HashCountedSet<T, U, V, HeapAllocator>> {};

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
    static_assert(!WTF::IsWeak<T>::value,
                  "Weak types are not allowed in HeapVector.");
    static_assert(WTF::IsTraceableInCollectionTrait<VectorTraits<T>>::value,
                  "Type must be traceable in collection");
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

template <typename T, wtf_size_t inlineCapacity>
struct GCInfoTrait<HeapVector<T, inlineCapacity>>
    : public GCInfoTrait<Vector<T, inlineCapacity, HeapAllocator>> {};

template <typename T>
class HeapDeque : public Deque<T, 0, HeapAllocator> {
  IS_GARBAGE_COLLECTED_CONTAINER_TYPE();
  DISALLOW_NEW();

  static void CheckType() {
    static_assert(internal::IsMember<T>, "HeapDeque supports only Member.");
    static_assert(std::is_trivially_destructible<HeapDeque>::value,
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
    return ThreadHeap::Allocate<HeapDeque<T>>(size);
  }

  HeapDeque() { CheckType(); }

  explicit HeapDeque(wtf_size_t size) : Deque<T, 0, HeapAllocator>(size) {
    CheckType();
  }

  HeapDeque(wtf_size_t size, const T& val)
      : Deque<T, 0, HeapAllocator>(size, val) {
    CheckType();
  }

  HeapDeque& operator=(const HeapDeque& other) {
    HeapDeque<T> copy(other);
    Deque<T, 0, HeapAllocator>::Swap(copy);
    return *this;
  }

  HeapDeque(const HeapDeque<T>& other) : Deque<T, 0, HeapAllocator>(other) {}
};

template <typename T>
struct GCInfoTrait<HeapDeque<T>>
    : public GCInfoTrait<Deque<T, 0, HeapAllocator>> {};

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

  static constexpr bool kCanTraceConcurrently = true;
};

// These traits are used in VectorBackedLinkedList to support WeakMember in
// HeapLinkedHashSet though HeapVector<WeakMember> usage is still banned.
// (See the discussion in https://crrev.com/c/2246014)
template <typename T>
struct VectorTraits<blink::WeakMember<T>>
    : VectorTraitsBase<blink::WeakMember<T>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanCopyWithMemcpy = true;
  static const bool kCanMoveWithMemcpy = true;

  static constexpr bool kCanTraceConcurrently = true;
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
struct VectorTraits<blink::HeapDeque<T>>
    : VectorTraitsBase<blink::HeapDeque<T>> {
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

  static constexpr bool kCanTraceConcurrently = true;
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

  static void ConstructDeletedValue(blink::WeakMember<T>& slot, bool) {
    slot = WTF::kHashTableDeletedValue;
  }

  static constexpr bool kCanTraceConcurrently = true;
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

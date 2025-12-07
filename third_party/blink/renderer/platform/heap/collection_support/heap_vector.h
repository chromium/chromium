// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_VECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_VECTOR_H_

#include <initializer_list>
#include <type_traits>

#include "third_party/blink/renderer/platform/heap/collection_support/utils.h"
#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator_impl.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

template <internal::HeapCollectionType CollectionType,
          typename T,
          wtf_size_t inlineCapacity = 0>
class BasicHeapVector final
    : public std::conditional_t<
          CollectionType == internal::HeapCollectionType::kGCed,
          GarbageCollected<BasicHeapVector<CollectionType, T, inlineCapacity>>,
          internal::DisallowNewBaseForHeapCollections>,
      public Vector<T, inlineCapacity, HeapAllocator> {
  using BaseVector = Vector<T, inlineCapacity, HeapAllocator>;

 public:
  BasicHeapVector() = default;

  explicit BasicHeapVector(wtf_size_t size) : BaseVector(size) {}

  BasicHeapVector(wtf_size_t size, const T& val) : BaseVector(size, val) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  BasicHeapVector(const BasicHeapVector& other) : BaseVector(other) {}

  template <wtf_size_t otherCapacity>
  // NOLINTNEXTLINE(google-explicit-constructor)
  BasicHeapVector(
      const BasicHeapVector<CollectionType, T, otherCapacity>& other)
      : BaseVector(other) {}

  template <internal::HeapCollectionType OtherCollectionType,
            wtf_size_t otherCapacity>
  explicit BasicHeapVector(
      const BasicHeapVector<OtherCollectionType, T, otherCapacity>& other)
      : BaseVector(other) {}

  template <internal::HeapCollectionType OtherCollectionType,
            typename U,
            wtf_size_t otherCapacity>
  explicit BasicHeapVector(
      const BasicHeapVector<OtherCollectionType, U, otherCapacity>& other)
      : BaseVector(static_cast<const BaseVector&>(other)) {}

  template <typename Collection>
    requires(std::is_class_v<Collection>)
  explicit BasicHeapVector(const Collection& other) : BaseVector(other) {}

  // Projection-based initialization. This way of initialization can avoid write
  // barriers even in the presence of GC due to allocations in `Proj`.
  //
  // This works because of the  following:
  // 1) During initialization the vector is reachable from the stack and will
  //    thus always be found and fully processed at the end of marking.
  // 2) The backing store is created via initializing store and thus does not
  //    escape to the object graph via write barrier.
  // 3) GCs triggered through allocations in `Proj` will never find the backing
  //    store as it's only reachable from stack or an in-construction HeapVector
  //    which is always delayed till the end of GC.
  template <typename Proj>
    requires(std::is_invocable_v<Proj, typename BaseVector::const_reference>)
  BasicHeapVector(const BasicHeapVector& other, Proj proj)
      : BaseVector(static_cast<const BaseVector&>(other), std::move(proj)) {}

  template <internal::HeapCollectionType OtherCollectionType,
            typename U,
            wtf_size_t otherSize,
            typename Proj>
    requires(std::is_invocable_v<
             Proj,
             typename BasicHeapVector<OtherCollectionType, U, otherSize>::
                 const_reference>)
  BasicHeapVector(
      const BasicHeapVector<OtherCollectionType, U, otherSize>& other,
      Proj proj)
      : BaseVector(
            static_cast<const Vector<U, otherSize, HeapAllocator>&>(other),
            std::move(proj)) {}

  BasicHeapVector& operator=(const BasicHeapVector& other) {
    BaseVector::operator=(other);
    return *this;
  }

  template <typename Collection>
  BasicHeapVector& operator=(const Collection& other) {
    Vector<T, inlineCapacity, HeapAllocator>::operator=(other);
    return *this;
  }

  BasicHeapVector(BasicHeapVector&& other) noexcept
      : BaseVector(static_cast<BaseVector&&>(std::move(other))) {}

  template <internal::HeapCollectionType OtherCollectionType,
            typename U,
            wtf_size_t otherCapacity>
  explicit BasicHeapVector(
      BasicHeapVector<OtherCollectionType, U, otherCapacity>&& other) noexcept
      : BaseVector(std::move(other)) {}

  BasicHeapVector& operator=(BasicHeapVector&& other) noexcept {
    BaseVector::operator=(std::move(other));
    return *this;
  }

  BasicHeapVector(std::initializer_list<T> elements)
      : BaseVector(std::move(elements)) {}

  BasicHeapVector& operator=(std::initializer_list<T> elements) {
    BaseVector::operator=(std::move(elements));
    return *this;
  }

  void Trace(Visitor* visitor) const { BaseVector::Trace(visitor); }

 private:
  struct TypeConstraints {
    constexpr TypeConstraints() {
      static_assert(
          std::is_trivially_destructible_v<BasicHeapVector> || inlineCapacity,
          "BasicHeapVector must be trivially destructible.");
      static_assert(!IsWeakV<T>,
                    "Weak types are not allowed in BasicHeapVector.");
      static_assert(!IsGarbageCollectedTypeV<T>,
                    "GCed types should not be inlined in a BasicHeapVector.");
      static_assert(!IsPointerToGarbageCollectedType<T>,
                    "Don't use raw pointers or reference to garbage collected "
                    "types in BasicHeapVector. Use Member<> instead.");
      static_assert(!IsPointerToTraceableType<T>,
                    "Don't use raw pointers or reference to traceable "
                    "types in BasicHeapVector. Use Member<> instead.");

      // HeapVector may hold non-traceable types. This is useful for vectors
      // held by garbage collected objects such that the vectors' backing stores
      // are accounted as memory held by the GC. HeapVectors of non-traceable
      // types should only be used as fields of traceable types.
    }
  };
  static_assert(std::is_empty_v<TypeConstraints>);
  NO_UNIQUE_ADDRESS TypeConstraints type_constraints_;
};

// On-stack for in-field version of Vector for referring to
// GarbageCollected objects.
template <typename T, wtf_size_t inlineCapacity = 0>
using HeapVector = BasicHeapVector<internal::HeapCollectionType::kDisallowNew,
                                   T,
                                   inlineCapacity>;
static_assert(IsDisallowNew<HeapVector<int>>);
ASSERT_SIZE(Vector<int>, HeapVector<int>);

// GCed version of Vector for referring to GarbageCollected objects.
template <typename T, wtf_size_t inlineCapacity = 0>
using GCedHeapVector =
    BasicHeapVector<internal::HeapCollectionType::kGCed, T, inlineCapacity>;
static_assert(!IsDisallowNew<GCedHeapVector<int>>);
ASSERT_SIZE(Vector<int>, GCedHeapVector<int>);

template <typename T>
struct VectorTraits<Member<T>> : VectorTraitsBase<Member<T>> {
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
struct VectorTraits<WeakMember<T>> : VectorTraitsBase<WeakMember<T>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanCopyWithMemcpy = true;
  static const bool kCanMoveWithMemcpy = true;

  static constexpr bool kCanTraceConcurrently = true;
};

template <typename T>
struct VectorTraits<UntracedMember<T>> : VectorTraitsBase<UntracedMember<T>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
};

template <typename T>
struct VectorTraits<HeapVector<T, 0>> : VectorTraitsBase<HeapVector<T, 0>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
};

template <typename T, wtf_size_t inlineCapacity>
struct VectorTraits<HeapVector<T, inlineCapacity>>
    : VectorTraitsBase<HeapVector<T, inlineCapacity>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = VectorTraits<T>::kNeedsDestruction;
  static const bool kCanInitializeWithMemset =
      VectorTraits<T>::kCanInitializeWithMemset;
  static const bool kCanClearUnusedSlotsWithMemset =
      VectorTraits<T>::kCanClearUnusedSlotsWithMemset;
  static const bool kCanMoveWithMemcpy = VectorTraits<T>::kCanMoveWithMemcpy;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_VECTOR_H_

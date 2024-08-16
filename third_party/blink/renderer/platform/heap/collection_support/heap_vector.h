// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_VECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_VECTOR_H_

#include <initializer_list>

#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator_impl.h"
#include "third_party/blink/renderer/platform/heap/member.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

// TODO(crbug.com/355003172): The default value of checked_iter should be true.
template <typename T, wtf_size_t inlineCapacity = 0, bool checked_iter = false>
class HeapVector final
    : public GarbageCollected<HeapVector<T, inlineCapacity, checked_iter>>,
      public Vector<T, inlineCapacity, HeapAllocator, checked_iter> {
  DISALLOW_NEW();

  using BaseVector = Vector<T, inlineCapacity, HeapAllocator, checked_iter>;

 public:
  HeapVector() = default;

  explicit HeapVector(wtf_size_t size) : BaseVector(size) {}

  HeapVector(wtf_size_t size, const T& val) : BaseVector(size, val) {}

  template <wtf_size_t otherCapacity>
  HeapVector(const HeapVector<T, otherCapacity, checked_iter>& other)  // NOLINT
      : BaseVector(other) {}

  HeapVector(const HeapVector& other)
      : BaseVector(static_cast<const BaseVector&>(other)) {}

  template <
      typename Proj,
      typename = std::enable_if_t<
          std::is_invocable_v<Proj, typename BaseVector::const_reference>>>
  HeapVector(const HeapVector& other, Proj proj)
      : BaseVector(static_cast<const BaseVector&>(other), std::move(proj)) {}

  template <
      typename U,
      wtf_size_t otherSize,
      typename Proj,
      typename = std::enable_if_t<
          std::is_invocable_v<Proj, typename BaseVector::const_reference>>>
  HeapVector(const HeapVector<U, otherSize, checked_iter>& other, Proj proj)
      : BaseVector(static_cast<const BaseVector&>(other), std::move(proj)) {}

  template <typename Collection,
            typename =
                typename std::enable_if<std::is_class<Collection>::value>::type>
  explicit HeapVector(const Collection& other) : BaseVector(other) {}

  HeapVector& operator=(const HeapVector& other) {
    BaseVector::operator=(other);
    return *this;
  }

  template <typename Collection>
  HeapVector& operator=(const Collection& other) {
    Vector<T, inlineCapacity, HeapAllocator>::operator=(other);
    return *this;
  }

  HeapVector(HeapVector&& other) noexcept
      : BaseVector(static_cast<BaseVector&&>(std::move(other))) {}

  HeapVector& operator=(HeapVector&& other) noexcept {
    BaseVector::operator=(std::move(other));
    return *this;
  }

  HeapVector(std::initializer_list<T> elements)
      : BaseVector(std::move(elements)) {}

  HeapVector& operator=(std::initializer_list<T> elements) {
    BaseVector::operator=(std::move(elements));
    return *this;
  }

  void Trace(Visitor* visitor) const { BaseVector::Trace(visitor); }

 private:
  struct TypeConstraints {
    constexpr TypeConstraints();
  };
  static_assert(std::is_empty_v<TypeConstraints>);
  NO_UNIQUE_ADDRESS TypeConstraints type_constraints_;
};

template <typename T>
concept IsHeapVector = requires { typename HeapVector<T>; };

template <typename T, wtf_size_t inlineCapacity, bool checked_iter>
constexpr HeapVector<T, inlineCapacity, checked_iter>::TypeConstraints::
    TypeConstraints() {
  static_assert(std::is_trivially_destructible_v<HeapVector> || inlineCapacity,
                "HeapVector must be trivially destructible.");
  static_assert(!WTF::IsWeak<T>::value,
                "Weak types are not allowed in HeapVector.");
  static_assert(!WTF::IsGarbageCollectedType<T>::value || IsHeapVector<T>,
                "GCed types should not be inlined in a HeapVector.");
  static_assert(!WTF::IsPointerToGced<T>::value,
                "Don't use raw pointers or reference to garbage collected "
                "types in HeapVector. Use Member<> instead.");

  // HeapVector may hold non-traceable types. This is useful for vectors held
  // by garbage collected objects such that the vectors' backing stores are
  // accounted as memory held by the GC. HeapVectors of non-traceable types
  // should only be used as fields of traceable types.
}

ASSERT_SIZE(Vector<int>, HeapVector<int>);

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

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_VECTOR_H_

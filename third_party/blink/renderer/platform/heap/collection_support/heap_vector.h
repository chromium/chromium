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
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

template <typename T, wtf_size_t inlineCapacity = 0>
class HeapVector final : public GarbageCollected<HeapVector<T, inlineCapacity>>,
                         public Vector<T, inlineCapacity, HeapAllocator> {
  DISALLOW_NEW();

  using BaseVector = Vector<T, inlineCapacity, HeapAllocator>;

 public:
  HeapVector() = default;

  explicit HeapVector(wtf_size_t size) : BaseVector(size) { CheckType(); }

  HeapVector(wtf_size_t size, const T& val) : BaseVector(size, val) {
    CheckType();
  }

  template <wtf_size_t otherCapacity>
  HeapVector(const HeapVector<T, otherCapacity>& other)  // NOLINT
      : BaseVector(other) {
    CheckType();
  }

  HeapVector(const HeapVector& other)
      : BaseVector(static_cast<const BaseVector&>(other)) {
    CheckType();
  }

  template <typename Collection,
            typename =
                typename std::enable_if<std::is_class<Collection>::value>::type>
  explicit HeapVector(const Collection& other) : BaseVector(other) {
    CheckType();
  }

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
      : BaseVector(static_cast<BaseVector&&>(std::move(other))) {
    CheckType();
  }

  HeapVector& operator=(HeapVector&& other) noexcept {
    BaseVector::operator=(std::move(other));
    return *this;
  }

  HeapVector(std::initializer_list<T> elements)
      : BaseVector(std::move(elements)) {
    CheckType();
  }

  HeapVector& operator=(std::initializer_list<T> elements) {
    BaseVector::operator=(std::move(elements));
    return *this;
  }

  void Trace(Visitor* visitor) const { BaseVector::Trace(visitor); }

 private:
  template <typename U>
  struct IsHeapVector {
   private:
    typedef char YesType;
    struct NoType {
      char padding[8];
    };

    template <typename X, wtf_size_t Y>
    static YesType SubclassCheck(HeapVector<X, Y>*);
    static NoType SubclassCheck(...);
    static U* u_;

   public:
    static const bool value = sizeof(SubclassCheck(u_)) == sizeof(YesType);
  };

  static constexpr void CheckType() {
    static_assert(
        std::is_trivially_destructible<HeapVector>::value || inlineCapacity,
        "HeapVector must be trivially destructible.");
    static_assert(WTF::IsTraceable<T>::value,
                  "For vectors without traceable elements, use Vector<> "
                  "instead of HeapVector<>.");
    static_assert(!WTF::IsWeak<T>::value,
                  "Weak types are not allowed in HeapVector.");
    static_assert(
        !WTF::IsGarbageCollectedType<T>::value || IsHeapVector<T>::value,
        "GCed types should not be inlined in a HeapVector.");
    static_assert(WTF::IsTraceableInCollectionTrait<VectorTraits<T>>::value,
                  "Type must be traceable in collection");
  }
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

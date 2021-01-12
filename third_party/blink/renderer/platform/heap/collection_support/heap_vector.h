// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_VECTOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_VECTOR_H_

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator_impl.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

template <typename T, wtf_size_t inlineCapacity = 0>
class HeapVector final : public GarbageCollected<HeapVector<T, inlineCapacity>>,
                         public Vector<T, inlineCapacity, HeapAllocator> {
  DISALLOW_NEW();

 public:
  HeapVector() = default;

  explicit HeapVector(wtf_size_t size)
      : Vector<T, inlineCapacity, HeapAllocator>(size) {}

  HeapVector(wtf_size_t size, const T& val)
      : Vector<T, inlineCapacity, HeapAllocator>(size, val) {}

  template <wtf_size_t otherCapacity>
  HeapVector(const HeapVector<T, otherCapacity>& other)  // NOLINT
      : Vector<T, inlineCapacity, HeapAllocator>(other) {}

  HeapVector(const HeapVector& other)
      : Vector<T, inlineCapacity, HeapAllocator>(other) {}

  HeapVector& operator=(const HeapVector& other) {
    Vector<T, inlineCapacity, HeapAllocator>::operator=(other);
    return *this;
  }

  HeapVector(HeapVector&& other) noexcept
      : Vector<T, inlineCapacity, HeapAllocator>(std::move(other)) {}

  HeapVector& operator=(HeapVector&& other) noexcept {
    Vector<T, inlineCapacity, HeapAllocator>::operator=(std::move(other));
    return *this;
  }

  HeapVector(std::initializer_list<T> elements)
      : Vector<T, inlineCapacity, HeapAllocator>(elements) {}

  void Trace(Visitor* visitor) const {
    CheckType();
    Vector<T, inlineCapacity, HeapAllocator>::Trace(visitor);
  }

 private:
  static constexpr void CheckType() {
    static_assert(
        std::is_trivially_destructible<HeapVector>::value || inlineCapacity,
        "HeapVector must be trivially destructible.");
    static_assert(WTF::IsTraceable<T>::value,
                  "For vectors without traceable elements, use Vector<> "
                  "instead of HeapVector<>.");
    static_assert(!WTF::IsWeak<T>::value,
                  "Weak types are not allowed in HeapVector.");
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

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_VECTOR_H_

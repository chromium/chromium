// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_DEQUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_DEQUE_H_

// Include heap_vector.h to also make general VectorTraits available.
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator_impl.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

template <typename T>
class HeapDeque final : public GarbageCollected<HeapDeque<T>>,
                        public Deque<T, 0, HeapAllocator> {
  DISALLOW_NEW();

 public:
  HeapDeque() { CheckType(); }

  explicit HeapDeque(wtf_size_t size) : Deque<T, 0, HeapAllocator>(size) {
    CheckType();
  }

  HeapDeque(wtf_size_t size, const T& val)
      : Deque<T, 0, HeapAllocator>(size, val) {
    CheckType();
  }

  HeapDeque(const HeapDeque<T>& other) : Deque<T, 0, HeapAllocator>(other) {
    CheckType();
  }

  HeapDeque& operator=(const HeapDeque& other) {
    Deque<T, 0, HeapAllocator>::operator=(other);
    return *this;
  }

  HeapDeque(HeapDeque&& other) noexcept
      : Deque<T, 0, HeapAllocator>(std::move(other)) {
    CheckType();
  }

  HeapDeque& operator=(HeapDeque&& other) noexcept {
    Deque<T, 0, HeapAllocator>::operator=(std::move(other));
    return *this;
  }

  void Trace(Visitor* visitor) const {
    Deque<T, 0, HeapAllocator>::Trace(visitor);
  }

 private:
  static constexpr void CheckType() {
    static_assert(WTF::IsMemberType<T>::value,
                  "HeapDeque supports only Member.");
    static_assert(std::is_trivially_destructible<HeapDeque>::value,
                  "HeapDeque must be trivially destructible.");
    static_assert(WTF::IsTraceable<T>::value,
                  "For vectors without traceable elements, use Deque<> instead "
                  "of HeapDeque<>");
  }
};

}  // namespace blink

namespace WTF {

template <typename T>
struct VectorTraits<blink::HeapDeque<T>>
    : VectorTraitsBase<blink::HeapDeque<T>> {
  STATIC_ONLY(VectorTraits);
  static const bool kNeedsDestruction = false;
  static const bool kCanInitializeWithMemset = true;
  static const bool kCanClearUnusedSlotsWithMemset = true;
  static const bool kCanMoveWithMemcpy = true;
};

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_DEQUE_H_

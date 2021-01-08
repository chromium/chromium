// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_DEQUE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_DEQUE_H_

#include "third_party/blink/renderer/platform/heap/heap_allocator_impl.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

template <typename T>
class HeapDeque final : public Deque<T, 0, HeapAllocator> {
  IS_GARBAGE_COLLECTED_CONTAINER_TYPE();
  DISALLOW_NEW();

  static void CheckType() {
    static_assert(WTF::IsMemberType<T>::value,
                  "HeapDeque supports only Member.");
    static_assert(std::is_trivially_destructible<HeapDeque>::value,
                  "HeapDeque must be trivially destructible.");
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

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_DEQUE_H_

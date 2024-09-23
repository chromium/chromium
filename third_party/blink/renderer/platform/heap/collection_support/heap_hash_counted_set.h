// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_COUNTED_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_COUNTED_SET_H_

#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator_impl.h"
#include "third_party/blink/renderer/platform/wtf/hash_counted_set.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"

namespace blink {

template <typename Value, typename Traits = HashTraits<Value>>
class HeapHashCountedSet final
    : public GarbageCollected<HeapHashCountedSet<Value, Traits>>,
      public HashCountedSet<Value, Traits, HeapAllocator> {
  DISALLOW_NEW();

 public:
  HeapHashCountedSet() = default;

  void Trace(Visitor* visitor) const {
    HashCountedSet<Value, Traits, HeapAllocator>::Trace(visitor);
  }

 private:
  struct TypeConstraints {
    constexpr TypeConstraints() {
      static_assert(WTF::IsMemberOrWeakMemberType<Value>::value,
                    "HeapHashCountedSet supports only Member and WeakMember.");
      static_assert(std::is_trivially_destructible<HeapHashCountedSet>::value,
                    "HeapHashCountedSet must be trivially destructible.");
      static_assert(WTF::IsTraceable<Value>::value,
                    "For counted sets without traceable elements, use "
                    "HashCountedSet<> instead of HeapHashCountedSet<>.");
    }
  };
  NO_UNIQUE_ADDRESS TypeConstraints type_constraints_;
};

ASSERT_SIZE(HeapHashCountedSet<int>, HashCountedSet<int>);

}  // namespace blink

namespace WTF {

template <typename Value, typename Traits, typename VectorType>
inline void CopyToVector(const blink::HeapHashCountedSet<Value, Traits>& set,
                         VectorType& vector) {
  CopyToVector(
      static_cast<const HashCountedSet<Value, Traits, blink::HeapAllocator>&>(
          set),
      vector);
}

}  // namespace WTF

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_COUNTED_SET_H_

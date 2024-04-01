// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_LINKED_HASH_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_LINKED_HASH_SET_H_

// Needs heap_vector.h for VectorTraits of Member and WeakMember.
#include "third_party/blink/renderer/platform/heap/collection_support/heap_vector.h"
#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator_impl.h"
#include "third_party/blink/renderer/platform/wtf/linked_hash_set.h"
#include "third_party/blink/renderer/platform/wtf/size_assertions.h"
#include "third_party/blink/renderer/platform/wtf/type_traits.h"

namespace blink {

template <typename ValueArg, typename TraitsArg = HashTraits<ValueArg>>
class HeapLinkedHashSet final
    : public GarbageCollected<HeapLinkedHashSet<ValueArg, TraitsArg>>,
      public LinkedHashSet<ValueArg, TraitsArg, HeapAllocator> {
 public:
  HeapLinkedHashSet() = default;

  void Trace(Visitor* v) const {
    LinkedHashSet<ValueArg, TraitsArg, HeapAllocator>::Trace(v);
  }

 private:
  struct TypeConstraints {
    constexpr TypeConstraints() {
      static_assert(WTF::IsMemberOrWeakMemberType<ValueArg>::value,
                    "HeapLinkedHashSet supports only Member and WeakMember.");
      static_assert(std::is_trivially_destructible_v<HeapLinkedHashSet>,
                    "HeapLinkedHashSet must be trivially destructible.");
      static_assert(WTF::IsTraceable<ValueArg>::value,
                    "For sets without traceable elements, use LinkedHashSet<> "
                    "instead of HeapLinkedHashSet<>.");
    }
  };
  NO_UNIQUE_ADDRESS TypeConstraints type_constraints_;
};

ASSERT_SIZE(HeapLinkedHashSet<int>, LinkedHashSet<int>);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_LINKED_HASH_SET_H_

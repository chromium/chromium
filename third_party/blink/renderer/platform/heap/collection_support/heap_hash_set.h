// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_SET_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_SET_H_

#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator_impl.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"

namespace blink {

template <typename ValueArg, typename TraitsArg = HashTraits<ValueArg>>
class HeapHashSet final
    : public GarbageCollected<HeapHashSet<ValueArg, TraitsArg>>,
      public HashSet<ValueArg, TraitsArg, HeapAllocator> {
  DISALLOW_NEW();

 public:
  HeapHashSet() { CheckType(); }

  void Trace(Visitor* visitor) const {
    HashSet<ValueArg, TraitsArg, HeapAllocator>::Trace(visitor);
  }

 private:
  static constexpr void CheckType() {
    static_assert(WTF::IsMemberOrWeakMemberType<ValueArg>::value,
                  "HeapHashSet supports only Member and WeakMember.");
    static_assert(std::is_trivially_destructible<HeapHashSet>::value,
                  "HeapHashSet must be trivially destructible.");
    static_assert(WTF::IsTraceable<ValueArg>::value,
                  "For hash sets without traceable elements, use HashSet<> "
                  "instead of HeapHashSet<>.");
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_SET_H_

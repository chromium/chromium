// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_MAP_H_

#include "third_party/blink/renderer/platform/heap/heap_allocator_impl.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

template <typename KeyArg,
          typename MappedArg,
          typename HashArg = typename DefaultHash<KeyArg>::Hash,
          typename KeyTraitsArg = HashTraits<KeyArg>,
          typename MappedTraitsArg = HashTraits<MappedArg>>
class HeapHashMap final : public HashMap<KeyArg,
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
        WTF::IsTraceable<KeyArg>::value || WTF::IsTraceable<MappedArg>::value,
        "For hash maps without traceable elements, use HashMap<> "
        "instead of HeapHashMap<>.");
    static_assert(WTF::IsMemberOrWeakMemberType<KeyArg>::value ||
                      !WTF::IsTraceable<KeyArg>::value,
                  "HeapHashMap supports only Member, WeakMember and "
                  "non-traceable types as keys.");
    static_assert(WTF::IsMemberOrWeakMemberType<MappedArg>::value ||
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

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_MAP_H_

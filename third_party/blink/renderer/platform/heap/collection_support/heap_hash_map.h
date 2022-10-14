// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_MAP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_MAP_H_

#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_allocator_impl.h"
#include "third_party/blink/renderer/platform/wtf/hash_map.h"

namespace blink {

template <typename KeyArg,
          typename MappedArg,
          typename HashArg = DefaultHash<KeyArg>,
          typename KeyTraitsArg = HashTraits<KeyArg>,
          typename MappedTraitsArg = HashTraits<MappedArg>>
class HeapHashMap final : public GarbageCollected<HeapHashMap<KeyArg,
                                                              MappedArg,
                                                              HashArg,
                                                              KeyTraitsArg,
                                                              MappedTraitsArg>>,
                          public HashMap<KeyArg,
                                         MappedArg,
                                         HashArg,
                                         KeyTraitsArg,
                                         MappedTraitsArg,
                                         HeapAllocator> {
  DISALLOW_NEW();

 public:
  HeapHashMap() { CheckType(); }

  void Trace(Visitor* visitor) const {
    HashMap<KeyArg, MappedArg, HashArg, KeyTraitsArg, MappedTraitsArg,
            HeapAllocator>::Trace(visitor);
  }

 private:
  static constexpr void CheckType() {
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
    static_assert(
        WTF::IsMemberOrWeakMemberType<MappedArg>::value ||
            !WTF::IsTraceable<MappedArg>::value ||
            WTF::IsSubclassOfTemplate<MappedArg, v8::TracedReference>::value,
        "HeapHashMap supports only Member, WeakMember, "
        "TraceWrapperV8Reference and "
        "non-traceable types as values.");
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_COLLECTION_SUPPORT_HEAP_HASH_MAP_H_

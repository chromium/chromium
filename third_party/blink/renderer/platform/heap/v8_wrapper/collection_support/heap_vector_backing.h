// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_COLLECTION_SUPPORT_HEAP_VECTOR_BACKING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_COLLECTION_SUPPORT_HEAP_VECTOR_BACKING_H_

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/conditional_destructor.h"
#include "third_party/blink/renderer/platform/wtf/vector_traits.h"

namespace blink {

template <typename T, typename Traits = WTF::VectorTraits<T>>
class HeapVectorBacking final
    : public GarbageCollected<HeapVectorBacking<T, Traits>>,
      public WTF::ConditionalDestructor<HeapVectorBacking<T, Traits>,
                                        !Traits::kNeedsDestruction> {
 public:
  // Conditionally invoked via destructor.
  void Finalize();
};

template <typename T, typename Traits>
void HeapVectorBacking<T, Traits>::Finalize() {
  static_assert(Traits::kNeedsDestruction,
                "Only vector buffers with items requiring destruction should "
                "be finalized");
  static_assert(
      Traits::kCanClearUnusedSlotsWithMemset || std::is_polymorphic<T>::value,
      "HeapVectorBacking doesn't support objects that cannot be cleared as "
      "unused with memset or don't have a vtable");
  static_assert(
      !std::is_trivially_destructible<T>::value,
      "Finalization of trivially destructible classes should not happen.");
  // TODO(1056170): Implement.
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_COLLECTION_SUPPORT_HEAP_VECTOR_BACKING_H_

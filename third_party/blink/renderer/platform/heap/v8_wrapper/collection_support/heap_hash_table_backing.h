// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_COLLECTION_SUPPORT_HEAP_HASH_TABLE_BACKING_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_COLLECTION_SUPPORT_HEAP_HASH_TABLE_BACKING_H_

#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/wtf/conditional_destructor.h"

namespace blink {

template <typename Table>
class HeapHashTableBacking final
    : public GarbageCollected<HeapHashTableBacking<Table>>,
      public WTF::ConditionalDestructor<
          HeapHashTableBacking<Table>,
          std::is_trivially_destructible<typename Table::ValueType>::value> {
 public:
  // Conditionally invoked via destructor.
  void Finalize();
};

template <typename Table>
void HeapHashTableBacking<Table>::Finalize() {
  using Value = typename Table::ValueType;
  static_assert(
      !std::is_trivially_destructible<Value>::value,
      "Finalization of trivially destructible classes should not happen.");
  // TODO(1056170): Implement.
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_COLLECTION_SUPPORT_HEAP_HASH_TABLE_BACKING_H_

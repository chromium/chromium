// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_WRITE_BARRIER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_WRITE_BARRIER_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/cppgc/heap-consistency.h"

namespace blink {

class WriteBarrier {
  STATIC_ONLY(WriteBarrier);

  using HeapConsistency = cppgc::subtle::HeapConsistency;

 public:
  template <typename T>
  static void DispatchForObject(T* element) {
    HeapConsistency::WriteBarrierParams params;
    switch (HeapConsistency::GetWriteBarrierType(element, *element, params)) {
      case HeapConsistency::WriteBarrierType::kMarking:
        HeapConsistency::DijkstraWriteBarrier(params, *element);
        break;
      case HeapConsistency::WriteBarrierType::kGenerational:
        HeapConsistency::GenerationalBarrier(params, element);
        break;
      case HeapConsistency::WriteBarrierType::kNone:
        break;
      default:
        break;  // TODO(1056170): Remove default case when API is stable.
    }
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_WRITE_BARRIER_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_UNIFIED_HEAP_MARKING_VISITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_UNIFIED_HEAP_MARKING_VISITOR_H_

#include "base/compiler_specific.h"
#include "third_party/blink/renderer/platform/bindings/wrapper_type_info.h"
#include "third_party/blink/renderer/platform/heap/v8_wrapper/thread_state.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8-cppgc.h"

namespace blink {

class PLATFORM_EXPORT UnifiedHeapMarkingVisitor final {
  STATIC_ONLY(UnifiedHeapMarkingVisitor);

 public:
  static ALWAYS_INLINE void WriteBarrier(
      const v8::TracedReference<v8::Value>& ref) {
    v8::JSHeapConsistency::WriteBarrierParams params;
    if (v8::JSHeapConsistency::GetWriteBarrierType(
            ref, params, []() -> cppgc::HeapHandle& {
              return ThreadState::Current()->heap_handle();
            }) == v8::JSHeapConsistency::WriteBarrierType::kMarking) {
      v8::JSHeapConsistency::DijkstraMarkingBarrier(
          params, ThreadState::Current()->heap_handle(), ref);
    }
  }

  static ALWAYS_INLINE void WriteBarrier(v8::Isolate*,
                                         v8::Local<v8::Object>& wrapper,
                                         const WrapperTypeInfo*,
                                         const void* wrappable) {
    v8::JSHeapConsistency::WriteBarrierParams params;
    if (v8::JSHeapConsistency::GetWriteBarrierType(
            wrapper, kV8DOMWrapperObjectIndex, wrappable, params,
            []() -> cppgc::HeapHandle& {
              return ThreadState::Current()->heap_handle();
            }) == v8::JSHeapConsistency::WriteBarrierType::kMarking) {
      v8::JSHeapConsistency::DijkstraMarkingBarrier(
          params, ThreadState::Current()->heap_handle(), wrappable);
    }
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_UNIFIED_HEAP_MARKING_VISITOR_H_

// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_UNIFIED_HEAP_MARKING_VISITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_UNIFIED_HEAP_MARKING_VISITOR_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/v8.h"

namespace blink {

template <typename T>
class TraceWrapperV8Reference;

class UnifiedHeapMarkingVisitor final {
  STATIC_ONLY(UnifiedHeapMarkingVisitor);

 public:
  static void WriteBarrier(const TraceWrapperV8Reference<v8::Value>&) {
    // TODO(mlippautz): Delegate to cppgc write barrier.
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_UNIFIED_HEAP_MARKING_VISITOR_H_

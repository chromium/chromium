// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_TEST_UTILITIES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_TEST_UTILITIES_H_

#include "third_party/blink/renderer/platform/heap/v8_wrapper/thread_state.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/cppgc/testing.h"

namespace blink {

// Allows for overriding the stack state for the purpose of testing. Any garbage
// collection calls scoped with `HeapPointersOnStackScope` will perform
// conservative stack scanning, even if other (more local) hints indicate that
// there's no need for it.
class HeapPointersOnStackScope final {
  STACK_ALLOCATED();

 public:
  explicit HeapPointersOnStackScope(const ThreadState* state)
      : embedder_stack_state_(
            state->cpp_heap().GetHeapHandle(),
            cppgc::EmbedderStackState::kMayContainHeapPointers) {}

  HeapPointersOnStackScope(const HeapPointersOnStackScope&) = delete;
  HeapPointersOnStackScope& operator=(const HeapPointersOnStackScope&) = delete;

 private:
  cppgc::testing::OverrideEmbedderStackStateScope embedder_stack_state_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_TEST_UTILITIES_H_

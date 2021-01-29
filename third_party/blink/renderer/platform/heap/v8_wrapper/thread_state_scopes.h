// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_THREAD_STATE_SCOPES_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_THREAD_STATE_SCOPES_H_

#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "v8/include/cppgc/heap-consistency.h"

namespace blink {

// The NoAllocationScope class is used in debug mode to catch unwanted
// allocations. E.g. allocations during GC.
class ThreadState::NoAllocationScope final {
  STACK_ALLOCATED();

 public:
  explicit NoAllocationScope(ThreadState* state)
      : disallow_gc_(state->cpp_heap().GetHeapHandle()) {}

  NoAllocationScope(const NoAllocationScope&) = delete;
  NoAllocationScope& operator=(const NoAllocationScope&) = delete;

 private:
  const cppgc::subtle::DisallowGarbageCollectionScope disallow_gc_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_THREAD_STATE_SCOPES_H_

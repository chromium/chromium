// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_PROCESS_HEAP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_PROCESS_HEAP_H_

#include "gin/public/v8_platform.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/cppgc/platform.h"

namespace blink {

// TODO(1056170): Implement wrapper.
class PLATFORM_EXPORT ProcessHeap {
  STATIC_ONLY(ProcessHeap);

 public:
  static void Init() {
    cppgc::InitializeProcess(gin::V8Platform::Get()->GetPageAllocator());
  }

  static size_t TotalAllocatedObjectSize() { return 0; }

  static size_t TotalAllocatedSpace() { return 0; }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_PROCESS_HEAP_H_

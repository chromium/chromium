// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_BLINK_GC_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_BLINK_GC_H_

#include "third_party/blink/renderer/platform/heap/forward.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "v8/include/cppgc/liveness-broker.h"
#include "v8/include/cppgc/trace-trait.h"

namespace blink {

class PLATFORM_EXPORT BlinkGC final {
  STATIC_ONLY(BlinkGC);

 public:
  // When garbage collecting we need to know whether or not there can be
  // pointers to Oilpan-managed objects on the stack for each thread. When
  // threads reach a safe point they record whether or not they have pointers on
  // the stack.
  enum StackState { kNoHeapPointersOnStack, kHeapPointersOnStack };
};

using Address = uint8_t*;
using ConstAddress = const uint8_t*;

using TraceCallback = cppgc::TraceCallback;
using WeakCallback = void (*)(const cppgc::LivenessBroker&, const void*);
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_BLINK_GC_H_

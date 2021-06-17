// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_H_

#include "third_party/blink/renderer/platform/heap/v8_wrapper/process_heap.h"
#include "third_party/blink/renderer/platform/heap/v8_wrapper/thread_state.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/cppgc/garbage-collected.h"
#include "v8/include/cppgc/internal/pointer-policies.h"
#include "v8/include/cppgc/liveness-broker.h"

namespace blink {

using LivenessBroker = cppgc::LivenessBroker;

template <typename T>
using GarbageCollected = cppgc::GarbageCollected<T>;

// Default MakeGarbageCollected: Constructs an instance of T, which is a garbage
// collected type.
template <typename T, typename... Args>
T* MakeGarbageCollected(Args&&... args) {
  return cppgc::MakeGarbageCollected<T>(
      ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState()
          ->allocation_handle(),
      std::forward<Args>(args)...);
}

using AdditionalBytes = cppgc::AdditionalBytes;

// Constructs an instance of T, which is a garbage collected type. This special
// version takes size which enables constructing inline objects.
template <typename T, typename... Args>
T* MakeGarbageCollected(AdditionalBytes additional_bytes, Args&&... args) {
  return cppgc::MakeGarbageCollected<T>(
      ThreadStateFor<ThreadingTrait<T>::kAffinity>::GetState()
          ->allocation_handle(),
      std::forward<AdditionalBytes>(additional_bytes),
      std::forward<Args>(args)...);
}

static constexpr bool kBlinkGCHasDebugChecks =
    !std::is_same<cppgc::internal::DefaultMemberCheckingPolicy,
                  cppgc::internal::DisabledCheckingPolicy>::value;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_V8_WRAPPER_HEAP_H_

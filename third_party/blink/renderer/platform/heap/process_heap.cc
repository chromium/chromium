// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/process_heap.h"

#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/heap/gc_info.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/heap/persistent_node.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"

namespace blink {

namespace {

void BlinkGCAllocHook(uint8_t* address, size_t size, const char* context) {
  base::PoissonAllocationSampler::RecordAlloc(
      address, size, base::PoissonAllocationSampler::AllocatorType::kBlinkGC,
      context);
}

void BlinkGCFreeHook(uint8_t* address) {
  base::PoissonAllocationSampler::RecordFree(address);
}

}  // namespace

void ProcessHeap::Init() {
  DCHECK(!base::FeatureList::IsEnabled(
             blink::features::kBlinkHeapConcurrentMarking) ||
         base::FeatureList::IsEnabled(
             blink::features::kBlinkHeapIncrementalMarking));

  total_allocated_space_ = 0;
  total_allocated_object_size_ = 0;

  GCInfoTable::CreateGlobalTable();

  base::PoissonAllocationSampler::SetHooksInstallCallback([]() {
    HeapAllocHooks::SetAllocationHook(&BlinkGCAllocHook);
    HeapAllocHooks::SetFreeHook(&BlinkGCFreeHook);
  });
}

void ProcessHeap::ResetHeapCounters() {
  total_allocated_object_size_ = 0;
}

CrossThreadPersistentRegion& ProcessHeap::GetCrossThreadPersistentRegion() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CrossThreadPersistentRegion,
                                  persistent_region, ());
  return persistent_region;
}

CrossThreadPersistentRegion& ProcessHeap::GetCrossThreadWeakPersistentRegion() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CrossThreadPersistentRegion,
                                  persistent_region, ());
  return persistent_region;
}

Mutex& ProcessHeap::CrossThreadPersistentMutex() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, ());
  return mutex;
}

std::atomic_size_t ProcessHeap::total_allocated_space_{0};
std::atomic_size_t ProcessHeap::total_allocated_object_size_{0};

}  // namespace blink

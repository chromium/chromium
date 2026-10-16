// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/public/cppgc.h"

#include "base/allocator/dispatcher/notification_data.h"
#include "base/allocator/dispatcher/subsystem.h"
#include "base/check_op.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "build/build_config.h"
#include "gin/gin_features.h"
#include "gin/public/v8_platform.h"
#include "v8/include/cppgc/platform.h"

namespace gin {

namespace {

int g_init_count = 0;

class CppGCSamplerAdapter final : public cppgc::AllocationSampler {
 public:
  static CppGCSamplerAdapter* Get() {
    static base::NoDestructor<CppGCSamplerAdapter> instance;
    return instance.get();
  }

  size_t GetSampleInterval() const override {
    if (!base::PoissonAllocationSampler::IsRunning()) {
      return 0;
    }
    return base::PoissonAllocationSampler::Get()->SamplingInterval();
  }

  bool OnAllocated(void* address, size_t size) override {
    return base::PoissonAllocationSampler::Get()->OnAllocation(
        base::allocator::dispatcher::AllocationNotificationData(
            address, size, nullptr,
            base::allocator::dispatcher::AllocationSubsystem::kCppGC));
  }

  void OnFreed(void* address) override {
    base::PoissonAllocationSampler::Get()->OnFree(
        base::allocator::dispatcher::FreeNotificationData(
            address, base::allocator::dispatcher::AllocationSubsystem::kCppGC));
  }

  bool OnMove(void* from, void* to, size_t size) override {
    return base::PoissonAllocationSampler::Get()->OnMove(from, to);
  }
};

}  // namespace

void InitializeCppgcFromV8Platform() {
#if BUILDFLAG(IS_ANDROID)
  // Keep the cage size at 4GB on Android, since some vendors can configure the
  // kernel to have address space limited to 2^39 or 2^36 bits, which is more
  // prone to address space exhaustion.
  static constexpr size_t kCageSize =
      static_cast<size_t>(4) * 1024 * 1024 * 1024;
#else
  static constexpr size_t kCageSize =
      static_cast<size_t>(16) * 1024 * 1024 * 1024;
#endif

  DCHECK_GE(g_init_count, 0);
  if (g_init_count++ > 0) {
    return;
  }

  cppgc::InitializeProcess(gin::V8Platform::Get()->GetPageAllocator(),
                           kCageSize);
}

void MaybeShutdownCppgc() {
  DCHECK_GT(g_init_count, 0);
  if (--g_init_count > 0)
    return;

  cppgc::ShutdownProcess();
}

cppgc::AllocationSampler* GetCppGCAllocationSampler() {
  // In-process heap profiling (HeapProfilerController) starts
  // base::SamplingHeapProfiler during early process initialization
  // (PostEarlyInitialization), before Blink and CppHeap are initialized.
  // Checking IsRunning() here is therefore sufficient to attach the sampler
  // whenever startup heap profiling is enabled. For on-demand heap profiling
  // started later (e.g. via Perfetto tracing), kV8CppGCAllocationSampler can be
  // enabled to force-install the sampler at CppHeap creation time.
  if (!base::PoissonAllocationSampler::IsRunning() &&
      !base::FeatureList::IsEnabled(features::kV8CppGCAllocationSampler)) {
    return nullptr;
  }
  return CppGCSamplerAdapter::Get();
}

}  // namespace gin

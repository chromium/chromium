// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/stack_sampler_android.h"

#include "base/profiler/profile_builder.h"
#include "base/profiler/unwinder.h"
#include "base/trace_event/trace_event.h"

namespace tracing {
namespace {
constexpr size_t kMaxFrameDepth = 48;
}  // namespace

StackSamplerAndroid::StackSamplerAndroid(
    base::SamplingProfilerThreadToken thread_token,
    base::ModuleCache* module_cache)
    : thread_token_(thread_token), module_cache_(module_cache) {}

StackSamplerAndroid::~StackSamplerAndroid() = default;

// Unimplemented. StackSamplerAndroid needs to be implemented in terms of
// base::StackSamplerImpl to make use of this.
void StackSamplerAndroid::AddAuxUnwinder(
    std::unique_ptr<base::Unwinder> unwinder) {}

void StackSamplerAndroid::RecordStackFrames(
    base::StackBuffer* stack_buffer,
    base::ProfileBuilder* profile_builder) {
  if (!unwinder_.is_initialized()) {
    // May block on disk access. This function is executed on the profiler
    // thread, so this will only block profiling execution.
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cpu_profiler"),
                 "StackUnwinderAndroid::Initialize");
    unwinder_.Initialize();
  }
  const void* pcs[kMaxFrameDepth];
  size_t depth =
      unwinder_.TraceStack(thread_token_.id, stack_buffer, pcs, kMaxFrameDepth);
  std::vector<base::Frame> frames;
  frames.reserve(depth);
  for (size_t i = 0; i < depth; ++i) {
    uintptr_t address = reinterpret_cast<uintptr_t>(pcs[i]);
    frames.emplace_back(address, module_cache_->GetModuleForAddress(address));
  }
  profile_builder->OnSampleCompleted(std::move(frames), TRACE_TIME_TICKS_NOW());
}

}  // namespace tracing

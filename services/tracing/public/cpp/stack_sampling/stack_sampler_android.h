// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_STACK_SAMPLER_ANDROID_H_
#define SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_STACK_SAMPLER_ANDROID_H_

#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/profiler/stack_sampler.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/threading/platform_thread.h"
#include "services/tracing/public/cpp/stack_sampling/stack_unwinder_android.h"

namespace tracing {

// On Android the sampling implementation is delegated and this class just
// stores a callback to the real implementation.
class StackSamplerAndroid : public base::StackSampler {
 public:
  // StackUnwinderAndroid only supports sampling one thread at a time. So, the
  // clients of this class must ensure synchronization between multiple
  // instances of the sampler.
  explicit StackSamplerAndroid(base::SamplingProfilerThreadToken thread_token,
                               base::ModuleCache*);
  ~StackSamplerAndroid() override;

  StackSamplerAndroid(const StackSamplerAndroid&) = delete;
  StackSamplerAndroid& operator=(const StackSamplerAndroid&) = delete;

  // StackSampler:
  void AddAuxUnwinder(std::unique_ptr<base::Unwinder> unwinder) override;
  void RecordStackFrames(base::StackBuffer* stack_buffer,
                         base::ProfileBuilder* profile_builder) override;

 private:
  base::SamplingProfilerThreadToken thread_token_;
  base::ModuleCache* module_cache_;
  StackUnwinderAndroid unwinder_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_STACK_SAMPLER_ANDROID_H_

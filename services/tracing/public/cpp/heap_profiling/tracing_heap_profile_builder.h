// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_HEAP_PROFILING_TRACING_HEAP_PROFILE_BUILDER_H_
#define SERVICES_TRACING_PUBLIC_CPP_HEAP_PROFILING_TRACING_HEAP_PROFILE_BUILDER_H_

#include <vector>

#include "base/allocator/dispatcher/subsystem.h"
#include "base/component_export.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/memory/stack_allocated.h"
#include "base/profiler/module_cache.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"

namespace perfetto::protos::pbzero {
class InternedData;
class TracePacket;
}  // namespace perfetto::protos::pbzero

namespace tracing {

struct HeapProfilingIncrementalState;

class COMPONENT_EXPORT(TRACING_CPP) TracingHeapProfileBuilder {
  STACK_ALLOCATED();

 public:
  explicit TracingHeapProfileBuilder(HeapProfilingIncrementalState* incr_state);
  ~TracingHeapProfileBuilder();

  TracingHeapProfileBuilder(const TracingHeapProfileBuilder&) = delete;
  TracingHeapProfileBuilder& operator=(const TracingHeapProfileBuilder&) =
      delete;

  // Writes a single stack sample to the trace packet.
  // This may also write interned data (mappings, frames, callstacks) to the
  // packet if they haven't been emitted yet in the current sequence.
  void WriteSample(perfetto::protos::pbzero::TracePacket* trace_packet,
                   const base::SamplingHeapProfiler::Sample& sample,
                   base::ModuleCache& module_cache);

  // Writes the default configuration for stack samples (e.g. the source name).
  static void WriteDefaults(
      perfetto::protos::pbzero::TracePacket* trace_packet);

 private:
  uint32_t GetOrCreateCounterDescriptor(
      base::allocator::dispatcher::AllocationSubsystem allocator);

  uint32_t GetOrCreateCallstack(const std::vector<const void*>& stack,
                                base::ModuleCache& module_cache);

  uint32_t GetOrCreateFrame(const void* pc, base::ModuleCache& module_cache);

  uint32_t GetOrCreateMapping(const base::ModuleCache::Module* module);

  perfetto::protos::pbzero::InternedData* GetOrCreateInternedData();

  const char* AllocatorName(
      base::allocator::dispatcher::AllocationSubsystem allocator);

  RAW_PTR_EXCLUSION HeapProfilingIncrementalState* incr_state_ = nullptr;
  RAW_PTR_EXCLUSION perfetto::protos::pbzero::TracePacket* trace_packet_ =
      nullptr;
  RAW_PTR_EXCLUSION perfetto::protos::pbzero::InternedData* interned_data_ =
      nullptr;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_HEAP_PROFILING_TRACING_HEAP_PROFILE_BUILDER_H_

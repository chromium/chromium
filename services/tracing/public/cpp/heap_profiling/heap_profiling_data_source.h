// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_HEAP_PROFILING_HEAP_PROFILING_DATA_SOURCE_H_
#define SERVICES_TRACING_PUBLIC_CPP_HEAP_PROFILING_HEAP_PROFILING_DATA_SOURCE_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/component_export.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/threading/sequence_bound.h"
#include "services/tracing/public/cpp/perfetto/interning_index.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"

namespace perfetto {
class TraceWriterBase;
}  // namespace perfetto

namespace tracing {

struct COMPONENT_EXPORT(TRACING_CPP) HeapProfilingIncrementalState {
  bool was_cleared = true;

  // Interning indexes
  InterningIndex<TypeList<size_t>, SizeList<1024>> interned_callstacks_;
  InterningIndex<TypeList<std::pair<uintptr_t, std::string>>, SizeList<1024>>
      interned_frames_;
  InterningIndex<TypeList<std::string>, SizeList<1024>> interned_module_ids_;
  InterningIndex<TypeList<std::string>, SizeList<1024>> interned_module_paths_;
  InterningIndex<TypeList<uintptr_t>, SizeList<1024>> interned_modules_;

  // We use uint32_t for the enum value in the index key to keep includes
  // minimal.
  InterningIndex<TypeList<uint32_t>, SizeList<8>> interned_counters_;
};

class COMPONENT_EXPORT(TRACING_CPP) HeapProfilingDataSource
    : public perfetto::DataSource<HeapProfilingDataSource> {
 public:
  static constexpr bool kSupportsMultipleInstances = true;
  static constexpr bool kRequiresCallbacksUnderLock = false;

  static void Register();

  HeapProfilingDataSource();
  ~HeapProfilingDataSource() override;

  void OnSetup(const SetupArgs&) override;
  void OnStart(const StartArgs&) override;
  void OnStop(const StopArgs&) override;
  void WillClearIncrementalState(const ClearIncrementalStateArgs&) override;

 private:
  class HeapSampler;

  static std::unique_ptr<perfetto::TraceWriterBase> CreateTraceWriter(
      uint32_t instance_index);

  size_t sampling_interval_bytes_ = 0;
  uint32_t dump_interval_ms_ = 0;
  std::optional<base::SamplingHeapProfiler::Session> session_;
  base::SequenceBound<HeapSampler> sampler_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_HEAP_PROFILING_HEAP_PROFILING_DATA_SOURCE_H_

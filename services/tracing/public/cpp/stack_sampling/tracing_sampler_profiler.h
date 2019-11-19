// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_TRACING_SAMPLER_PROFILER_H_
#define SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_TRACING_SAMPLER_PROFILER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/sequence_checker.h"
#include "base/threading/platform_thread.h"
#include "services/tracing/public/cpp/perfetto/interning_index.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"

namespace tracing {

class PerfettoProducer;

// This class is a bridge between the base stack sampling profiler and chrome
// tracing. It's listening to TraceLog enabled/disabled events and it's starting
// a stack profiler on the current thread if needed. The sampling profiler is
// lazily instantiated when tracing is activated and released when tracing is
// disabled.
//
// The TracingSamplerProfiler must be created and destroyed on the sampled
// thread. The tracelog observers can be called on any thread which force the
// field |profiler_| to be thread-safe.
class COMPONENT_EXPORT(TRACING_CPP) TracingSamplerProfiler {
 public:
  // This class will receive the sampling profiler stackframes and output them
  // to the chrome trace via an event. Exposed for testing.
  class COMPONENT_EXPORT(TRACING_CPP) TracingProfileBuilder
      : public base::ProfileBuilder {
   public:
    TracingProfileBuilder(base::PlatformThreadId sampled_thread_id,
                          std::unique_ptr<perfetto::TraceWriter> trace_writer,
                          bool should_enable_filtering);
    ~TracingProfileBuilder() override;

    // base::ProfileBuilder
    base::ModuleCache* GetModuleCache() override;
    void OnSampleCompleted(std::vector<base::Frame> frames) override;
    void OnProfileCompleted(base::TimeDelta profile_duration,
                            base::TimeDelta sampling_period) override {}

    void SetTraceWriter(std::unique_ptr<perfetto::TraceWriter> trace_writer);

   private:
    struct BufferedSample {
      BufferedSample(base::TimeTicks, std::vector<base::Frame>&&);
      BufferedSample(BufferedSample&& other);
      ~BufferedSample();

      base::TimeTicks timestamp;
      std::vector<base::Frame> sample;

      DISALLOW_COPY_AND_ASSIGN(BufferedSample);
    };

    InterningID GetCallstackIDAndMaybeEmit(
        const std::vector<base::Frame>& frames,
        perfetto::TraceWriter::TracePacketHandle* trace_packet);
    void WriteSampleToTrace(const BufferedSample& sample);

    // We usually sample at 50ms, and expect that tracing should have started in
    // 10s.
    constexpr static size_t kMaxBufferedSamples = 200;
    std::vector<BufferedSample> buffered_samples_;

    base::ModuleCache module_cache_;
    const base::PlatformThreadId sampled_thread_id_;
    base::Lock trace_writer_lock_;
    std::unique_ptr<perfetto::TraceWriter> trace_writer_;
    InterningIndex<TypeList<size_t>, SizeList<1024>> interned_callstacks_{};
    InterningIndex<TypeList<std::pair<std::string, std::string>,
                            std::pair<uintptr_t, std::string>>,
                   SizeList<1024, 1024>>
        interned_frames_{};
    InterningIndex<TypeList<std::string>, SizeList<1024>>
        interned_frame_names_{};
    InterningIndex<TypeList<std::string>, SizeList<1024>>
        interned_module_names_{};
    InterningIndex<TypeList<std::string>, SizeList<1024>>
        interned_module_ids_{};
    InterningIndex<TypeList<uintptr_t>, SizeList<1024>> interned_modules_{};
    bool reset_incremental_state_ = true;
    uint32_t last_incremental_state_reset_id_ = 0;
    int32_t last_emitted_process_priority_ = -1;
    base::TimeTicks last_timestamp_;
    const bool should_enable_filtering_;
  };

  // Creates sampling profiler on main thread. The profiler *must* be
  // destroyed prior to process shutdown.
  static std::unique_ptr<TracingSamplerProfiler> CreateOnMainThread();

  // Sets up tracing sampling profiler on a child thread. The profiler will be
  // stored in SequencedLocalStorageSlot and will be destroyed with the thread
  // task runner.
  static void CreateOnChildThread();

  // Registers the TracingSamplerProfiler as a Perfetto data source
  static void RegisterDataSource();

  static void SetupStartupTracing();

  // For tests.
  static void DeleteOnChildThreadForTesting();
  static void StartTracingForTesting(tracing::PerfettoProducer* producer);
  static void StopTracingForTesting();

  explicit TracingSamplerProfiler(
      base::SamplingProfilerThreadToken sampled_thread_token);
  virtual ~TracingSamplerProfiler();

  void StartTracing(std::unique_ptr<perfetto::TraceWriter> trace_writer,
                    bool should_enable_filtering);
  void StopTracing();

 private:
  const base::SamplingProfilerThreadToken sampled_thread_token_;

  base::Lock lock_;
  std::unique_ptr<base::StackSamplingProfiler> profiler_;  // under |lock_|
  TracingProfileBuilder* profile_builder_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(TracingSamplerProfiler);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_TRACING_SAMPLER_PROFILER_H_

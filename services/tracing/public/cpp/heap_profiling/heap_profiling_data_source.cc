// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/heap_profiling/heap_profiling_data_source.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/profiler/module_cache.h"
#include "base/sampling_heap_profiler/poisson_allocation_sampler.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "services/tracing/public/cpp/heap_profiling/tracing_heap_profile_builder.h"
#include "services/tracing/public/cpp/perfetto/perfetto_data_source_names.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_descriptor.h"
#include "third_party/perfetto/include/perfetto/tracing/internal/tracing_muxer.h"
#include "third_party/perfetto/include/perfetto/tracing/trace_writer_base.h"
#include "third_party/perfetto/protos/perfetto/config/chrome/sampling_heap_profiler.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"

namespace tracing {

using perfetto::protos::gen::ChromeConfig;

class HeapProfilingDataSource::HeapSampler {
 public:
  HeapSampler(base::TimeDelta interval,
              base::SamplingHeapProfiler::Session session,
              std::unique_ptr<perfetto::TraceWriterBase> trace_writer)
      : session_(session), trace_writer_(std::move(trace_writer)) {
    if (!interval.is_zero()) {
      timer_.Start(FROM_HERE, interval, this, &HeapSampler::OnTimer);
    }
  }

  ~HeapSampler() {
    EmitSamples();
    base::SamplingHeapProfiler::Get()->Stop(session_);
    if (stop_callback_) {
      std::move(stop_callback_).Run();
    }
  }

  void SetStopCallback(base::OnceClosure cb) { stop_callback_ = std::move(cb); }

  void ClearIncrementalState() {
    incr_state_ = HeapProfilingIncrementalState();
  }

 private:
  void OnTimer() { EmitSamples(); }

  void EmitSamples() {
    if (!trace_writer_) {
      return;
    }
    auto samples = base::SamplingHeapProfiler::Get()->GetSamples(session_);
    WriteSamples(samples, module_cache_);
  }

  void WriteSamples(
      const std::vector<base::SamplingHeapProfiler::Sample>& samples,
      base::ModuleCache& module_cache) {
    uint64_t timestamp = base::TimeTicks::Now().since_origin().InNanoseconds();
    TracingHeapProfileBuilder builder(&incr_state_);
    for (const auto& sample : samples) {
      auto packet = trace_writer_->NewTracePacket();
      packet->set_timestamp(timestamp);
      builder.WriteSample(packet.get(), sample, module_cache);
    }
  }

  base::SamplingHeapProfiler::Session session_;
  std::unique_ptr<perfetto::TraceWriterBase> trace_writer_;
  HeapProfilingIncrementalState incr_state_;
  base::ModuleCache module_cache_;
  base::RepeatingTimer timer_;
  base::OnceClosure stop_callback_;
};

// static
void HeapProfilingDataSource::Register() {
  perfetto::DataSourceDescriptor desc;
  desc.set_name(kNativeHeapProfilerSourceName);
  perfetto::DataSource<HeapProfilingDataSource>::Register(desc);
}

HeapProfilingDataSource::HeapProfilingDataSource() = default;
HeapProfilingDataSource::~HeapProfilingDataSource() = default;

void HeapProfilingDataSource::OnSetup(const SetupArgs& args) {
  const std::string& raw_config =
      args.config->chromium_sampling_heap_profiler_raw();
  if (!raw_config.empty()) {
    perfetto::protos::gen::ChromiumSamplingHeapProfilerConfig config;
    if (config.ParseFromArray(raw_config.data(), raw_config.size())) {
      sampling_interval_bytes_ = config.sampling_interval_bytes();
      dump_interval_ms_ = config.sampling_interval_ms();
    }
  }

  if (sampling_interval_bytes_ == 0) {
    sampling_interval_bytes_ =
        base::PoissonAllocationSampler::kDefaultSamplingIntervalBytes;
  }
}

void HeapProfilingDataSource::OnStart(const StartArgs& args) {
  session_ = base::SamplingHeapProfiler::Get()->Start(
      base::ByteSize(sampling_interval_bytes_),
      base::SamplingHeapProfiler::Priority::kInteractive);
  if (!session_) {
    DLOG(WARNING) << "Failed to start SamplingHeapProfiler session";
    return;
  }

  base::TimeDelta dump_interval = base::Milliseconds(dump_interval_ms_);

  sampler_ = base::SequenceBound<HeapSampler>(
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN}),
      dump_interval, *session_,
      CreateTraceWriter(args.internal_instance_index));
}

void HeapProfilingDataSource::OnStop(const StopArgs& args) {
  auto stop_complete_callback = args.HandleStopAsynchronously();
  if (!sampler_) {
    stop_complete_callback();
    return;
  }
  base::OnceClosure cb =
      base::BindOnce([](auto callback) { callback(); }, stop_complete_callback);

  sampler_.AsyncCall(&HeapSampler::SetStopCallback).WithArgs(std::move(cb));
  sampler_.Reset();
}

void HeapProfilingDataSource::WillClearIncrementalState(
    const ClearIncrementalStateArgs& args) {
  if (sampler_) {
    sampler_.AsyncCall(&HeapSampler::ClearIncrementalState);
  }
}

}  // namespace tracing

PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS(
    COMPONENT_EXPORT(TRACING_CPP),
    tracing::HeapProfilingDataSource);

// This should go after PERFETTO_DEFINE_DATA_SOURCE_STATIC_MEMBERS_WITH_ATTRS
// to avoid instantiation of type() template method before specialization.
// static
std::unique_ptr<perfetto::TraceWriterBase>
tracing::HeapProfilingDataSource::CreateTraceWriter(uint32_t instance_index) {
  perfetto::internal::DataSourceStaticState* static_state =
      perfetto::DataSourceHelper<HeapProfilingDataSource>::type()
          .static_state();
  perfetto::internal::DataSourceState* instance_state =
      static_state->TryGet(instance_index);
  if (!instance_state) {
    return nullptr;
  }
  return perfetto::internal::TracingMuxer::Get()->CreateTraceWriter(
      static_state, instance_index, instance_state,
      perfetto::BufferExhaustedPolicy::kDrop);
}

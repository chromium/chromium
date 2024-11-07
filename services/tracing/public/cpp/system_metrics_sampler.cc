// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/system_metrics_sampler.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/power_monitor/cpu_frequency_utils.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_descriptor.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

// Psapi.h must come after Windows.h.
#include <psapi.h>
#endif  // BUILDFLAG(IS_WIN)

namespace tracing {

namespace {

constexpr base::TimeDelta kSamplingInterval = base::Seconds(5);

#if BUILDFLAG(IS_WIN)
// Returns memory in bytes from pages count.
size_t GetTotalMemory(size_t num_pages, size_t page_size) {
  return base::ValueOrDefaultForType<size_t>(
      base::CheckedNumeric(num_pages) * page_size, 0U);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

void SystemMetricsSampler::Register() {
  perfetto::DataSourceDescriptor desc;
  desc.set_name(tracing::mojom::kSystemMetricsSourceName);
  perfetto::DataSource<SystemMetricsSampler>::Register(desc);
}

SystemMetricsSampler::SystemMetricsSampler() = default;
SystemMetricsSampler::~SystemMetricsSampler() = default;

void SystemMetricsSampler::OnSetup(const SetupArgs& args) {}

void SystemMetricsSampler::OnStart(const StartArgs&) {
  sampler_ = base::SequenceBound<Sampler>(
      base::ThreadPool::CreateSequencedTaskRunner({}));
}

void SystemMetricsSampler::OnStop(const StopArgs&) {
  sampler_.Reset();
}

SystemMetricsSampler::Sampler::Sampler()
    : cpu_probe_{system_cpu::CpuProbe::Create()} {
  cpu_probe_->StartSampling();
  sample_timer_.Start(FROM_HERE, kSamplingInterval, this,
                      &Sampler::SampleSystemMetrics);
}

SystemMetricsSampler::Sampler::~Sampler() = default;

void SystemMetricsSampler::Sampler::SampleSystemMetrics() {
  cpu_probe_->RequestSample(
      base::BindOnce(&Sampler::OnCpuProbeResult, base::Unretained(this)));
  std::optional<base::CpuThroughputEstimationResult> cpu_throughput =
      base::EstimateCpuThroughput();
  if (cpu_throughput) {
    TRACE_COUNTER(TRACE_DISABLED_BY_DEFAULT("system_metrics"),
                  perfetto::CounterTrack("EstimatedCpuThroughput",
                                         perfetto::Track::Global(0)),
                  cpu_throughput->estimated_frequency);
  }

#if BUILDFLAG(IS_WIN)
  base::CpuFrequencyInfo cpu_info = base::GetCpuFrequencyInfo();
  TRACE_COUNTER(
      TRACE_DISABLED_BY_DEFAULT("system_metrics"),
      perfetto::CounterTrack("NumActiveCpus", perfetto::Track::Global(0)),
      cpu_info.num_active_cpus);

  SampleMemoryMetrics();
#endif
}

#if BUILDFLAG(IS_WIN)
void SystemMetricsSampler::Sampler::SampleMemoryMetrics() {
  PERFORMANCE_INFORMATION performance_info = {};
  performance_info.cb = sizeof(performance_info);
  bool get_performance_info_result =
      ::GetPerformanceInfo(&performance_info, sizeof(performance_info));
  if (!get_performance_info_result) {
    return;
  }

  TRACE_COUNTER(
      TRACE_DISABLED_BY_DEFAULT("system_metrics"),
      perfetto::CounterTrack("CommitMemoryLimit", perfetto::Track::Global(0)),
      GetTotalMemory(performance_info.CommitLimit, performance_info.PageSize));
  TRACE_COUNTER(
      TRACE_DISABLED_BY_DEFAULT("system_metrics"),
      perfetto::CounterTrack("CommitMemoryTotal", perfetto::Track::Global(0)),
      GetTotalMemory(performance_info.CommitTotal, performance_info.PageSize));
  TRACE_COUNTER(TRACE_DISABLED_BY_DEFAULT("system_metrics"),
                perfetto::CounterTrack("AvailablePhysicalMemory",
                                       perfetto::Track::Global(0)),
                GetTotalMemory(performance_info.PhysicalAvailable,
                               performance_info.PageSize));
}
#endif  // BUILDFLAG(IS_WIN)

void SystemMetricsSampler::Sampler::OnCpuProbeResult(
    std::optional<system_cpu::CpuSample> cpu_sample) {
  if (!cpu_sample) {
    return;
  }
  TRACE_COUNTER(
      TRACE_DISABLED_BY_DEFAULT("system_metrics"),
      perfetto::CounterTrack("SystemCpuUsage", perfetto::Track::Global(0)),
      cpu_sample->cpu_utilization);
}

}  // namespace tracing

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
#include "third_party/perfetto/protos/perfetto/config/chrome/system_metrics.gen.h"
#include "third_party/perfetto/protos/perfetto/config/data_source_config.gen.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

// Psapi.h must come after Windows.h.
#include <psapi.h>
#endif  // BUILDFLAG(IS_WIN)

namespace tracing {

namespace {

constexpr base::TimeDelta kDefaultSamplingInterval = base::Seconds(5);

#if BUILDFLAG(IS_WIN)
// Returns memory in bytes from pages count.
size_t GetTotalMemory(size_t num_pages, size_t page_size) {
  return base::ValueOrDefaultForType<size_t>(
      base::CheckedNumeric(num_pages) * page_size, 0U);
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

void SystemMetricsSampler::Register(bool system_wide) {
  perfetto::DataSourceDescriptor desc;
  desc.set_name(tracing::mojom::kSystemMetricsSourceName);
  perfetto::DataSource<SystemMetricsSampler>::Register(desc, system_wide);
}

SystemMetricsSampler::SystemMetricsSampler(bool system_wide)
    : system_wide_(system_wide), sampling_interval_(kDefaultSamplingInterval) {}
SystemMetricsSampler::~SystemMetricsSampler() = default;

void SystemMetricsSampler::OnSetup(const SetupArgs& args) {
  if (args.config->chromium_system_metrics_raw().empty()) {
    return;
  }
  perfetto::protos::gen::ChromiumSystemMetricsConfig config;
  if (!config.ParseFromString(args.config->chromium_system_metrics_raw())) {
    DLOG(ERROR) << "Failed to parse chromium_system_metrics";
    return;
  }
  if (config.has_sampling_interval_ms()) {
    sampling_interval_ = base::Milliseconds(config.sampling_interval_ms());
  }
}

void SystemMetricsSampler::OnStart(const StartArgs&) {
  if (system_wide_) {
    system_sampler_ = base::SequenceBound<SystemSampler>(
        base::ThreadPool::CreateSequencedTaskRunner({}), sampling_interval_);
  }
  process_sampler_ = base::SequenceBound<ProcessSampler>(
      base::ThreadPool::CreateSequencedTaskRunner({}), sampling_interval_);
}

void SystemMetricsSampler::OnStop(const StopArgs&) {
  system_sampler_.Reset();
  process_sampler_.Reset();
}

SystemMetricsSampler::SystemSampler::SystemSampler(
    base::TimeDelta sampling_interval)
    : cpu_probe_{system_cpu::CpuProbe::Create()} {
  cpu_probe_->StartSampling();
  sample_timer_.Start(FROM_HERE, sampling_interval, this,
                      &SystemSampler::SampleSystemMetrics);
}

SystemMetricsSampler::SystemSampler::~SystemSampler() = default;

void SystemMetricsSampler::SystemSampler::SampleSystemMetrics() {
  cpu_probe_->RequestSample(
      base::BindOnce(&SystemSampler::OnCpuProbeResult, base::Unretained(this)));
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
void SystemMetricsSampler::SystemSampler::SampleMemoryMetrics() {
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

void SystemMetricsSampler::SystemSampler::OnCpuProbeResult(
    std::optional<system_cpu::CpuSample> cpu_sample) {
  if (!cpu_sample) {
    return;
  }
  TRACE_COUNTER(
      TRACE_DISABLED_BY_DEFAULT("system_metrics"),
      perfetto::CounterTrack("SystemCpuUsage", perfetto::Track::Global(0)),
      cpu_sample->cpu_utilization);
}

SystemMetricsSampler::ProcessSampler::ProcessSampler(
    base::TimeDelta sampling_interval) {
  process_metrics_ = base::ProcessMetrics::CreateCurrentProcessMetrics();
  SampleProcessMetrics();
  sample_timer_.Start(FROM_HERE, sampling_interval, this,
                      &ProcessSampler::SampleProcessMetrics);
}

SystemMetricsSampler::ProcessSampler::~ProcessSampler() = default;

void SystemMetricsSampler::ProcessSampler::SampleProcessMetrics() {
  auto cpu_usage = process_metrics_->GetPlatformIndependentCPUUsage();
  if (!cpu_usage.has_value()) {
    return;
  }
  TRACE_COUNTER(TRACE_DISABLED_BY_DEFAULT("system_metrics"), "CpuUsage",
                *cpu_usage);
}

}  // namespace tracing

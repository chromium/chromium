// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/system_metrics_sampler.h"

#include "base/check.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_descriptor.h"

namespace tracing {

namespace {

constexpr base::TimeDelta kSamplingInterval = base::Seconds(5);

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
}

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

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/process_metrics_sampler.h"

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

void ProcessMetricsSampler::Register() {
  perfetto::DataSourceDescriptor desc;
  desc.set_name(tracing::mojom::kSystemMetricsSourceName);
  perfetto::DataSource<ProcessMetricsSampler>::Register(desc);
}

ProcessMetricsSampler::ProcessMetricsSampler() = default;
ProcessMetricsSampler::~ProcessMetricsSampler() = default;

void ProcessMetricsSampler::OnSetup(const SetupArgs& args) {}

void ProcessMetricsSampler::OnStart(const StartArgs&) {
  sampler_ = base::SequenceBound<Sampler>(
      base::ThreadPool::CreateSequencedTaskRunner({}));
}

void ProcessMetricsSampler::OnStop(const StopArgs&) {
  sampler_.Reset();
}

ProcessMetricsSampler::Sampler::Sampler() {
  process_metrics_ = base::ProcessMetrics::CreateCurrentProcessMetrics();
  SampleProcessMetrics();
  sample_timer_.Start(FROM_HERE, kSamplingInterval, this,
                      &Sampler::SampleProcessMetrics);
}

ProcessMetricsSampler::Sampler::~Sampler() = default;

void ProcessMetricsSampler::Sampler::SampleProcessMetrics() {
  auto cpu_usage = process_metrics_->GetPlatformIndependentCPUUsage();
  if (!cpu_usage.has_value()) {
    return;
  }
  TRACE_COUNTER(TRACE_DISABLED_BY_DEFAULT("system_metrics"), "CpuUsage",
                *cpu_usage);
}

}  // namespace tracing

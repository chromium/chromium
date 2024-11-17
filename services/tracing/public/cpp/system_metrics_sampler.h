// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_SYSTEM_METRICS_SAMPLER_H_
#define SERVICES_TRACING_PUBLIC_CPP_SYSTEM_METRICS_SAMPLER_H_

#include "base/component_export.h"
#include "base/process/process_metrics.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "components/system_cpu/cpu_probe.h"
#include "third_party/perfetto/include/perfetto/tracing/core/forward_decls.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"

namespace tracing {

// A data source that periodically samples and emits system metrics.
class COMPONENT_EXPORT(TRACING_CPP) SystemMetricsSampler final
    : public perfetto::DataSource<SystemMetricsSampler> {
 public:
  // `system_wide` enables recording system wide metrics (e.g. total cpu).
  static void Register(bool system_wide);

  explicit SystemMetricsSampler(bool system_wide);
  ~SystemMetricsSampler() override;

  SystemMetricsSampler(const SystemMetricsSampler& other) = delete;
  SystemMetricsSampler& operator=(const SystemMetricsSampler& other) = delete;

  void OnSetup(const SetupArgs&) override;
  void OnStart(const StartArgs&) override;
  void OnStop(const StopArgs&) override;

 private:
  // Samples system metrics on the sampling thread.
  class SystemSampler {
   public:
    explicit SystemSampler(base::TimeDelta sampling_interval);
    ~SystemSampler();

    SystemSampler(const SystemSampler& other) = delete;
    SystemSampler& operator=(const SystemSampler& other) = delete;

   private:
    void SampleSystemMetrics();
    void OnCpuProbeResult(std::optional<system_cpu::CpuSample> cpu_sample);

#if BUILDFLAG(IS_WIN)
    void SampleMemoryMetrics();
#endif

    base::RepeatingTimer sample_timer_;
    std::unique_ptr<system_cpu::CpuProbe> cpu_probe_;
  };
  // Samples process metrics on the sampling thread.
  class ProcessSampler {
   public:
    explicit ProcessSampler(base::TimeDelta sampling_interval);
    ~ProcessSampler();

    ProcessSampler(const ProcessSampler& other) = delete;
    ProcessSampler& operator=(const ProcessSampler& other) = delete;

   private:
    void SampleProcessMetrics();

    base::RepeatingTimer sample_timer_;
    std::unique_ptr<base::ProcessMetrics> process_metrics_;
  };

  bool system_wide_;
  base::TimeDelta sampling_interval_;
  base::SequenceBound<SystemSampler> system_sampler_;
  base::SequenceBound<ProcessSampler> process_sampler_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_SYSTEM_METRICS_SAMPLER_H_

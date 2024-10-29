// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PROCESS_METRICS_SAMPLER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PROCESS_METRICS_SAMPLER_H_

#include "base/component_export.h"
#include "base/process/process_metrics.h"
#include "base/threading/sequence_bound.h"
#include "base/threading/thread.h"
#include "base/timer/timer.h"
#include "third_party/perfetto/include/perfetto/tracing/core/forward_decls.h"
#include "third_party/perfetto/include/perfetto/tracing/data_source.h"

namespace tracing {

// A data source that periodically samples and emits process metrics.
class COMPONENT_EXPORT(TRACING_CPP) ProcessMetricsSampler final
    : public perfetto::DataSource<ProcessMetricsSampler> {
 public:
  static void Register();

  ProcessMetricsSampler();
  ~ProcessMetricsSampler() override;

  ProcessMetricsSampler(const ProcessMetricsSampler& other) = delete;
  ProcessMetricsSampler& operator=(const ProcessMetricsSampler& other) = delete;

  void OnSetup(const SetupArgs&) override;
  void OnStart(const StartArgs&) override;
  void OnStop(const StopArgs&) override;

 private:
  // Samples process metrics on the sampling thread.
  class Sampler {
   public:
    Sampler();
    ~Sampler();

    Sampler(const Sampler& other) = delete;
    Sampler& operator=(const Sampler& other) = delete;

   private:
    void SampleProcessMetrics();

    base::RepeatingTimer sample_timer_;
    std::unique_ptr<base::ProcessMetrics> process_metrics_;
  };

  base::SequenceBound<Sampler> sampler_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PROCESS_METRICS_SAMPLER_H_

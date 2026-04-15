// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/common_data_sources.h"

#include "base/trace_event/trace_event.h"
#include "services/tracing/public/cpp/perfetto/custom_event_recorder.h"
#include "services/tracing/public/cpp/perfetto/histogram_samples_data_source.h"
#include "services/tracing/public/cpp/perfetto/track_name_recorder.h"
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "services/tracing/public/cpp/system_metrics_sampler.h"

namespace tracing {

void RegisterCommonPerfettoDataSources(bool enable_consumer) {
  base::TrackEvent::Register();
  TracingSamplerProfiler::RegisterDataSource();
  HistogramSamplesDataSource::Register();
  // SystemMetricsSampler will be started when enabling
  // kSystemMetricsSourceName.
  SystemMetricsSampler::Register(/*system_wide=*/enable_consumer);
  TrackNameRecorder::GetInstance();
  CustomEventRecorder::GetInstance();
}

}  // namespace tracing

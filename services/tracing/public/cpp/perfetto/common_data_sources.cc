// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/common_data_sources.h"

#include "base/check.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/trace_event/trace_event.h"
#include "build/blink_buildflags.h"
#include "services/tracing/public/cpp/perfetto/custom_event_recorder.h"
#include "services/tracing/public/cpp/perfetto/histogram_samples_data_source.h"
#include "services/tracing/public/cpp/perfetto/track_name_recorder.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"

#if BUILDFLAG(USE_BLINK)
#include "services/tracing/public/cpp/heap_profiling/heap_profiling_data_source.h"
#endif
#include "services/tracing/public/cpp/stack_sampling/tracing_sampler_profiler.h"
#include "services/tracing/public/cpp/system_metrics_sampler.h"

namespace tracing {

void RegisterCommonPerfettoDataSources(bool enable_consumer,
                                       bool defer_thread_pool_data_sources) {
  base::TrackEvent::Register();
  TracingSamplerProfiler::RegisterDataSource();
  HistogramSamplesDataSource::Register();
  TrackNameRecorder::GetInstance();
  CustomEventRecorder::GetInstance();

  if (!defer_thread_pool_data_sources) {
    RegisterThreadPoolDependentDataSources(enable_consumer);
  }
}

void RegisterThreadPoolDependentDataSources(bool enable_consumer) {
  CHECK(base::ThreadPoolInstance::Get() && perfetto::Tracing::IsInitialized());
#if BUILDFLAG(USE_BLINK)
  HeapProfilingDataSource::Register();
#endif
  // SystemMetricsSampler will be started when enabling
  // kSystemMetricsSourceName.
  SystemMetricsSampler::Register(/*system_wide=*/enable_consumer);
}

}  // namespace tracing

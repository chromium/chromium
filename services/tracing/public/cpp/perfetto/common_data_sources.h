// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_COMMON_DATA_SOURCES_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_COMMON_DATA_SOURCES_H_

#include "base/component_export.h"

namespace tracing {

// Registers the Perfetto data sources that are common to all platforms (e.g.
// TrackEvent, HistogramSamples, SystemMetrics, etc).
//
// If `defer_thread_pool_data_sources` is true, data sources that depend on
// `base::ThreadPool` (e.g. HeapProfilingDataSource, SystemMetricsSampler) are
// skipped and the caller must invoke `RegisterThreadPoolDependentDataSources()`
// once `base::ThreadPool` has been created.
void COMPONENT_EXPORT(TRACING_CPP) RegisterCommonPerfettoDataSources(
    bool enable_consumer,
    bool defer_thread_pool_data_sources = false);

// Registers data sources that depend on `base::ThreadPool`.
// Must only be called once `base::ThreadPool` has been created and Perfetto
// tracing has been initialized. Embedders should only call this explicitly if
// `defer_thread_pool_data_sources` was passed as true to
// `RegisterCommonPerfettoDataSources`.
void COMPONENT_EXPORT(TRACING_CPP)
    RegisterThreadPoolDependentDataSources(bool enable_consumer);

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_COMMON_DATA_SOURCES_H_

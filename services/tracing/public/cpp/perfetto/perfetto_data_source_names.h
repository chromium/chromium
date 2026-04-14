// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_DATA_SOURCE_NAMES_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_DATA_SOURCE_NAMES_H_

namespace tracing {

// Producer processes register with the format
// "kPerfettoProducerNamePrefix-PID" when connecting to Chrome's internal
// tracing service. Note that system producers use a different but similar
// naming scheme to disambiguate between different apps on the same system,
// see SystemProducer::ConnectToSystemService() implementations. Of
// particular interest is PosixSystemProducer::ConnectSocket().
inline constexpr char kPerfettoProducerNamePrefix[] = "org.chromium-";
inline constexpr char kTraceEventDataSourceName[] = "org.chromium.trace_event";
inline constexpr char kMemoryInstrumentationDataSourceName[] =
    "org.chromium.memory_instrumentation";
inline constexpr char kMetaData2SourceName[] = "org.chromium.trace_metadata2";
inline constexpr char kSystemTraceDataSourceName[] =
    "org.chromium.trace_system";
inline constexpr char kArcTraceDataSourceName[] = "org.chromium.trace_arc";
inline constexpr char kSamplerProfilerSourceName[] =
    "org.chromium.sampler_profiler";
inline constexpr char kJavaHeapProfilerSourceName[] =
    "org.chromium.java_heap_profiler";
inline constexpr char kNativeHeapProfilerSourceName[] =
    "org.chromium.native_heap_profiler";
inline constexpr char kSystemMetricsSourceName[] =
    "org.chromium.system_metrics";
inline constexpr char kHistogramSampleSourceName[] =
    "org.chromium.histogram_sample";

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PERFETTO_DATA_SOURCE_NAMES_H_

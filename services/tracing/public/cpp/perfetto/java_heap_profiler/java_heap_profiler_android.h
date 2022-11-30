// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_JAVA_HEAP_PROFILER_JAVA_HEAP_PROFILER_ANDROID_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_JAVA_HEAP_PROFILER_JAVA_HEAP_PROFILER_ANDROID_H_

#include "base/component_export.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"

namespace tracing {

// This is a Java heap profiler on Android that provides heap dumps to
// tracing The profiler is enabled based on
// DISABLED_BY_DEFAULT("java_heap_profiler") category.
class COMPONENT_EXPORT(TRACING_CPP) JavaHeapProfiler
    : public PerfettoTracedProcess::DataSourceBase {
 public:
  static JavaHeapProfiler* GetInstance();

  JavaHeapProfiler();

  // PerfettoTracedProcess::DataSourceBase implementation:
  void StartTracingImpl(
      PerfettoProducer* producer,
      const perfetto::DataSourceConfig& data_source_config) override;
  void StopTracingImpl(base::OnceClosure stop_complete_callback) override;
  void Flush(base::RepeatingClosure flush_complete_callback) override;
};
}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_JAVA_HEAP_PROFILER_JAVA_HEAP_PROFILER_ANDROID_H_

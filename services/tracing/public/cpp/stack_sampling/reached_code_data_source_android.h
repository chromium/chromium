// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_REACHED_CODE_DATA_SOURCE_ANDROID_H_
#define SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_REACHED_CODE_DATA_SOURCE_ANDROID_H_

#include <memory>

#include "base/component_export.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"

namespace tracing {

class COMPONENT_EXPORT(TRACING_CPP) ReachedCodeDataSource
    : public PerfettoTracedProcess::DataSourceBase {
 public:
  static ReachedCodeDataSource* Get();

  ReachedCodeDataSource();
  ~ReachedCodeDataSource() override;

  // PerfettoTracedProcess::DataSourceBase implementation, called by
  // ProducerClient.
  void StartTracing(
      PerfettoProducer* producer,
      const perfetto::DataSourceConfig& data_source_config) override;
  void StopTracing(base::OnceClosure stop_complete_callback) override;
  void Flush(base::RepeatingClosure flush_complete_callback) override;
  void ClearIncrementalState() override;

  ReachedCodeDataSource(ReachedCodeDataSource&&) = delete;
  ReachedCodeDataSource& operator=(ReachedCodeDataSource&&) = delete;

 private:
  void WriteProfileData();

  std::unique_ptr<perfetto::TraceWriter> trace_writer_;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_STACK_SAMPLING_REACHED_CODE_DATA_SOURCE_ANDROID_H_

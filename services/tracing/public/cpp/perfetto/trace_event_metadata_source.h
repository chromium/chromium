// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_EVENT_METADATA_SOURCE_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_EVENT_METADATA_SOURCE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted_memory.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/trace_event/trace_config.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "third_party/perfetto/protos/perfetto/trace/chrome/chrome_metadata.pbzero.h"

namespace tracing {

// This class is a data source that clients can use to provide
// global metadata in dictionary form, by registering callbacks.
class COMPONENT_EXPORT(TRACING_CPP) TraceEventMetadataSource
    : public PerfettoTracedProcess::DataSourceBase {
 public:
  static TraceEventMetadataSource* GetInstance();

  TraceEventMetadataSource(const TraceEventMetadataSource&) = delete;
  TraceEventMetadataSource& operator=(const TraceEventMetadataSource&) = delete;

  using JsonMetadataGeneratorFunction =
      base::RepeatingCallback<std::optional<base::Value::Dict>()>;

  using MetadataGeneratorFunction = base::RepeatingCallback<void(
      perfetto::protos::pbzero::ChromeMetadataPacket*,
      bool /* privacy_filtering_enabled */)>;

  using PacketGeneratorFunction =
      base::RepeatingCallback<void(perfetto::protos::pbzero::TracePacket*,
                                   bool /* privacy_filtering_enabled */)>;

  // Any callbacks passed here will be called when tracing. Note that if tracing
  // is enabled while calling this method, the callback may be invoked
  // directly.
  void AddGeneratorFunction(JsonMetadataGeneratorFunction generator);
  // Same as above, but for filling in proto format.
  void AddGeneratorFunction(MetadataGeneratorFunction generator);
  void AddGeneratorFunction(PacketGeneratorFunction generator);

  // PerfettoTracedProcess::DataSourceBase implementation:
  void StartTracingImpl(
      const perfetto::DataSourceConfig& data_source_config) override;
  void StopTracingImpl(base::OnceClosure stop_complete_callback) override;
  void Flush(base::RepeatingClosure flush_complete_callback) override;
  base::SequencedTaskRunner* GetTaskRunner() override;

  void ResetForTesting();

  using DataSourceProxy =
      PerfettoTracedProcess::DataSourceProxy<TraceEventMetadataSource>;

 private:
  friend class base::NoDestructor<TraceEventMetadataSource>;
  friend class perfetto::DataSource<TraceEventMetadataSource>;

  TraceEventMetadataSource();
  ~TraceEventMetadataSource() override;

  void GenerateMetadata(
      std::unique_ptr<std::vector<JsonMetadataGeneratorFunction>>
          json_generators,
      std::unique_ptr<std::vector<MetadataGeneratorFunction>> proto_generators,
      std::unique_ptr<std::vector<PacketGeneratorFunction>> packet_generators);
  void GenerateMetadataFromGenerator(
      const MetadataGeneratorFunction& generator);
  void GenerateJsonMetadataFromGenerator(
      const JsonMetadataGeneratorFunction& generator,
      perfetto::protos::pbzero::ChromeEventBundle* event_bundle);
  void GenerateMetadataPacket(
      const TraceEventMetadataSource::PacketGeneratorFunction& generator);

  void WriteMetadataPacket(perfetto::protos::pbzero::ChromeMetadataPacket*,
                           bool privacy_filtering_enabled);
  std::optional<base::Value::Dict> GenerateTraceConfigMetadataDict();

  // All members are protected by |lock_|.
  // TODO(crbug.com/40153181): Change annotations to GUARDED_BY
  base::Lock lock_;
  std::vector<JsonMetadataGeneratorFunction> json_generator_functions_
      GUARDED_BY(lock_);
  std::vector<MetadataGeneratorFunction> generator_functions_ GUARDED_BY(lock_);
  std::vector<PacketGeneratorFunction> packet_generator_functions_
      GUARDED_BY(lock_);

  const scoped_refptr<base::SequencedTaskRunner> origin_task_runner_
      GUARDED_BY_FIXME(lock_);

  bool privacy_filtering_enabled_ GUARDED_BY_FIXME(lock_) = false;
  std::string chrome_config_ GUARDED_BY(lock_);
  std::unique_ptr<base::trace_event::TraceConfig> parsed_chrome_config_
      GUARDED_BY(lock_);
  bool emit_metadata_at_start_ GUARDED_BY(lock_) = false;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_TRACE_EVENT_METADATA_SOURCE_H_

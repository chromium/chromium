// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_TRACING_OBSERVER_PROTO_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_TRACING_OBSERVER_PROTO_H_

#include "base/callback.h"
#include "base/component_export.h"
#include "base/thread_annotations.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "services/tracing/public/cpp/perfetto/perfetto_traced_process.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"

namespace perfetto {
namespace protos {
namespace pbzero {
class SmapsPacket;
}
}  // namespace protos
}  // namespace perfetto

namespace memory_instrumentation {

// Version of TracingObserver that serializes the dump into a proto TracePacket.
class COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION)
    TracingObserverProto
    : public TracingObserver,
      public tracing::PerfettoTracedProcess::DataSourceBase {
 public:
  TracingObserverProto(base::trace_event::TraceLog*,
                       base::trace_event::MemoryDumpManager*);

  TracingObserverProto(const TracingObserverProto&) = delete;
  TracingObserverProto& operator=(const TracingObserverProto&) = delete;

  ~TracingObserverProto() override;

  static void RegisterForTesting();

  bool AddChromeDumpToTraceIfEnabled(
      const base::trace_event::MemoryDumpRequestArgs&,
      const base::ProcessId pid,
      const base::trace_event::ProcessMemoryDump*,
      const base::TimeTicks& timestamp) override;
  bool AddOsDumpToTraceIfEnabled(
      const base::trace_event::MemoryDumpRequestArgs&,
      const base::ProcessId,
      const mojom::OSMemDump&,
      const std::vector<mojom::VmRegionPtr>&,
      const base::TimeTicks& timestamp) override;

#if !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  void StartTracingImpl(
      tracing::PerfettoProducer* producer,
      const perfetto::DataSourceConfig& data_source_config) override;

  void StopTracingImpl(
      base::OnceClosure stop_complete_callback = base::OnceClosure()) override;

  void Flush(base::RepeatingClosure flush_complete_callback) override;
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

  static void MemoryMapsAsProtoInto(
      const std::vector<mojom::VmRegionPtr>& memory_maps,
      perfetto::protos::pbzero::SmapsPacket* smaps,
      bool is_argument_filtering_enabled);

#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  using DataSourceProxy =
      tracing::PerfettoTracedProcess::DataSourceProxy<TracingObserverProto>;
#endif

 private:
#if BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  friend class perfetto::DataSource<TracingObserverProto>;
#else   // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)
  base::Lock writer_lock_;
  std::unique_ptr<perfetto::TraceWriter> trace_writer_ GUARDED_BY(writer_lock_);
#endif  // !BUILDFLAG(USE_PERFETTO_CLIENT_LIBRARY)

  static tracing::PerfettoTracedProcess::DataSourceBase* instance_for_testing_;
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_TRACING_OBSERVER_PROTO_H_

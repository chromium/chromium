// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer.h"
#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"

#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"

namespace memory_instrumentation {

using base::trace_event::TracedValue;
using base::trace_event::ProcessMemoryDump;

namespace {

}  // namespace

TracingObserver::TracingObserver()
    : tracing::PerfettoTracedProcess::DataSourceBase(
          tracing::mojom::kMemoryInstrumentationDataSourceName) {
  perfetto::DataSourceDescriptor dsd;
  dsd.set_name(name());
  DataSourceProxy::Register(dsd, this);
  tracing::PerfettoTracedProcess::Get()->AddDataSource(this);
}

TracingObserver::~TracingObserver() = default;

void TracingObserver::StartTracingImpl(
    tracing::PerfettoProducer* producer,
    const perfetto::DataSourceConfig& data_source_config) {
  const base::trace_event::TraceConfig trace_config{
      data_source_config.chrome_config().trace_config()};
  const base::trace_event::TraceConfig::MemoryDumpConfig& memory_dump_config =
      trace_config.memory_dump_config();

  {
    base::AutoLock lock(memory_dump_config_lock_);
    memory_dump_config_ =
        std::make_unique<base::trace_event::TraceConfig::MemoryDumpConfig>(
            memory_dump_config);
  }

  auto* mdm = base::trace_event::MemoryDumpManager::GetInstance();
  if (mdm->IsInitialized()) {
    mdm->SetupForTracing(memory_dump_config);
  }
}

void TracingObserver::StopTracingImpl(
    base::OnceClosure stop_complete_callback) {
  base::trace_event::MemoryDumpManager::GetInstance()->TeardownForTracing();

  {
    base::AutoLock lock(memory_dump_config_lock_);
    memory_dump_config_.reset();
  }

  if (stop_complete_callback) {
    std::move(stop_complete_callback).Run();
  }
}

void TracingObserver::Flush(base::RepeatingClosure flush_complete_callback) {
  DataSourceProxy::Trace(
      [&](DataSourceProxy::TraceContext ctx) { ctx.Flush(); });

  if (flush_complete_callback) {
    std::move(flush_complete_callback).Run();
  }
}

bool TracingObserver::AddChromeDumpToTraceIfEnabled(
    const base::trace_event::MemoryDumpRequestArgs&,
    const base::ProcessId pid,
    const base::trace_event::ProcessMemoryDump*,
    const base::TimeTicks& timestamp) {
  return false;
}

bool TracingObserver::AddOsDumpToTraceIfEnabled(
    const base::trace_event::MemoryDumpRequestArgs& args,
    const base::ProcessId pid,
    const mojom::OSMemDump& os_dump,
    const std::vector<mojom::VmRegionPtr>& memory_maps,
    const base::TimeTicks& timestamp) {
  return false;
}

bool TracingObserver::ShouldAddToTrace(
    const base::trace_event::MemoryDumpRequestArgs& args) {
  // If tracing has been disabled early out to avoid the cost of serializing the
  // dump then ignoring the result.
  if (!IsMemoryInfraTracingEnabled())
    return false;
  // If the dump mode is too detailed don't add to trace to avoid accidentally
  // including PII.
  if (!IsDumpModeAllowed(args.level_of_detail))
    return false;
  return true;
}

// static
std::string TracingObserver::ApplyPathFiltering(
    const std::string& file,
    bool is_argument_filtering_enabled) {
  if (is_argument_filtering_enabled) {
    base::FilePath::StringType path(file.begin(), file.end());
    return base::FilePath(path).BaseName().AsUTF8Unsafe();
  }
  return file;
}

bool TracingObserver::IsDumpModeAllowed(
    base::trace_event::MemoryDumpLevelOfDetail dump_mode) const {
  base::AutoLock lock(memory_dump_config_lock_);
  if (!memory_dump_config_)
    return false;
  return memory_dump_config_->allowed_dump_modes.count(dump_mode) != 0;
}

bool TracingObserver::IsMemoryInfraTracingEnabled() const {
  bool enabled = false;
  DataSourceProxy::Trace(
      [&](DataSourceProxy::TraceContext) { enabled = true; });
  return enabled;
}

}  // namespace memory_instrumentation

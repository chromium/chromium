// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer.h"

#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"

namespace memory_instrumentation {

using base::trace_event::TracedValue;
using base::trace_event::ProcessMemoryDump;

namespace {

bool IsMemoryInfraTracingEnabled() {
  bool enabled;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(
      base::trace_event::MemoryDumpManager::kTraceCategory, &enabled);
  return enabled;
}

}  // namespace

TracingObserver::TracingObserver(
    base::trace_event::TraceLog* trace_log,
    base::trace_event::MemoryDumpManager* memory_dump_manager)
    : memory_dump_manager_(memory_dump_manager), trace_log_(trace_log) {
  // If tracing was enabled before initializing MemoryDumpManager, we missed the
  // OnTraceLogEnabled() event. Synthesize it so we can late-join the party.
  // IsEnabled is called before adding observer to avoid calling
  // OnTraceLogEnabled twice.
  bool is_tracing_already_enabled = trace_log_->IsEnabled();
  trace_log_->AddEnabledStateObserver(this);
  if (is_tracing_already_enabled)
    OnTraceLogEnabled();
}

TracingObserver::~TracingObserver() {
  trace_log_->RemoveEnabledStateObserver(this);
}

void TracingObserver::OnTraceLogEnabled() {
  if (!IsMemoryInfraTracingEnabled())
    return;

  // Initialize the TraceLog for the current thread. This is to avoids that the
  // TraceLog memory dump provider is registered lazily during the MDM
  // SetupForTracing().
  base::trace_event::TraceLog::GetInstance()
      ->InitializeThreadLocalEventBufferIfSupported();

  const base::trace_event::TraceConfig& trace_config =
      base::trace_event::TraceLog::GetInstance()->GetCurrentTraceConfig();
  const base::trace_event::TraceConfig::MemoryDumpConfig& memory_dump_config =
      trace_config.memory_dump_config();

  memory_dump_config_ =
      std::make_unique<base::trace_event::TraceConfig::MemoryDumpConfig>(
          memory_dump_config);

  if (memory_dump_manager_)
    memory_dump_manager_->SetupForTracing(memory_dump_config);
}

void TracingObserver::OnTraceLogDisabled() {
  if (memory_dump_manager_)
    memory_dump_manager_->TeardownForTracing();
  memory_dump_config_.reset();
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
  if (!memory_dump_config_)
    return false;
  return memory_dump_config_->allowed_dump_modes.count(dump_mode) != 0;
}

}  // namespace memory_instrumentation

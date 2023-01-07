// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_TRACING_OBSERVER_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_TRACING_OBSERVER_H_

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"

namespace memory_instrumentation {

// Observes TraceLog for Enable/Disable events and when they occur Enables and
// Disables the MemoryDumpManager with the correct state based on reading the
// trace log. Also provides a method for adding a dump to the trace.
class COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION)
    TracingObserver : public base::trace_event::TraceLog::EnabledStateObserver {
 public:
  TracingObserver(base::trace_event::TraceLog*,
                  base::trace_event::MemoryDumpManager*);

  TracingObserver(const TracingObserver&) = delete;
  TracingObserver& operator=(const TracingObserver&) = delete;

  ~TracingObserver() override;

  // TraceLog::EnabledStateObserver implementation.
  void OnTraceLogEnabled() override;
  void OnTraceLogDisabled() override;

  virtual bool AddChromeDumpToTraceIfEnabled(
      const base::trace_event::MemoryDumpRequestArgs&,
      const base::ProcessId pid,
      const base::trace_event::ProcessMemoryDump*,
      const base::TimeTicks& timestamp);

  virtual bool AddOsDumpToTraceIfEnabled(
      const base::trace_event::MemoryDumpRequestArgs& args,
      const base::ProcessId pid,
      const mojom::OSMemDump& os_dump,
      const std::vector<mojom::VmRegionPtr>& memory_maps,
      const base::TimeTicks& timestamp);

  // TODO(lalitm): make these private again after TracingObserver is moved
  // to private space.
  bool ShouldAddToTrace(const base::trace_event::MemoryDumpRequestArgs&);

 protected:
  static std::string ApplyPathFiltering(const std::string& file,
                                        bool is_argument_filtering_enabled);

 private:
  // Returns true if the dump mode is allowed for current tracing session.
  bool IsDumpModeAllowed(base::trace_event::MemoryDumpLevelOfDetail) const;

  const raw_ptr<base::trace_event::MemoryDumpManager> memory_dump_manager_;
  const raw_ptr<base::trace_event::TraceLog> trace_log_;
  std::unique_ptr<base::trace_event::TraceConfig::MemoryDumpConfig>
      memory_dump_config_;
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_TRACING_OBSERVER_H_

// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_TRACING_OBSERVER_TRACED_VALUE_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_TRACING_OBSERVER_TRACED_VALUE_H_

#include "base/component_export.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace memory_instrumentation {

// Version of TracingObserver that serialized the dump into a TracedValue
class COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION)
    TracingObserverTracedValue : public TracingObserver {
 public:
  TracingObserverTracedValue(base::trace_event::TraceLog*,
                             base::trace_event::MemoryDumpManager*);

  TracingObserverTracedValue(const TracingObserverTracedValue&) = delete;
  TracingObserverTracedValue& operator=(const TracingObserverTracedValue&) =
      delete;

  ~TracingObserverTracedValue() override;

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
      const base::TimeTicks& timastamp) override;

  static void MemoryMapsAsValueInto(
      const std::vector<mojom::VmRegionPtr>& memory_maps,
      base::trace_event::TracedValue* value,
      bool is_argument_filtering_enabled);

  static void AddToTrace(const base::trace_event::MemoryDumpRequestArgs&,
                         const base::ProcessId,
                         std::unique_ptr<base::trace_event::TracedValue>);
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_TRACING_OBSERVER_TRACED_VALUE_H_

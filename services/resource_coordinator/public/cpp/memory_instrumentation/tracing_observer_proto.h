// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_TRACING_OBSERVER_PROTO_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_TRACING_OBSERVER_PROTO_H_

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/thread_annotations.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/trace_event.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer.h"
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
    TracingObserverProto : public TracingObserver {
 public:
  static TracingObserverProto* GetInstance();
  TracingObserverProto();
  TracingObserverProto(const TracingObserverProto&) = delete;
  TracingObserverProto& operator=(const TracingObserverProto&) = delete;

  ~TracingObserverProto() override;

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

  static void MemoryMapsAsProtoInto(
      const std::vector<mojom::VmRegionPtr>& memory_maps,
      perfetto::protos::pbzero::SmapsPacket* smaps,
      bool is_argument_filtering_enabled);

  void ResetForTesting();

  using OnChromeDumpCallback = base::OnceCallback<void(void)>;

  // Set a callback that will fire after a dump is written into trace, but only
  // once. Useful in tracing tests.
  void SetOnChromeDumpCallbackForTesting(OnChromeDumpCallback);

 private:
  base::Lock on_chrome_dump_callback_lock_;
  OnChromeDumpCallback on_chrome_dump_callback_for_testing_
      GUARDED_BY(on_chrome_dump_callback_lock_);
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_TRACING_OBSERVER_PROTO_H_

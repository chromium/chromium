// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_MEMORY_INSTRUMENTATION_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_MEMORY_INSTRUMENTATION_H_

#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace memory_instrumentation {

// This a public API for the memory-infra service and allows any
// sequence/process to request memory snapshots. This is a convenience wrapper
// around the memory_instrumentation service and hides away the complexity
// associated with having to deal with it (e.g., maintaining service
// connections, bindings, handling timeouts).
class COMPONENT_EXPORT(RESOURCE_COORDINATOR_PUBLIC_MEMORY_INSTRUMENTATION)
    MemoryInstrumentation {
 public:
  using MemoryDumpType = base::trace_event::MemoryDumpType;
  using MemoryDumpLevelOfDetail = base::trace_event::MemoryDumpLevelOfDetail;
  using MemoryDumpDeterminism = base::trace_event::MemoryDumpDeterminism;
  using RequestGlobalDumpCallback =
      base::OnceCallback<void(bool success,
                              std::unique_ptr<GlobalMemoryDump> dump)>;
  using RequestGlobalMemoryDumpAndAppendToTraceCallback =
      base::OnceCallback<void(bool success, uint64_t dump_id)>;

  static void CreateInstance(
      mojo::PendingRemote<memory_instrumentation::mojom::Coordinator>
          coordinator,
      bool is_browser_process);
  static MemoryInstrumentation* GetInstance();

  MemoryInstrumentation(const MemoryInstrumentation&) = delete;
  MemoryInstrumentation& operator=(const MemoryInstrumentation&) = delete;

  // Retrieves a Coordinator interface to communicate with the service. This is
  // safe to call from any thread.
  memory_instrumentation::mojom::Coordinator* GetCoordinator() const {
    return coordinator_.get();
  }

  // Requests a global memory dump with |allocator_dump_names| indicating
  // the name of allocator dumps in which the consumer is interested. If
  // |allocator_dump_names| is empty, no dumps will be returned.
  // Returns asynchronously, via the callback argument:
  //  (true, global_dump) if succeeded;
  //  (false, global_dump) if failed, with global_dump being non-null
  //  but missing data.
  // The callback (if not null), will be posted on the same sequence of the
  // RequestGlobalDump() call.
  // Note: Even if |allocator_dump_names| is empty, all MemoryDumpProviders will
  // still be queried.
  void RequestGlobalDump(const std::vector<std::string>& allocator_dump_names,
                         RequestGlobalDumpCallback);

  // Returns asynchronously, via the callback argument:
  //  (true, global_dump) if succeeded;
  //  (false, global_dump) if failed, with global_dump being non-null
  //  but missing data.
  // The callback (if not null), will be posted on the same sequence of the
  // RequestPrivateMemoryFootprint() call.
  // Passing a null |pid| is the same as requesting the PrivateMemoryFootprint
  // for all processes.
  void RequestPrivateMemoryFootprint(base::ProcessId pid,
                                     RequestGlobalDumpCallback);

  // Requests a global memory dump with |allocator_dump_names| indicating
  // the name of allocator dumps in which the consumer is interested. If
  // |allocator_dump_names| is empty, all allocator dumps will be returned.
  // Returns asynchronously, via the callback argument, the global memory
  // dump with the process memory dump for the given pid:
  //  (true, global_dump) if succeeded;
  //  (false, global_dump) if failed, with global_dump being non-null
  //  but missing data.
  // The callback (if not null), will be posted on the same sequence of the
  // RequestGlobalDump() call.
  void RequestGlobalDumpForPid(
      base::ProcessId pid,
      const std::vector<std::string>& allocator_dump_names,
      RequestGlobalDumpCallback);

  // Requests a global memory dump and serializes the result into the trace.
  // This requires that both tracing and the memory-infra category have been
  // previously enabled. Will just gracefully fail otherwise.
  // Returns asynchronously, via the callback argument:
  //  (true, id of the object injected into the trace) if succeeded;
  //  (false, undefined) if failed.
  // The callback (if not null), will be posted on the same sequence of the
  // RequestGlobalDumpAndAppendToTrace() call.
  void RequestGlobalDumpAndAppendToTrace(
      MemoryDumpType,
      MemoryDumpLevelOfDetail,
      MemoryDumpDeterminism,
      RequestGlobalMemoryDumpAndAppendToTraceCallback);

 private:
  explicit MemoryInstrumentation(
      mojo::PendingRemote<memory_instrumentation::mojom::Coordinator>
          coordinator,
      bool is_browser_process);
  ~MemoryInstrumentation();

  const mojo::SharedRemote<memory_instrumentation::mojom::Coordinator>
      coordinator_;

  // Only browser process is allowed to request memory dumps.
  const bool is_browser_process_;
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_MEMORY_INSTRUMENTATION_MEMORY_INSTRUMENTATION_H_

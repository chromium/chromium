// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_QUEUED_REQUEST_DISPATCHER_H_
#define SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_QUEUED_REQUEST_DISPATCHER_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "services/resource_coordinator/memory_instrumentation/coordinator_impl.h"
#include "services/resource_coordinator/memory_instrumentation/queued_request.h"
#include "third_party/perfetto/include/perfetto/ext/trace_processor/importers/memory_tracker/graph.h"

namespace memory_instrumentation {

class QueuedRequestDispatcher {
 public:
  using OSMemDumpMap =
      base::flat_map<base::ProcessId,
                     memory_instrumentation::mojom::RawOSMemDumpPtr>;
  using RequestGlobalMemoryDumpInternalCallback = base::OnceCallback<
      void(bool, uint64_t, memory_instrumentation::mojom::GlobalMemoryDumpPtr)>;
  using ChromeCallback = base::RepeatingCallback<void(
      base::ProcessId,
      bool,
      uint64_t,
      std::unique_ptr<base::trace_event::ProcessMemoryDump>)>;
  using OsCallback =
      base::RepeatingCallback<void(base::ProcessId, bool, OSMemDumpMap)>;
  using VmRegions =
      base::flat_map<base::ProcessId,
                     std::vector<memory_instrumentation::mojom::VmRegionPtr>>;

  struct ClientInfo {
    ClientInfo(mojom::ClientProcess* client,
               base::ProcessId pid,
               mojom::ProcessType process_type,
               std::optional<std::string> service_name);
    ClientInfo(ClientInfo&& other);
    ~ClientInfo();

    const raw_ptr<mojom::ClientProcess> client;
    const base::ProcessId pid;
    const mojom::ProcessType process_type;
    const std::optional<std::string> service_name;
  };

  // Sets up the parameters of the queued |request| using |clients| and then
  // dispatches the request for a memory dump to each client process.
  static void SetUpAndDispatch(QueuedRequest* request,
                               const std::vector<ClientInfo>& clients,
                               const ChromeCallback& chrome_callback,
                               const OsCallback& os_callback);

  // Finalizes the queued |request| by collating all responses recieved and
  // dispatching to the appropriate callback. Also adds to tracing using
  // |tracing_observer| if the |request| requires it.
  static void Finalize(QueuedRequest* request,
                       TracingObserver* tracing_observer);

  static void SetUpAndDispatchVmRegionRequest(
      QueuedVmRegionRequest* request,
      const std::vector<ClientInfo>& clients,
      const std::vector<base::ProcessId>& desired_pids,
      const OsCallback& os_callback);
  static VmRegions FinalizeVmRegionRequest(QueuedVmRegionRequest* request);

 private:
  static bool AddChromeMemoryDumpToTrace(
      const base::trace_event::MemoryDumpRequestArgs& args,
      base::ProcessId pid,
      const base::trace_event::ProcessMemoryDump& raw_chrome_dump,
      TracingObserver* tracing_observer,
      const base::TimeTicks& timestamp);
};

}  // namespace memory_instrumentation

#endif  // SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_QUEUED_REQUEST_DISPATCHER_H_

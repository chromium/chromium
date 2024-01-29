// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_COORDINATOR_IMPL_H_
#define SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_COORDINATOR_IMPL_H_

#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/resource_coordinator/memory_instrumentation/queued_request.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/registry.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/tracing_observer.h"
#include "services/resource_coordinator/public/mojom/memory_instrumentation/memory_instrumentation.mojom.h"

namespace memory_instrumentation {

// Memory instrumentation service. It serves two purposes:
// - Handles a registry of the processes that have a memory instrumentation
//   client library instance (../../public/cpp/memory).
// - Provides global (i.e. for all processes) memory snapshots on demand.
//   Global snapshots are obtained by requesting in-process snapshots from each
//   registered client and aggregating them.
class CoordinatorImpl : public Registry,
                        public mojom::Coordinator,
                        public mojom::HeapProfilerHelper {
 public:
  CoordinatorImpl();

  CoordinatorImpl(const CoordinatorImpl&) = delete;
  CoordinatorImpl& operator=(const CoordinatorImpl&) = delete;

  ~CoordinatorImpl() override;

  // The getter of the unique instance.
  static CoordinatorImpl* GetInstance();

  // Registry:
  void RegisterHeapProfiler(mojo::PendingRemote<mojom::HeapProfiler> profiler,
                            mojo::PendingReceiver<mojom::HeapProfilerHelper>
                                helper_receiver) override;
  void RegisterClientProcess(
      mojo::PendingReceiver<mojom::Coordinator> receiver,
      mojo::PendingRemote<mojom::ClientProcess> client_process,
      mojom::ProcessType process_type,
      base::ProcessId process_id,
      const std::optional<std::string>& service_name) override;

  // mojom::Coordinator implementation.
  void RequestGlobalMemoryDump(
      base::trace_event::MemoryDumpType,
      base::trace_event::MemoryDumpLevelOfDetail,
      base::trace_event::MemoryDumpDeterminism,
      const std::vector<std::string>& allocator_dump_names,
      RequestGlobalMemoryDumpCallback) override;
  void RequestGlobalMemoryDumpForPid(
      base::ProcessId,
      const std::vector<std::string>& allocator_dump_names,
      RequestGlobalMemoryDumpForPidCallback) override;
  void RequestPrivateMemoryFootprint(
      base::ProcessId,
      RequestPrivateMemoryFootprintCallback) override;
  void RequestGlobalMemoryDumpAndAppendToTrace(
      base::trace_event::MemoryDumpType,
      base::trace_event::MemoryDumpLevelOfDetail,
      base::trace_event::MemoryDumpDeterminism,
      RequestGlobalMemoryDumpAndAppendToTraceCallback) override;

  // mojom::HeapProfilerHelper implementation.
  void GetVmRegionsForHeapProfiler(
      const std::vector<base::ProcessId>& pids,
      GetVmRegionsForHeapProfilerCallback) override;


 private:
  using OSMemDumpMap = base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr>;
  using RequestGlobalMemoryDumpInternalCallback =
      base::OnceCallback<void(bool, uint64_t, mojom::GlobalMemoryDumpPtr)>;
  friend class CoordinatorImplTest;  // For testing
  FRIEND_TEST_ALL_PREFIXES(CoordinatorImplTest,
                           DumpsAreAddedToTraceWhenRequested);
  FRIEND_TEST_ALL_PREFIXES(CoordinatorImplTest,
                           DumpsArentAddedToTraceUnlessRequested);

  // Holds metadata and a client pipe connected to every client process.
  struct ClientInfo {
    ClientInfo(mojo::Remote<mojom::ClientProcess> client,
               mojom::ProcessType,
               std::optional<std::string> service_name);
    ~ClientInfo();

    const mojo::Remote<mojom::ClientProcess> client;
    const mojom::ProcessType process_type;
    const std::optional<std::string> service_name;
  };

  void UnregisterClientProcess(base::ProcessId);

  void RequestGlobalMemoryDumpInternal(
      const QueuedRequest::Args& args,
      RequestGlobalMemoryDumpInternalCallback callback);

  // Callback of RequestChromeMemoryDump.
  void OnChromeMemoryDumpResponse(
      base::ProcessId process_id,
      bool success,
      uint64_t dump_guid,
      std::unique_ptr<base::trace_event::ProcessMemoryDump> chrome_memory_dump);

  // Callback of RequestOSMemoryDump.
  void OnOSMemoryDumpResponse(uint64_t dump_guid,
                              base::ProcessId process_id,
                              bool success,
                              OSMemDumpMap);

  // Callback of RequestOSMemoryDumpForVmRegions.
  void OnOSMemoryDumpForVMRegions(uint64_t dump_guid,
                                  base::ProcessId process_id,
                                  bool success,
                                  OSMemDumpMap);

  void FinalizeVmRegionDumpIfAllManagersReplied(uint64_t dump_guid);

  // Callback of DumpProcessesForTracing.
  void OnDumpProcessesForTracing(
      uint64_t dump_guid,
      std::vector<mojom::HeapProfileResultPtr> heap_profile_results);

  void RemovePendingResponse(base::ProcessId process_id,
                             QueuedRequest::PendingResponse::Type);

  void OnQueuedRequestTimedOut(uint64_t dump_guid);
  void OnHeapDumpTimeOut(uint64_t dump_guid);

  void PerformNextQueuedGlobalMemoryDump();
  void FinalizeGlobalMemoryDumpIfAllManagersReplied();
  QueuedRequest* GetCurrentRequest();

  void set_client_process_timeout(base::TimeDelta client_process_timeout) {
    client_process_timeout_ = client_process_timeout;
  }

  // Map of registered client processes.
  std::map<base::ProcessId, std::unique_ptr<ClientInfo>> clients_;

  // Outstanding dump requests, enqueued via RequestGlobalMemoryDump().
  std::list<QueuedRequest> queued_memory_dump_requests_;

  // Outstanding vm region requests, enqueued via GetVmRegionsForHeapProfiler().
  // This is kept in a separate list from |queued_memory_dump_requests_| for two
  // reasons:
  //   1) The profiling service is queried during a memory dump request, but the
  //   profiling service in turn needs to query for vm regions. Keeping this in
  //   the same list as |queued_memory_dump_requests_| would require this class
  //   to support concurrent requests.
  //
  //   2) Vm region requests are only ever requested by the profiling service,
  //   which uses the HeapProfilerHelper interface. Keeping the requests
  //   separate means we can avoid littering the RequestGlobalMemoryDump
  //   interface with flags intended for HeapProfilerHelper. This was already
  //   technically possible before, but required some additional plumbing. Now
  //   the separation is much cleaner.
  //
  // Unlike queued_memory_dump_requests_, all requests are executed in parallel.
  // The key is a |dump_guid|.
  std::map<uint64_t, std::unique_ptr<QueuedVmRegionRequest>>
      in_progress_vm_region_requests_;

  // There may be extant callbacks in |queued_memory_dump_requests_|. These
  // receivers must be closed before destroying the un-run callbacks.
  mojo::ReceiverSet<mojom::Coordinator, base::ProcessId> coordinator_receivers_;

  // Dump IDs are unique across both heap dump and memory dump requests.
  uint64_t next_dump_id_;

  // Timeout for registered client processes to respond to dump requests.
  base::TimeDelta client_process_timeout_;

  // When not null, can be queried for heap dumps.
  mojo::Remote<mojom::HeapProfiler> heap_profiler_;
  mojo::Receiver<mojom::HeapProfilerHelper> heap_profiler_helper_receiver_{
      this};

  const bool write_proto_heap_profile_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<CoordinatorImpl> weak_ptr_factory_{this};
};

}  // namespace memory_instrumentation
#endif  // SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_COORDINATOR_IMPL_H_

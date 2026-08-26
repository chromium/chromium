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
class CoordinatorImpl : public Registry, public mojom::Coordinator {
 public:
  CoordinatorImpl();

  CoordinatorImpl(const CoordinatorImpl&) = delete;
  CoordinatorImpl& operator=(const CoordinatorImpl&) = delete;

  ~CoordinatorImpl() override;

  // The getter of the unique instance.
  static CoordinatorImpl* GetInstance();

  // Registry:
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

 private:
  using OSMemDumpMap = base::flat_map<base::ProcessId, mojom::RawOSMemDumpPtr>;
  using RequestGlobalMemoryDumpInternalCallback = base::OnceCallback<
      void(mojom::RequestOutcome, uint64_t, mojom::GlobalMemoryDumpPtr)>;
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
      mojom::RequestOutcome outcome,
      uint64_t dump_guid,
      std::unique_ptr<base::trace_event::ProcessMemoryDump> chrome_memory_dump);

  // Callback of RequestOSMemoryDump.
  void OnOSMemoryDumpResponse(uint64_t dump_guid,
                              base::ProcessId process_id,
                              mojom::RequestOutcome outcome,
                              OSMemDumpMap);

  void RemovePendingResponse(base::ProcessId process_id,
                             QueuedRequest::PendingResponse::Type);

  void OnQueuedRequestTimedOut(uint64_t dump_guid);

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

  // There may be extant callbacks in |queued_memory_dump_requests_|. These
  // receivers must be closed before destroying the un-run callbacks.
  mojo::ReceiverSet<mojom::Coordinator, base::ProcessId> coordinator_receivers_;

  // Dump IDs are unique across memory dump requests.
  uint64_t next_dump_id_;

  // Timeout for registered client processes to respond to dump requests.
  base::TimeDelta client_process_timeout_;

  THREAD_CHECKER(thread_checker_);
  base::WeakPtrFactory<CoordinatorImpl> weak_ptr_factory_{this};
};

}  // namespace memory_instrumentation
#endif  // SERVICES_RESOURCE_COORDINATOR_MEMORY_INSTRUMENTATION_COORDINATOR_IMPL_H_

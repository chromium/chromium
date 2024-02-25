// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_PERFETTO_SERVICE_H_
#define SERVICES_TRACING_PERFETTO_PERFETTO_SERVICE_H_

#include <map>
#include <memory>
#include <set>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/tracing/perfetto_task_runner.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/tracing/perfetto/consumer_host.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"

namespace perfetto {
class TracingService;
}  // namespace perfetto

namespace tracing {

// This class serves two purposes: It wraps the use of the system-wide
// perfetto::TracingService instance, and serves as the main Mojo interface for
// connecting per-process ProducerClient with corresponding service-side
// ProducerHost.
class PerfettoService : public mojom::PerfettoService {
 public:
  explicit PerfettoService(scoped_refptr<base::SequencedTaskRunner>
                               task_runner_for_testing = nullptr);

  PerfettoService(const PerfettoService&) = delete;
  PerfettoService& operator=(const PerfettoService&) = delete;

  ~PerfettoService() override;

  static PerfettoService* GetInstance();
  static bool ParsePidFromProducerName(const std::string& producer_name,
                                       base::ProcessId* pid);

  void BindReceiver(mojo::PendingReceiver<mojom::PerfettoService> receiver,
                    uint32_t pid);

  // mojom::PerfettoService implementation.
  void ConnectToProducerHost(
      mojo::PendingRemote<mojom::ProducerClient> producer_client,
      mojo::PendingReceiver<mojom::ProducerHost> producer_host_receiver,
      base::UnsafeSharedMemoryRegion shared_memory,
      uint64_t shared_memory_buffer_page_size_bytes) override;

  perfetto::TracingService* GetService() const;

  // Called when a ConsumerHost::TracingSession is created/destroyed (i.e. when
  // a consumer starts/finishes tracing.
  void RegisterTracingSession(ConsumerHost::TracingSession* consumer_host);
  void UnregisterTracingSession(ConsumerHost::TracingSession* consumer_host);

  // Make a request of the service for whether or not a TracingSession
  // should be allowed to start tracing, in case of pre-existing sessions.
  // |callback| will eventually be called once a session is allowed, or it
  // will be destroyed.
  void RequestTracingSession(mojom::TracingClientPriority priority,
                             base::OnceClosure callback);

  // Called by TracingService to notify the perfetto service of the PIDs of
  // actively running services (whenever a service starts or stops).
  void AddActiveServicePid(base::ProcessId pid);
  void RemoveActiveServicePid(base::ProcessId pid);
  void RemoveActiveServicePidIfNoActiveConnections(base::ProcessId pid);
  void SetActiveServicePidsInitialized();

  std::set<base::ProcessId> active_service_pids() const {
    base::AutoLock lock(active_service_pids_lock_);
    return active_service_pids_;
  }

  bool active_service_pids_initialized() const {
    return active_service_pids_initialized_;
  }

  base::tracing::PerfettoTaskRunner* perfetto_task_runner() {
    return &perfetto_task_runner_;
  }

 private:
  void BindOnSequence(mojo::PendingReceiver<mojom::PerfettoService> receiver);
  void CreateServiceOnSequence();
  void OnProducerHostDisconnect();
  void OnServiceDisconnect();
  void OnDisconnectFromProcess(base::ProcessId pid);

  base::tracing::PerfettoTaskRunner perfetto_task_runner_;
  std::unique_ptr<perfetto::TracingService> service_;
  mojo::ReceiverSet<mojom::PerfettoService, uint32_t> receivers_;
  mojo::UniqueReceiverSet<mojom::ProducerHost, uint32_t> producer_receivers_;
  std::set<raw_ptr<ConsumerHost::TracingSession, SetExperimental>>
      tracing_sessions_;  // Not owned.
  // Protects access to |active_service_pids_|. We need this lock because
  // CustomEventRecorder calls active_service_pids() from a possibly different
  // thread on incremental state reset.
  mutable base::Lock active_service_pids_lock_;
  std::set<base::ProcessId> active_service_pids_
      GUARDED_BY(active_service_pids_lock_);
  std::map<base::ProcessId, int> num_active_connections_;
  bool active_service_pids_initialized_ = false;
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_PERFETTO_SERVICE_H_

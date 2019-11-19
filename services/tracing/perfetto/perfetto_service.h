// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PERFETTO_PERFETTO_SERVICE_H_
#define SERVICES_TRACING_PERFETTO_PERFETTO_SERVICE_H_

#include <memory>
#include <set>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/strong_binding_set.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/tracing/perfetto/consumer_host.h"
#include "services/tracing/public/cpp/perfetto/task_runner.h"
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
  ~PerfettoService() override;

  static PerfettoService* GetInstance();
  static bool ParsePidFromProducerName(const std::string& producer_name,
                                       base::ProcessId* pid);

  void BindReceiver(mojo::PendingReceiver<mojom::PerfettoService> receiver,
                    uint32_t pid);

  // mojom::PerfettoService implementation.
  void ConnectToProducerHost(
      mojo::PendingRemote<mojom::ProducerClient> producer_client,
      mojo::PendingReceiver<mojom::ProducerHost> producer_host_receiver)
      override;

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
  void SetActiveServicePidsInitialized();

  std::set<base::ProcessId> active_service_pids() const {
    return active_service_pids_;
  }

  bool active_service_pids_initialized() const {
    return active_service_pids_initialized_;
  }

 private:
  void BindOnSequence(mojo::PendingReceiver<mojom::PerfettoService> receiver);
  void CreateServiceOnSequence();

  PerfettoTaskRunner perfetto_task_runner_;
  std::unique_ptr<perfetto::TracingService> service_;
  mojo::ReceiverSet<mojom::PerfettoService, uint32_t> receivers_;
  mojo::UniqueReceiverSet<mojom::ProducerHost> producer_receivers_;
  std::set<ConsumerHost::TracingSession*> tracing_sessions_;  // Not owned.
  std::set<base::ProcessId> active_service_pids_;
  bool active_service_pids_initialized_ = false;

  DISALLOW_COPY_AND_ASSIGN(PerfettoService);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PERFETTO_PERFETTO_SERVICE_H_

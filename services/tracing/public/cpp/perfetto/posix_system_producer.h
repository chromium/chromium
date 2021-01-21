// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_POSIX_SYSTEM_PRODUCER_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_POSIX_SYSTEM_PRODUCER_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/atomicops.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "services/tracing/public/cpp/perfetto/system_producer.h"
#include "services/tracing/public/cpp/perfetto/task_runner.h"

namespace perfetto {
class SharedMemoryArbiter;
class SharedMemory;
}  // namespace perfetto

namespace tracing {

class COMPONENT_EXPORT(TRACING_CPP) PosixSystemProducer
    : public SystemProducer {
 public:
  enum class State {
    kDisconnected = 0,
    kConnecting = 1,
    // Connected but all data sources unregistered.
    kUnregistered = 2,
    kConnected = 3,
  };
  PosixSystemProducer(const char* socket, PerfettoTaskRunner* task_runner);
  ~PosixSystemProducer() override;

  // Functions needed for PosixSystemProducer only.
  //
  // Lets tests ignore the SDK check (Perfetto only runs on post Android Pie
  // devices by default, so for trybots on older OSs we need to ignore the check
  // for our test system service).
  //
  // TODO(nuskos): We need to make this possible for telemetry as well, since
  // they might have side loaded the app.
  void SetDisallowPreAndroidPieForTesting(bool disallow);
  // |socket| must remain alive as long as PosixSystemProducer is around
  // trying to connect to it.
  void SetNewSocketForTesting(const char* socket);

  // PerfettoProducer implementation.
  perfetto::SharedMemoryArbiter* MaybeSharedMemoryArbiter() override;
  void NewDataSourceAdded(
      const PerfettoTracedProcess::DataSourceBase* const data_source) override;
  bool IsTracingActive() override;

  // SystemProducer implementation.
  void ConnectToSystemService() override;
  void ActivateTriggers(const std::vector<std::string>& triggers) override;
  // When Chrome's tracing service wants to trace it always takes priority over
  // the system Perfetto service. To cleanly shut down and let the system
  // Perfetto know we are no longer participating we unregister the data
  // sources. Once all the data sources are stopped |on_disconnect_complete| is
  // called. Afterwards we will periodically check to see if the local trace has
  // finished and then re-register everything to the system Perfetto service.
  // Which might mean we rejoin the same trace if it is still ongoing or future
  // traces.
  void DisconnectWithReply(
      base::OnceClosure on_disconnect_complete = base::OnceClosure()) override;

  // perfetto::Producer implementation.
  // Used by the service to start and stop traces.
  void OnConnect() override;
  void OnDisconnect() override;
  void OnTracingSetup() override;
  void SetupDataSource(perfetto::DataSourceInstanceID,
                       const perfetto::DataSourceConfig&) override;
  void StartDataSource(perfetto::DataSourceInstanceID,
                       const perfetto::DataSourceConfig&) override;
  void StopDataSource(perfetto::DataSourceInstanceID) override;
  void Flush(perfetto::FlushRequestID,
             const perfetto::DataSourceInstanceID* data_source_ids,
             size_t num_data_sources) override;
  void ClearIncrementalState(
      const perfetto::DataSourceInstanceID* data_source_ids,
      size_t num_data_sources) override;

 protected:
  // PerfettoProducer implementation.
  bool SetupSharedMemoryForStartupTracing() override;

  // Given our current |state_| determine how to properly connect and set up our
  // connection to the service via the named fd socket provided in the
  // constructor. If we succeed OnConnect() will be called, if we fail
  // OnDisconnect() will be called.
  void Connect();

 private:
  // This sets |service_| by connecting over Perfetto's IPC connection.
  void ConnectSocket();
  // Returns true if we should skip setup because this Android device is Android
  // O or below.
  bool SkipIfOnAndroidAndPreAndroidPie() const;
  // If any OnDisconnect callbacks are stored, this will invoke them and delete
  // references to them must be called on the proper sequence.
  void InvokeStoredOnDisconnectCallbacks();
  // After a certain amount of backoff time we will attempt to Connect() or if
  // Chrome is already tracing we will wait awhile and attempt to Connect()
  // later.
  void DelayedReconnect();
  // Called after a data source has completed a flush.
  void NotifyDataSourceFlushComplete(perfetto::FlushRequestID id);

  perfetto::TracingService::ProducerEndpoint* GetService();

  bool retrying_ = false;
  std::string socket_name_;
  uint32_t connection_backoff_ms_;
  bool disallow_pre_android_pie_ = true;
  State state_ = State::kDisconnected;
  std::vector<base::OnceClosure> on_disconnect_callbacks_;
  // First value is the flush ID, the second is the number of
  // replies we're still waiting for.
  std::pair<uint64_t, size_t> pending_replies_for_latest_flush_;

  // -- Begin lock-protected members. --
  base::Lock lock_;

  // ProducerEndpoints must outlive all trace writers, but some trace writers
  // will never flush until future tracing sessions, which means even on
  // disconnecting we have to keep these around. So instead of destroying any
  // Endpoints (which hold the SharedMemory and SharedMemoryArbiters) we store
  // them forever (leaking their amount of memory).
  //
  // |services_| is accessed by MaybeSharedMemoryArbiter() on any thread. This
  // access and any modifications to |services_| are protected by the |lock_|.
  //
  // TODO(nuskos): We should improve this once we're on the client library.
  std::vector<std::unique_ptr<perfetto::TracingService::ProducerEndpoint>>
      services_;

  uint64_t data_sources_tracing_ = 0;
  // -- End lock-protected members. --

  // NOTE: Weak pointers must be invalidated before all other member variables.
  // and thus must be the last member variable.
  base::WeakPtrFactory<PosixSystemProducer> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(PosixSystemProducer);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_POSIX_SYSTEM_PRODUCER_H_

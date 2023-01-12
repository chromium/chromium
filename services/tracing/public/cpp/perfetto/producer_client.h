// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_CLIENT_H_
#define SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_CLIENT_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/atomicops.h"
#include "base/component_export.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/tracing/perfetto_task_runner.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/tracing_service.h"

namespace perfetto {
class SharedMemoryArbiter;
}  // namespace perfetto

namespace tracing {

class ChromeBaseSharedMemory;

// This class is the per-process client side of the Perfetto
// producer, and is responsible for creating specific kinds
// of DataSources (like ChromeTracing) on demand, and provide
// them with TraceWriters and a configuration to start logging.
class COMPONENT_EXPORT(TRACING_CPP) ProducerClient
    : public PerfettoProducer,
      public perfetto::TracingService::ProducerEndpoint,
      public mojom::ProducerClient {
 public:
  explicit ProducerClient(base::tracing::PerfettoTaskRunner*);

  ProducerClient(const ProducerClient&) = delete;
  ProducerClient& operator=(const ProducerClient&) = delete;

  ~ProducerClient() override;

  void Connect(mojo::PendingRemote<mojom::PerfettoService> perfetto_service);
  void BindInProcessSharedMemoryArbiter(
      perfetto::TracingService::ProducerEndpoint*,
      base::tracing::PerfettoTaskRunner*);

  void Disconnect() override;

  // PerfettoProducer implementation.
  void BindStartupTargetBuffer(
      uint16_t target_buffer_reservation_id,
      perfetto::BufferID startup_target_buffer) override;
  void AbortStartupTracingForReservation(
      uint16_t target_buffer_reservation_id) override;
  perfetto::SharedMemoryArbiter* MaybeSharedMemoryArbiter() override;
  void NewDataSourceAdded(
      const PerfettoTracedProcess::DataSourceBase* const data_source) override;
  bool IsTracingActive() override;

  // mojom::ProducerClient implementation.
  // Called through Mojo by the ProducerHost on the service-side to control
  // tracing and toggle specific DataSources.
  void OnTracingStart() override;
  void StartDataSource(uint64_t id,
                       const perfetto::DataSourceConfig& data_source_config,
                       StartDataSourceCallback callback) override;

  void StopDataSource(uint64_t id, StopDataSourceCallback callback) override;
  void Flush(uint64_t flush_request_id,
             const std::vector<uint64_t>& data_source_ids) override;
  void ClearIncrementalState() override;

  // perfetto::TracingService::ProducerEndpoint implementation.

  // Called by DataSources to create trace writers. The returned trace writers
  // are hooked up to our SharedMemoryArbiter directly.
  std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(
      perfetto::BufferID target_buffer,
      perfetto::BufferExhaustedPolicy =
          perfetto::BufferExhaustedPolicy::kDefault) override;

  // Used by SharedMemoryArbiterImpl to register/unregister TraceWriters and
  // send commit requests, which signal that shared memory chunks are ready
  // for consumption.
  void CommitData(const perfetto::CommitDataRequest& commit,
                  CommitDataCallback callback) override;
  void RegisterTraceWriter(uint32_t writer_id, uint32_t target_buffer) override;
  void UnregisterTraceWriter(uint32_t writer_id) override;

  // These ProducerEndpoint functions are only used on the service
  // side and should not be called on the clients.
  perfetto::SharedMemory* shared_memory() const override;
  void NotifyFlushComplete(perfetto::FlushRequestID) override;
  void RegisterDataSource(const perfetto::DataSourceDescriptor&) override;
  void UpdateDataSource(const perfetto::DataSourceDescriptor&) override;
  void UnregisterDataSource(const std::string& name) override;
  void NotifyDataSourceStopped(perfetto::DataSourceInstanceID) override;
  void NotifyDataSourceStarted(perfetto::DataSourceInstanceID) override;
  void ActivateTriggers(const std::vector<std::string>&) override;
  size_t shared_buffer_page_size_kb() const override;
  bool IsShmemProvidedByProducer() const override;
  void Sync(std::function<void()> callback) override;

  void BindClientAndHostPipesForTesting(
      mojo::PendingReceiver<mojom::ProducerClient>,
      mojo::PendingRemote<mojom::ProducerHost>);
  perfetto::SharedMemory* shared_memory_for_testing();

 protected:
  // Protected for testing. Returns false if SMB creation failed.
  bool InitSharedMemoryIfNeeded();

  // PerfettoProducer implementation.
  bool SetupSharedMemoryForStartupTracing() override;

 private:
  friend class base::NoDestructor<ProducerClient>;

  void BindClientAndHostPipesOnSequence(
      mojo::PendingReceiver<mojom::ProducerClient>,
      mojo::PendingRemote<mojom::ProducerHost>);

  // Called after a data source has completed a flush.
  void NotifyDataSourceFlushComplete(perfetto::FlushRequestID id);

  uint32_t data_sources_tracing_ = 0;
  std::unique_ptr<mojo::Receiver<mojom::ProducerClient>> receiver_;
  mojo::Remote<mojom::ProducerHost> producer_host_;
  raw_ptr<base::tracing::PerfettoTaskRunner> in_process_arbiter_task_runner_ =
      nullptr;
  // First value is the flush ID, the second is the number of
  // replies we're still waiting for.
  std::pair<uint64_t, size_t> pending_replies_for_latest_flush_;

  // Guards initialization of SMB and startup tracing.
  base::Lock lock_;
  // TODO(eseckler): Consider accessing |shared_memory_| and
  // |shared_memory_arbiter_| without locks after setup was completed, since we
  // never destroy or unset them.
  std::unique_ptr<ChromeBaseSharedMemory> shared_memory_ GUARDED_BY(lock_);
  std::unique_ptr<perfetto::SharedMemoryArbiter> shared_memory_arbiter_
      GUARDED_BY(lock_);

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<ProducerClient> weak_ptr_factory_{this};
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_CLIENT_H_

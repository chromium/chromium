// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"
#include "services/tracing/public/cpp/perfetto/task_runner.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"

namespace perfetto {
class SharedMemoryArbiter;
}  // namespace perfetto

namespace tracing {

class MojoSharedMemory;

// This class is the per-process client side of the Perfetto
// producer, and is responsible for creating specific kinds
// of DataSources (like ChromeTracing) on demand, and provide
// them with TraceWriters and a configuration to start logging.
class COMPONENT_EXPORT(TRACING_CPP) ProducerClient
    : public PerfettoProducer,
      public mojom::ProducerClient {
 public:
  ProducerClient(PerfettoTaskRunner* task_runner);
  ~ProducerClient() override;

  void NewDataSourceAdded(
      const PerfettoTracedProcess::DataSourceBase* const data_source) override;

  bool IsTracingActive() override;

  void Connect(mojo::PendingRemote<mojom::PerfettoService> perfetto_service);

  void set_in_process_shmem_arbiter(perfetto::SharedMemoryArbiter* arbiter) {
    DCHECK(!in_process_arbiter_);
    in_process_arbiter_ = arbiter;
  }

  // mojom::ProducerClient implementation.
  // Called through Mojo by the ProducerHost on the service-side to control
  // tracing and toggle specific DataSources.
  void OnTracingStart(mojo::ScopedSharedBufferHandle shared_memory,
                      uint64_t shared_memory_buffer_page_size_bytes) override;
  void StartDataSource(uint64_t id,
                       const perfetto::DataSourceConfig& data_source_config,
                       StartDataSourceCallback callback) override;

  void StopDataSource(uint64_t id, StopDataSourceCallback callback) override;
  void Flush(uint64_t flush_request_id,
             const std::vector<uint64_t>& data_source_ids) override;
  void ClearIncrementalState() override;

  // perfetto::TracingService::ProducerEndpoint implementation.
  // Used by the TraceWriters
  // to signal Perfetto that shared memory chunks are ready
  // for consumption.
  void CommitData(const perfetto::CommitDataRequest& commit,
                  CommitDataCallback callback) override;
  // Used by the DataSource implementations to create TraceWriters
  // for writing their protobufs, and respond to flushes.
  void NotifyFlushComplete(perfetto::FlushRequestID) override;
  perfetto::SharedMemory* shared_memory() const override;
  void RegisterTraceWriter(uint32_t writer_id, uint32_t target_buffer) override;
  void UnregisterTraceWriter(uint32_t writer_id) override;

  // These ProducerEndpoint functions are only used on the service
  // side and should not be called on the clients.
  void RegisterDataSource(const perfetto::DataSourceDescriptor&) override;
  void UnregisterDataSource(const std::string& name) override;
  void NotifyDataSourceStopped(perfetto::DataSourceInstanceID) override;
  void NotifyDataSourceStarted(perfetto::DataSourceInstanceID) override;
  void ActivateTriggers(const std::vector<std::string>&) override;
  size_t shared_buffer_page_size_kb() const override;
  perfetto::SharedMemoryArbiter* GetInProcessShmemArbiter() override;

  void BindClientAndHostPipesForTesting(
      mojo::PendingReceiver<mojom::ProducerClient>,
      mojo::PendingRemote<mojom::ProducerHost>);
  void ResetSequenceForTesting();

 protected:
  perfetto::SharedMemoryArbiter* GetSharedMemoryArbiter() override;

 private:
  friend class base::NoDestructor<ProducerClient>;

  void BindClientAndHostPipesOnSequence(
      mojo::PendingReceiver<mojom::ProducerClient>,
      mojo::PendingRemote<mojom::ProducerHost>);

  uint32_t data_sources_tracing_ = 0;
  std::unique_ptr<mojo::Receiver<mojom::ProducerClient>> receiver_;
  mojo::Remote<mojom::ProducerHost> producer_host_;
  std::unique_ptr<MojoSharedMemory> shared_memory_;
  std::unique_ptr<perfetto::SharedMemoryArbiter> shared_memory_arbiter_;
  perfetto::SharedMemoryArbiter* in_process_arbiter_ = nullptr;
  // First value is the flush ID, the second is the number of
  // replies we're still waiting for.
  std::pair<uint64_t, size_t> pending_replies_for_latest_flush_;

  SEQUENCE_CHECKER(sequence_checker_);

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<ProducerClient> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ProducerClient);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_CLIENT_H_

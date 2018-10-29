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
#include "mojo/public/cpp/bindings/binding.h"
#include "services/tracing/public/cpp/perfetto/task_runner.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/core/tracing_service.h"

namespace perfetto {
class SharedMemoryArbiter;
}  // namespace perfetto

namespace tracing {

class MojoSharedMemory;

// This class is the per-process client side of the Perfetto
// producer, and is responsible for creating specific kinds
// of DataSources (like ChromeTracing) on demand, and provide
// them with TraceWriters and a configuration to start logging.

// Implementations of new DataSources should:
// * Implement ProducerClient::DataSourceBase.
// * Add a new data source name in perfetto_service.mojom.
// * Register the data source with Perfetto in ProducerHost::OnConnect.
// * Construct the new implementation when requested to
//   in ProducerClient::StartDataSource.
class COMPONENT_EXPORT(TRACING_CPP) ProducerClient
    : public mojom::ProducerClient,
      public perfetto::TracingService::ProducerEndpoint {
 public:
  class DataSourceBase {
   public:
    explicit DataSourceBase(const std::string& name);
    virtual ~DataSourceBase();

    void StartTracingWithID(uint64_t data_source_id,
                            ProducerClient* producer_client,
                            const mojom::DataSourceConfig& data_source_config);

    virtual void StartTracing(
        ProducerClient* producer_client,
        const mojom::DataSourceConfig& data_source_config) = 0;
    virtual void StopTracing(
        base::OnceClosure stop_complete_callback = base::OnceClosure()) = 0;

    // Flush the data source.
    virtual void Flush(base::RepeatingClosure flush_complete_callback) = 0;

    const std::string& name() const { return name_; }
    uint64_t data_source_id() const { return data_source_id_; }

   private:
    uint64_t data_source_id_ = 0;
    std::string name_;
  };

  ProducerClient();
  ~ProducerClient() override;

  static void DeleteSoonForTesting(std::unique_ptr<ProducerClient>);

  // Returns the taskrunner used by Perfetto.
  static base::SequencedTaskRunner* GetTaskRunner();

  // Create the messagepipes that'll be used to connect
  // to the service-side ProducerHost, on the correct
  // sequence. The callback will be called on same sequence
  // as CreateMojoMessagepipes() got called on.
  using MessagepipesReadyCallback =
      base::OnceCallback<void(mojom::ProducerClientPtr,
                              mojom::ProducerHostRequest)>;
  void CreateMojoMessagepipes(MessagepipesReadyCallback);

  // Add a new data source to the ProducerClient; the caller
  // retains ownership and is responsible for making sure
  // the data source outlives the ProducerClient.
  void AddDataSource(DataSourceBase*);

  // mojom::ProducerClient implementation.
  // Called through Mojo by the ProducerHost on the service-side to control
  // tracing and toggle specific DataSources.
  void OnTracingStart(mojo::ScopedSharedBufferHandle shared_memory) override;
  void StartDataSource(uint64_t id,
                       mojom::DataSourceConfigPtr data_source_config) override;

  void StopDataSource(uint64_t id, StopDataSourceCallback callback) override;
  void Flush(uint64_t flush_request_id,
             const std::vector<uint64_t>& data_source_ids) override;

  // perfetto::TracingService::ProducerEndpoint implementation.
  // Used by the TraceWriters
  // to signal Perfetto that shared memory chunks are ready
  // for consumption.
  void CommitData(const perfetto::CommitDataRequest& commit,
                  CommitDataCallback callback) override;
  // Used by the DataSource implementations to create TraceWriters
  // for writing their protobufs, and respond to flushes.
  std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(
      perfetto::BufferID target_buffer) override;
  void NotifyFlushComplete(perfetto::FlushRequestID) override;
  perfetto::SharedMemory* shared_memory() const override;

  // These ProducerEndpoint functions are only used on the service
  // side and should not be called on the clients.
  void RegisterDataSource(const perfetto::DataSourceDescriptor&) override;
  void UnregisterDataSource(const std::string& name) override;
  void NotifyDataSourceStopped(perfetto::DataSourceInstanceID) override;
  size_t shared_buffer_page_size_kb() const override;

  static void ResetTaskRunnerForTesting();

 private:
  void CommitDataOnSequence(mojom::CommitDataRequestPtr request);
  void AddDataSourceOnSequence(DataSourceBase*);

  // The callback will be run on the |origin_task_runner|, meaning
  // the same sequence as CreateMojoMessagePipes() got called on.
  void CreateMojoMessagepipesOnSequence(
      scoped_refptr<base::SequencedTaskRunner> origin_task_runner,
      MessagepipesReadyCallback,
      mojom::ProducerClientRequest,
      mojom::ProducerClientPtr);

  std::unique_ptr<mojo::Binding<mojom::ProducerClient>> binding_;
  std::unique_ptr<perfetto::SharedMemoryArbiter> shared_memory_arbiter_;
  mojom::ProducerHostPtr producer_host_;
  std::unique_ptr<MojoSharedMemory> shared_memory_;
  std::set<DataSourceBase*> data_sources_;
  // First value is the flush ID, the second is the number of
  // replies we're still waiting for.
  std::pair<uint64_t, size_t> pending_replies_for_latest_flush_;

  SEQUENCE_CHECKER(sequence_checker_);

  // NOTE: Weak pointers must be invalidated before all other member variables.
  base::WeakPtrFactory<ProducerClient> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ProducerClient);
};

}  // namespace tracing

#endif  // SERVICES_TRACING_PUBLIC_CPP_PERFETTO_PRODUCER_CLIENT_H_

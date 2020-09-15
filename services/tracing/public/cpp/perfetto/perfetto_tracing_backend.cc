// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_tracing_backend.h"

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "base/task/post_task.h"
#include "build/build_config.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"
#include "services/tracing/public/cpp/perfetto/perfetto_producer.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/perfetto/trace_packet_tokenizer.h"
#include "services/tracing/public/mojom/perfetto_service.mojom.h"
#include "services/tracing/public/mojom/tracing_service.mojom.h"
#include "third_party/perfetto/include/perfetto/base/task_runner.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/consumer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/producer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/shared_memory_arbiter.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_packet.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_stats.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/tracing_service.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace tracing {
namespace {

// TODO(crbug.com/83907): Find a good compromise between performance and
// data granularity (mainly relevant to running with small buffer sizes
// when we use background tracing) on Android.
#if defined(OS_ANDROID)
constexpr size_t kDefaultSMBPageSizeBytes = 4 * 1024;
#else
constexpr size_t kDefaultSMBPageSizeBytes = 32 * 1024;
#endif

// TODO(crbug.com/839071): Figure out a good buffer size.
constexpr size_t kDefaultSMBSizeBytes = 4 * 1024 * 1024;

}  // namespace

// Implements Perfetto's ProducerEndpoint interface on top of the
// PerfettoService mojo service.
class ProducerEndpoint : public perfetto::ProducerEndpoint,
                         public mojom::ProducerClient {
 public:
  ProducerEndpoint(PerfettoTracingBackend::Delegate& delegate,
                   const std::string& producer_name,
                   perfetto::Producer* producer,
                   perfetto::base::TaskRunner* producer_task_runner,
                   size_t shmem_size_hint_bytes,
                   size_t shmem_page_size_hint_bytes)
      : producer_(producer) {
    // The producers's task runner must match where the endpoint is
    // constructed; otherwise we can't safely use a weak pointer to send
    // events back to the producer. |producer_task_runner| is also assumed to
    // outlive this endpoint.
    DCHECK(producer_task_runner->RunsTasksOnCurrentThread());
    delegate.CreateProducerConnection(
        base::BindOnce(&ProducerEndpoint::OnConnected,
                       weak_factory_.GetWeakPtr(), producer_task_runner,
                       shmem_size_hint_bytes, shmem_page_size_hint_bytes));
  }

  ~ProducerEndpoint() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    producer_->OnDisconnect();
  }

  // perfetto::ProducerEndpoint implementation:
  void RegisterDataSource(
      const perfetto::DataSourceDescriptor& descriptor) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    producer_host_->RegisterDataSource(descriptor);
  }

  void UnregisterDataSource(const std::string& name) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Implement data source unregistering.
    NOTREACHED();
  }

  void RegisterTraceWriter(uint32_t writer_id,
                           uint32_t target_buffer) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    producer_host_->RegisterTraceWriter(writer_id, target_buffer);
  }

  void UnregisterTraceWriter(uint32_t writer_id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    producer_host_->UnregisterTraceWriter(writer_id);
  }

  void CommitData(const perfetto::CommitDataRequest& commit,
                  CommitDataCallback callback = {}) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto commit_callback =
        callback
            ? base::BindOnce(
                  [](perfetto::ProducerEndpoint::CommitDataCallback callback) {
                    callback();
                  },
                  callback)
            : mojom::ProducerHost::CommitDataCallback();
    // We need to make sure the CommitData IPC is sent off without triggering
    // any trace events, as that could stall waiting for SMB chunks to be freed
    // up which requires the tracing service to receive the IPC.
    if (!TraceEventDataSource::GetThreadIsInTraceEventTLS()->Get()) {
      AutoThreadLocalBoolean thread_is_in_trace_event(
          TraceEventDataSource::GetThreadIsInTraceEventTLS());
      producer_host_->CommitData(commit, std::move(commit_callback));
      return;
    }
    producer_host_->CommitData(commit, std::move(commit_callback));
  }

  perfetto::SharedMemory* shared_memory() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return shared_memory_.get();
  }

  size_t shared_buffer_page_size_kb() const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return shared_buffer_page_size_kb_;
  }

  std::unique_ptr<perfetto::TraceWriter> CreateTraceWriter(
      perfetto::BufferID target_buffer,
      perfetto::BufferExhaustedPolicy) override {
    // Can be called from any thread.
    // Chromium uses BufferExhaustedPolicy::kDrop to avoid stalling trace
    // writers when the chunks in the SMB are exhausted. Stalling could
    // otherwise lead to deadlocks in chromium, because a stalled mojo IPC
    // thread could prevent CommitRequest messages from reaching the perfetto
    // service.
    return MaybeSharedMemoryArbiter()->CreateTraceWriter(
        target_buffer, perfetto::BufferExhaustedPolicy::kDrop);
  }

  perfetto::SharedMemoryArbiter* MaybeSharedMemoryArbiter() override {
    // Can be called from any thread.
    return shared_memory_arbiter_.get();
  }

  bool IsShmemProvidedByProducer() const override { return true; }

  void NotifyFlushComplete(perfetto::FlushRequestID flush_request_id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    perfetto::CommitDataRequest commit;
    commit.set_flush_request_id(flush_request_id);
    producer_host_->CommitData(commit,
                               mojom::ProducerHost::CommitDataCallback());
  }

  void NotifyDataSourceStarted(
      perfetto::DataSourceInstanceID instance_id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto it = ds_start_callbacks_.find(instance_id);
    if (it == ds_start_callbacks_.end())
      return;
    auto callback = std::move(it->second);
    ds_start_callbacks_.erase(it);
    std::move(callback).Run();
  }

  void NotifyDataSourceStopped(
      perfetto::DataSourceInstanceID instance_id) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto it = ds_stop_callbacks_.find(instance_id);
    if (it == ds_stop_callbacks_.end())
      return;
    auto callback = std::move(it->second);
    ds_stop_callbacks_.erase(it);
    std::move(callback).Run();
  }

  void ActivateTriggers(const std::vector<std::string>&) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Implement.
    NOTREACHED();
  }

  void Sync(std::function<void()> callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Implement.
    NOTREACHED();
  }

  // mojom::ProducerClient implementation.
  void OnTracingStart() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    producer_->OnTracingSetup();
  }

  void StartDataSource(uint64_t id,
                       const perfetto::DataSourceConfig& data_source_config,
                       StartDataSourceCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(ds_start_callbacks_.find(id) == ds_start_callbacks_.end());
    ds_start_callbacks_[id] = std::move(callback);
    auto it_and_inserted = ds_instances_.insert(id);
    DCHECK(it_and_inserted.second);
    producer_->SetupDataSource(id, data_source_config);
    producer_->StartDataSource(id, data_source_config);
  }

  void StopDataSource(uint64_t id, StopDataSourceCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(ds_stop_callbacks_.find(id) == ds_stop_callbacks_.end());
    ds_stop_callbacks_[id] = std::move(callback);
    ds_instances_.erase(id);
    producer_->StopDataSource(id);
  }

  void Flush(uint64_t flush_request_id,
             const std::vector<uint64_t>& data_source_ids) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    producer_->Flush(flush_request_id, data_source_ids.data(),
                     data_source_ids.size());
  }

  void ClearIncrementalState() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::vector<perfetto::DataSourceInstanceID> ds_instances;
    for (auto id : ds_instances_)
      ds_instances.push_back(id);
    producer_->ClearIncrementalState(ds_instances.data(), ds_instances.size());
  }

 private:
  struct EndpointBindings {
    mojo::PendingReceiver<mojom::ProducerClient> client_receiver;
    mojo::PendingRemote<mojom::ProducerHost> host_remote;
    std::unique_ptr<MojoSharedMemory> shared_memory;
  };

  static void OnConnected(
      base::WeakPtr<ProducerEndpoint> weak_endpoint,
      perfetto::base::TaskRunner* producer_task_runner,
      size_t shmem_size_bytes,
      size_t shmem_page_size_bytes,
      mojo::PendingRemote<mojom::PerfettoService> perfetto_service) {
    // Called on the connection's sequence -- |this| may have been deleted.
    auto bindings = std::make_unique<EndpointBindings>();

    // TODO(skyostil): Make it possible to pass the shared memory allocation
    // from the client library to here (for startup tracing).
    if (!shmem_size_bytes)
      shmem_size_bytes = kDefaultSMBSizeBytes;
    if (!shmem_page_size_bytes)
      shmem_page_size_bytes = kDefaultSMBPageSizeBytes;
    bindings->shared_memory =
        std::make_unique<MojoSharedMemory>(shmem_size_bytes);

    if (!bindings->shared_memory->shared_buffer().is_valid()) {
      // There's no way to do tracing after an SMB allocation failure, so let's
      // disconnect Perfetto.
      // TODO(skyostil): Record failure in UMA.
      producer_task_runner->PostTask([weak_endpoint] {
        if (!weak_endpoint)
          return;
        DCHECK_CALLED_ON_VALID_SEQUENCE(weak_endpoint->sequence_checker_);
        weak_endpoint->producer_->OnDisconnect();
      });
      return;
    }

    mojo::PendingRemote<mojom::ProducerClient> client;
    bindings->client_receiver = client.InitWithNewPipeAndPassReceiver();
    mojo::Remote<mojom::PerfettoService>(std::move(perfetto_service))
        ->ConnectToProducerHost(
            std::move(client),
            bindings->host_remote.InitWithNewPipeAndPassReceiver(),
            bindings->shared_memory->Clone(), shmem_page_size_bytes);

    // Bind the interfaces on Perfetto's sequence so we can avoid extra thread
    // hops.
    producer_task_runner->PostTask([weak_endpoint, producer_task_runner,
                                    raw_bindings = bindings.release(),
                                    shmem_size_bytes, shmem_page_size_bytes]() {
      auto bindings = base::WrapUnique(raw_bindings);
      // Called on the endpoint's sequence -- |endpoint| may be deleted.
      if (!weak_endpoint)
        return;
      weak_endpoint->BindConnectionOnSequence(
          producer_task_runner, std::move(bindings), shmem_size_bytes,
          shmem_page_size_bytes);
    });
  }

  void BindConnectionOnSequence(
      perfetto::base::TaskRunner* producer_task_runner,
      std::unique_ptr<EndpointBindings> bindings,
      size_t shmem_size_bytes,
      size_t shmem_page_size_bytes) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    producer_host_.Bind(std::move(bindings->host_remote));
    producer_host_.reset_on_disconnect();
    receiver_ = std::make_unique<mojo::Receiver<mojom::ProducerClient>>(
        this, std::move(bindings->client_receiver));
    receiver_->set_disconnect_handler(base::BindOnce(
        [](ProducerEndpoint* endpoint) { endpoint->receiver_->reset(); },
        base::Unretained(this)));

    shared_memory_ = std::move(bindings->shared_memory);
    shared_buffer_page_size_kb_ = shmem_size_bytes / 1024;
    shared_memory_arbiter_ = perfetto::SharedMemoryArbiter::CreateInstance(
        shared_memory_.get(), shmem_page_size_bytes, this,
        producer_task_runner);
    producer_->OnConnect();
  }

  SEQUENCE_CHECKER(sequence_checker_);

  perfetto::Producer* const producer_;

  base::flat_map<perfetto::DataSourceInstanceID, StartDataSourceCallback>
      ds_start_callbacks_;
  base::flat_map<perfetto::DataSourceInstanceID, StopDataSourceCallback>
      ds_stop_callbacks_;
  base::flat_set<perfetto::DataSourceInstanceID> ds_instances_;

  std::unique_ptr<mojo::Receiver<mojom::ProducerClient>> receiver_;
  mojo::Remote<mojom::ProducerHost> producer_host_;

  size_t shared_buffer_page_size_kb_ = 0;

  // Accessed on arbitrary threads after setup.
  std::unique_ptr<MojoSharedMemory> shared_memory_;
  std::unique_ptr<perfetto::SharedMemoryArbiter> shared_memory_arbiter_;

  base::WeakPtrFactory<ProducerEndpoint> weak_factory_{this};
};

// Implements Perfetto's ConsumerEndpoint interface on top of the
// ConsumerHost mojo service.
class ConsumerEndpoint : public perfetto::ConsumerEndpoint,
                         public mojom::TracingSessionClient,
                         public mojo::DataPipeDrainer::Client {
 public:
  ConsumerEndpoint(PerfettoTracingBackend::Delegate& delegate,
                   perfetto::Consumer* consumer,
                   perfetto::base::TaskRunner* consumer_task_runner)
      : consumer_{consumer} {
    // To avoid extra thread hops, the consumer's task runner must match where
    // the endpoint is constructed.
    DCHECK(consumer_task_runner->RunsTasksOnCurrentThread());
    delegate.CreateConsumerConnection(
        base::BindOnce(&ConsumerEndpoint::OnConnected,
                       weak_factory_.GetWeakPtr(), consumer_task_runner));
  }

  ~ConsumerEndpoint() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    consumer_->OnDisconnect();
  }

  // perfetto::ConsumerEndpoint implementation.
  void EnableTracing(const perfetto::TraceConfig& trace_config,
                     perfetto::base::ScopedFile file) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!file);  // Direct tracing to a file isn't supported.
    trace_config_ = trace_config;
    if (!trace_config.deferred_start())
      StartTracing();
  }

  void ChangeTraceConfig(const perfetto::TraceConfig& trace_config) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    trace_config_ = trace_config;
    tracing_session_host_->ChangeTraceConfig(trace_config);
  }

  void StartTracing() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto priority = mojom::TracingClientPriority::kUnknown;
    for (const auto& data_source : trace_config_.data_sources()) {
      if (!data_source.has_config() ||
          !data_source.config().has_chrome_config()) {
        continue;
      }
      switch (data_source.config().chrome_config().client_priority()) {
        case perfetto::protos::gen::ChromeConfig::BACKGROUND:
          priority =
              std::max(priority, mojom::TracingClientPriority::kBackground);
          break;
        case perfetto::protos::gen::ChromeConfig::USER_INITIATED:
          priority =
              std::max(priority, mojom::TracingClientPriority::kUserInitiated);
          break;
        default:
        case perfetto::protos::gen::ChromeConfig::UNKNOWN:
          break;
      }
    }
    consumer_host_->EnableTracing(
        tracing_session_host_.BindNewPipeAndPassReceiver(),
        tracing_session_client_.BindNewPipeAndPassRemote(), trace_config_,
        priority);
    tracing_session_host_.set_disconnect_handler(base::BindOnce(
        &ConsumerEndpoint::OnTracingFailed, base::Unretained(this)));
    tracing_session_client_.set_disconnect_handler(base::BindOnce(
        &ConsumerEndpoint::OnTracingFailed, base::Unretained(this)));
  }

  void DisableTracing() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    tracing_session_host_->DisableTracing();
  }

  void Flush(uint32_t timeout_ms, FlushCallback callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Implement flushing.
    NOTREACHED();
  }

  void ReadBuffers() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!tracing_session_host_ || !tracing_session_client_.is_bound()) {
      OnTracingFailed();
      return;
    }
    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    MojoResult result =
        mojo::CreateDataPipe(nullptr, &producer_handle, &consumer_handle);
    if (result != MOJO_RESULT_OK) {
      OnTracingFailed();
      return;
    }
    drainer_ = std::make_unique<mojo::DataPipeDrainer>(
        this, std::move(consumer_handle));
    trace_data_complete_ = false;
    read_buffers_complete_ = false;

    // Convert to legacy JSON if needed.
    for (const auto& data_source : trace_config_.data_sources()) {
      if (data_source.config().has_chrome_config() &&
          data_source.config().chrome_config().convert_to_legacy_json()) {
        tracing_session_host_->DisableTracingAndEmitJson(
            /*agent_label_filter=*/"", std::move(producer_handle),
            /*privacy_filter_enabled=*/false,
            base::BindOnce(&ConsumerEndpoint::OnReadBuffersComplete,
                           base::Unretained(this)));
        return;
      }
    }

    tokenizer_ = std::make_unique<TracePacketTokenizer>();
    tracing_session_host_->ReadBuffers(
        std::move(producer_handle),
        base::BindOnce(&ConsumerEndpoint::OnReadBuffersComplete,
                       base::Unretained(this)));
  }

  void FreeBuffers() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    tracing_session_host_.reset();
    tracing_session_client_.reset();
    drainer_.reset();
    tokenizer_.reset();
  }

  void Detach(const std::string& key) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    NOTREACHED() << "Detaching session not supported";
  }

  void Attach(const std::string& key) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    NOTREACHED() << "Attaching session not supported";
  }

  void GetTraceStats() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Capturing |this| is safe, because the callback will be cancelled if the
    // connection terminates.
    tracing_session_host_->RequestBufferUsage(base::BindOnce(
        [](ConsumerEndpoint* endpoint, bool success, float percent_full,
           bool data_loss) {
          DCHECK_CALLED_ON_VALID_SEQUENCE(endpoint->sequence_checker_);
          // Since we only get a few basic stats from the service, synthesize
          // just enough trace statistics to be able to show a buffer usage
          // indicator.
          // TODO(skyostil): Plumb the entire TraceStats objects from the
          // service to avoid this.
          uint64_t buffer_size = 0;
          if (endpoint->trace_config_.buffers_size() >= 1) {
            buffer_size = endpoint->trace_config_.buffers()[0].size_kb() * 1024;
          }
          perfetto::TraceStats stats;
          if (success && buffer_size) {
            auto* buf_stats = stats.add_buffer_stats();
            buf_stats->set_buffer_size(buffer_size);
            buf_stats->set_bytes_written(
                static_cast<uint64_t>(percent_full * buffer_size));
            if (data_loss)
              buf_stats->set_trace_writer_packet_loss(1);
          }
          endpoint->consumer_->OnTraceStats(success, stats);
        },
        base::Unretained(this)));
  }

  void ObserveEvents(uint32_t events_mask) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(!(events_mask &
             ~perfetto::ObservableEvents::TYPE_DATA_SOURCES_INSTANCES));
    observed_events_mask_ = events_mask;
  }

  void QueryServiceState(QueryServiceStateCallback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Implement service state querying.
    NOTREACHED();
  }

  void QueryCapabilities(QueryCapabilitiesCallback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Implement capability querying.
    NOTREACHED();
  }

  // tracing::mojom::TracingSessionClient implementation:
  void OnTracingEnabled() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Wire up full data source state. For now Perfetto just
    // needs to know all data sources have started.
    if (observed_events_mask_ &
        perfetto::ObservableEvents::TYPE_DATA_SOURCES_INSTANCES) {
      perfetto::ObservableEvents events;
      events.add_instance_state_changes()->set_state(
          perfetto::ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STARTED);
      consumer_->OnObservableEvents(events);
    }
  }

  void OnTracingDisabled() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Wire up full data source state. For now Perfetto just
    // needs to know all data sources have stopped.
    if (observed_events_mask_ &
        perfetto::ObservableEvents::TYPE_DATA_SOURCES_INSTANCES) {
      perfetto::ObservableEvents events;
      events.add_instance_state_changes()->set_state(
          perfetto::ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STOPPED);
      consumer_->OnObservableEvents(events);
    }
    consumer_->OnTracingDisabled();
  }

  // mojo::DataPipeDrainer::Client implementation:
  void OnDataAvailable(const void* data, size_t num_bytes) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (tokenizer_) {
      // Protobuf-format data.
      auto packets =
          tokenizer_->Parse(reinterpret_cast<const uint8_t*>(data), num_bytes);
      if (!packets.empty())
        consumer_->OnTraceData(std::move(packets), /*has_more=*/true);
    } else {
      // Legacy JSON-format data.
      std::vector<perfetto::TracePacket> packets;
      packets.emplace_back();
      packets.back().AddSlice(data, num_bytes);
      consumer_->OnTraceData(std::move(packets), /*has_more=*/true);
    }
  }

  void OnDataComplete() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (tokenizer_) {
      DCHECK(!tokenizer_->has_more());
      tokenizer_.reset();
    }
    trace_data_complete_ = true;
    MaybeFinishTraceData();
  }

 private:
  static void OnConnected(
      base::WeakPtr<ConsumerEndpoint> weak_endpoint,
      perfetto::base::TaskRunner* consumer_task_runner,
      mojo::PendingRemote<mojom::ConsumerHost> consumer_host_remote) {
    // Called on the connection's sequence -- |this| may have been deleted.
    auto wrapped_remote =
        std::make_unique<mojo::PendingRemote<mojom::ConsumerHost>>(
            std::move(consumer_host_remote));

    // Bind the interfaces on Perfetto's sequence so we can avoid extra thread
    // hops.
    consumer_task_runner->PostTask([weak_endpoint,
                                    raw_remote = wrapped_remote.release()]() {
      auto consumer_host_remote = base::WrapUnique(raw_remote);
      // Called on the endpoint's sequence -- |endpoint| may be deleted.
      if (!weak_endpoint)
        return;
      weak_endpoint->BindConnectionOnSequence(std::move(*consumer_host_remote));
    });
  }

  void BindConnectionOnSequence(
      mojo::PendingRemote<mojom::ConsumerHost> consumer_host_remote) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    consumer_host_.Bind(std::move(consumer_host_remote));
    consumer_host_.reset_on_disconnect();
    consumer_->OnConnect();
  }

  void OnTracingFailed() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Inform the crew.
    tracing_session_host_.reset();
    tracing_session_client_.reset();
    drainer_.reset();
    tokenizer_.reset();
  }

  void OnReadBuffersComplete() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    read_buffers_complete_ = true;
    MaybeFinishTraceData();
  }

  void MaybeFinishTraceData() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (!read_buffers_complete_ || !trace_data_complete_)
      return;
    consumer_->OnTraceData({}, /*has_more=*/false);
  }

  SEQUENCE_CHECKER(sequence_checker_);
  perfetto::Consumer* const consumer_;
  mojo::Remote<tracing::mojom::ConsumerHost> consumer_host_;
  mojo::Remote<tracing::mojom::TracingSessionHost> tracing_session_host_;
  mojo::Receiver<tracing::mojom::TracingSessionClient> tracing_session_client_{
      this};
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  perfetto::TraceConfig trace_config_;

  std::unique_ptr<TracePacketTokenizer> tokenizer_;
  bool read_buffers_complete_ = false;
  bool trace_data_complete_ = false;
  uint32_t observed_events_mask_ = 0;

  base::WeakPtrFactory<ConsumerEndpoint> weak_factory_{this};
};

PerfettoTracingBackend::PerfettoTracingBackend(Delegate& delegate)
    : delegate_(delegate) {}

PerfettoTracingBackend::~PerfettoTracingBackend() = default;
PerfettoTracingBackend::Delegate::~Delegate() = default;

std::unique_ptr<perfetto::ConsumerEndpoint>
PerfettoTracingBackend::ConnectConsumer(const ConnectConsumerArgs& args) {
  return std::make_unique<ConsumerEndpoint>(delegate_, args.consumer,
                                            args.task_runner);
}

std::unique_ptr<perfetto::ProducerEndpoint>
PerfettoTracingBackend::ConnectProducer(const ConnectProducerArgs& args) {
  return std::make_unique<ProducerEndpoint>(
      delegate_, args.producer_name, args.producer, args.task_runner,
      args.shmem_size_hint_bytes, args.shmem_page_size_hint_bytes);
}

}  // namespace tracing

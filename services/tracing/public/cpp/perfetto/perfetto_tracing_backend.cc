// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/perfetto_tracing_backend.h"

#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/tracing/tracing_tls.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
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

using ShmemMode = perfetto::SharedMemoryArbiter::ShmemMode;

namespace tracing {
namespace {

// TODO(crbug.com/40574593): Find a good compromise between performance and
// data granularity (mainly relevant to running with small buffer sizes
// when we use background tracing) on Android.
#if BUILDFLAG(IS_ANDROID)
constexpr size_t kDefaultSMBPageSizeBytes = 4 * 1024;
#else
constexpr size_t kDefaultSMBPageSizeBytes = 32 * 1024;
#endif

// TODO(crbug.com/40574594): Figure out a good buffer size.
constexpr size_t kDefaultSMBSizeBytes = 4 * 1024 * 1024;

constexpr char kErrorTracingFailed[] = "Tracing failed";

}  // namespace

// Implements Perfetto's ProducerEndpoint interface on top of the
// PerfettoService mojo service.
class ProducerEndpoint : public perfetto::ProducerEndpoint,
                         public mojom::ProducerClient {
 public:
  ProducerEndpoint(const std::string& producer_name,
                   perfetto::Producer* producer,
                   perfetto::base::TaskRunner* producer_task_runner,
                   size_t shmem_page_size_bytes,
                   size_t shmem_size_bytes,
                   std::unique_ptr<ChromeBaseSharedMemory> shm,
                   std::unique_ptr<perfetto::SharedMemoryArbiter> shm_arbiter)
      : producer_(producer),
        shmem_page_size_bytes_(shmem_page_size_bytes),
        shmem_size_bytes_(shmem_size_bytes),
        shared_memory_(std::move(shm)),
        shared_memory_arbiter_(std::move(shm_arbiter)) {
    DCHECK(producer_task_runner->RunsTasksOnCurrentThread());
  }

  ~ProducerEndpoint() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  base::WeakPtr<ProducerEndpoint> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // perfetto::ProducerEndpoint implementation:
  void Disconnect() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    producer_->OnDisconnect();  // Will delete |this|.
  }

  void RegisterDataSource(
      const perfetto::DataSourceDescriptor& descriptor) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    producer_host_->RegisterDataSource(descriptor);
  }

  void UpdateDataSource(
      const perfetto::DataSourceDescriptor& descriptor) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    NOTREACHED_IN_MIGRATION();
  }

  void UnregisterDataSource(const std::string& name) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Implement data source unregistering. Data sources are
    // currently only unregistered in tests, and because the tracing service is
    // also torn down at the same time, we can ignore unregistrations here.
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
    if (!*base::tracing::GetThreadIsInTraceEvent()) {
      const base::AutoReset<bool> resetter(
          base::tracing::GetThreadIsInTraceEvent(), true);
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
    return shmem_page_size_bytes_ / 1024;
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
    NOTREACHED_IN_MIGRATION();
  }

  void Sync(std::function<void()> callback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Implement.
    NOTREACHED_IN_MIGRATION();
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
                     data_source_ids.size(), perfetto::FlushFlags(0));
  }

  void ClearIncrementalState() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    std::vector<perfetto::DataSourceInstanceID> ds_instances;
    for (auto id : ds_instances_)
      ds_instances.push_back(id);
    producer_->ClearIncrementalState(ds_instances.data(), ds_instances.size());
  }

  void BindConnection(
      perfetto::base::TaskRunner* producer_task_runner,
      mojo::PendingRemote<mojom::PerfettoService> perfetto_service) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    DCHECK(!shared_memory_ == !shared_memory_arbiter_);
    if (!shared_memory_) {
      shared_memory_ =
          std::make_unique<ChromeBaseSharedMemory>(shmem_size_bytes_);
    }

    mojo::PendingRemote<mojom::ProducerClient> client_remote;
    mojo::PendingRemote<mojom::ProducerHost> host_remote;
    auto client_receiver = client_remote.InitWithNewPipeAndPassReceiver();
    mojo::Remote<mojom::PerfettoService>(std::move(perfetto_service))
        ->ConnectToProducerHost(std::move(client_remote),
                                host_remote.InitWithNewPipeAndPassReceiver(),
                                shared_memory_->CloneRegion(),
                                shmem_page_size_bytes_);

    producer_host_.Bind(std::move(host_remote));
    receiver_ = std::make_unique<mojo::Receiver<mojom::ProducerClient>>(
        this, std::move(client_receiver));
    receiver_->set_disconnect_handler(base::BindOnce(
        [](ProducerEndpoint* endpoint) { endpoint->receiver_->reset(); },
        base::Unretained(this)));

    // The shared memory arbiter can call producer host methods if it has
    // uncommitted requests at this moment. So bind it to the producer only
    // after it has been connected to the host.
    if (shared_memory_arbiter_) {
      shared_memory_arbiter_->BindToProducerEndpoint(this,
                                                     producer_task_runner);
    } else {
      shared_memory_arbiter_ = perfetto::SharedMemoryArbiter::CreateInstance(
          shared_memory_.get(), shmem_page_size_bytes_, ShmemMode::kDefault,
          this, producer_task_runner);
    }

    // This backend connects to the custom mojo-based tracing service, which
    // always supports direct SMB patching.
    shared_memory_arbiter_->SetDirectSMBPatchingSupportedByService();

    producer_->OnConnect();
  }

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<perfetto::Producer> producer_;

  base::flat_map<perfetto::DataSourceInstanceID, StartDataSourceCallback>
      ds_start_callbacks_;
  base::flat_map<perfetto::DataSourceInstanceID, StopDataSourceCallback>
      ds_stop_callbacks_;
  base::flat_set<perfetto::DataSourceInstanceID> ds_instances_;

  std::unique_ptr<mojo::Receiver<mojom::ProducerClient>> receiver_;
  mojo::Remote<mojom::ProducerHost> producer_host_;

  size_t shmem_page_size_bytes_ = 0;
  size_t shmem_size_bytes_ = 0;

  // Accessed on arbitrary threads after setup.
  std::unique_ptr<ChromeBaseSharedMemory> shared_memory_;
  std::unique_ptr<perfetto::SharedMemoryArbiter> shared_memory_arbiter_;

  base::WeakPtrFactory<ProducerEndpoint> weak_factory_{this};
};

// Implements Perfetto's ConsumerEndpoint interface on top of the
// ConsumerHost mojo service.
class ConsumerEndpoint : public perfetto::ConsumerEndpoint,
                         public mojom::TracingSessionClient,
                         public mojo::DataPipeDrainer::Client {
 public:
  ConsumerEndpoint(perfetto::Consumer* consumer,
                   perfetto::base::TaskRunner* consumer_task_runner)
      : consumer_{consumer} {
    // To avoid extra thread hops, the consumer's task runner must match where
    // the endpoint is constructed.
    DCHECK(consumer_task_runner->RunsTasksOnCurrentThread());
  }

  ~ConsumerEndpoint() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    consumer_.ExtractAsDangling()->OnDisconnect();  // May delete |consumer_|.
  }

  base::WeakPtr<ConsumerEndpoint> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  // perfetto::ConsumerEndpoint implementation.
  void EnableTracing(const perfetto::TraceConfig& trace_config,
                     perfetto::base::ScopedFile file) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    trace_config_ = trace_config;
#if BUILDFLAG(IS_WIN)
    // TODO(crbug.com/40736989): Add support on Windows.
    DCHECK(!file)
        << "Tracing directly to a file isn't supported on Windows yet";
#else
    output_file_ = base::File(file.release());
#endif
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
    tracing_failed_ = false;
    consumer_host_->EnableTracing(
        tracing_session_host_.BindNewPipeAndPassReceiver(),
        tracing_session_client_.BindNewPipeAndPassRemote(), trace_config_,
        std::move(output_file_));
    tracing_session_host_.set_disconnect_handler(base::BindOnce(
        &ConsumerEndpoint::OnTracingFailed, base::Unretained(this)));
    tracing_session_client_.set_disconnect_handler(base::BindOnce(
        &ConsumerEndpoint::OnTracingFailed, base::Unretained(this)));
  }

  void DisableTracing() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (tracing_session_host_)
      tracing_session_host_->DisableTracing();
  }

  void Flush(uint32_t timeout_ms,
             FlushCallback callback,
             perfetto::FlushFlags) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Implement flushing.
    NOTREACHED_IN_MIGRATION();
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
        mojo::CreateDataPipe(nullptr, producer_handle, consumer_handle);
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
            data_source.config().chrome_config().json_agent_label_filter(),
            std::move(producer_handle),
            data_source.config().chrome_config().privacy_filtering_enabled(),
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
    NOTREACHED_IN_MIGRATION() << "Detaching session not supported";
  }

  void Attach(const std::string& key) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    NOTREACHED_IN_MIGRATION() << "Attaching session not supported";
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
    // Only some events are currently supported by this backend.
    DCHECK(!(events_mask &
             ~(perfetto::ObservableEvents::TYPE_DATA_SOURCES_INSTANCES |
               perfetto::ObservableEvents::TYPE_ALL_DATA_SOURCES_STARTED)));
    observed_events_mask_ = events_mask;
  }

  void QueryServiceState(QueryServiceStateArgs,
                         QueryServiceStateCallback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Implement service state querying.
    NOTREACHED_IN_MIGRATION();
  }

  void QueryCapabilities(QueryCapabilitiesCallback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Implement capability querying.
    NOTREACHED_IN_MIGRATION();
  }

  void SaveTraceForBugreport(SaveTraceForBugreportCallback) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Not implemented yet.
    NOTREACHED_IN_MIGRATION();
  }

  void CloneSession(perfetto::TracingSessionID, CloneSessionArgs) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Not implemented yet.
    NOTREACHED_IN_MIGRATION();
  }

  // tracing::mojom::TracingSessionClient implementation:
  void OnTracingEnabled() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // TODO(skyostil): Wire up full data source state. For now Perfetto just
    // needs to know all data sources have started.
    if (observed_events_mask_ &
        (perfetto::ObservableEvents::TYPE_ALL_DATA_SOURCES_STARTED |
         perfetto::ObservableEvents::TYPE_DATA_SOURCES_INSTANCES)) {
      perfetto::ObservableEvents events;
      events.add_instance_state_changes()->set_state(
          perfetto::ObservableEvents::DATA_SOURCE_INSTANCE_STATE_STARTED);
      events.set_all_data_sources_started(true);
      consumer_->OnObservableEvents(events);
    }
  }

  void OnTracingDisabled(bool tracing_succeeded) override {
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
    consumer_->OnTracingDisabled(
        tracing_succeeded && !tracing_failed_ ? "" : kErrorTracingFailed);
  }

  // mojo::DataPipeDrainer::Client implementation:
  void OnDataAvailable(base::span<const uint8_t> data) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (tokenizer_) {
      // Protobuf-format data.
      auto packets = tokenizer_->Parse(data.data(), data.size());
      if (!packets.empty())
        consumer_->OnTraceData(std::move(packets), /*has_more=*/true);
    } else {
      // Legacy JSON-format data.
      std::vector<perfetto::TracePacket> packets;
      packets.emplace_back();
      packets.back().AddSlice(data.data(), data.size());
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

  void BindConnection(
      mojo::PendingRemote<mojom::ConsumerHost> consumer_host_remote) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    consumer_host_.Bind(std::move(consumer_host_remote));
    consumer_host_.reset_on_disconnect();
    consumer_->OnConnect();
  }

 private:
  void OnTracingFailed() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    tracing_failed_ = true;
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
  raw_ptr<perfetto::Consumer> consumer_;
  mojo::Remote<tracing::mojom::ConsumerHost> consumer_host_;
  mojo::Remote<tracing::mojom::TracingSessionHost> tracing_session_host_;
  mojo::Receiver<tracing::mojom::TracingSessionClient> tracing_session_client_{
      this};
  std::unique_ptr<mojo::DataPipeDrainer> drainer_;
  perfetto::TraceConfig trace_config_;
  base::File output_file_;

  std::unique_ptr<TracePacketTokenizer> tokenizer_;
  bool tracing_failed_ = false;
  bool read_buffers_complete_ = false;
  bool trace_data_complete_ = false;
  uint32_t observed_events_mask_ = 0;

  base::WeakPtrFactory<ConsumerEndpoint> weak_factory_{this};
};

PerfettoTracingBackend::PerfettoTracingBackend() {
  DETACH_FROM_SEQUENCE(muxer_sequence_checker_);
}

PerfettoTracingBackend::~PerfettoTracingBackend() = default;

std::unique_ptr<perfetto::ConsumerEndpoint>
PerfettoTracingBackend::ConnectConsumer(const ConnectConsumerArgs& args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(muxer_sequence_checker_);

  {
    base::AutoLock lock(task_runner_lock_);
    DCHECK(!muxer_task_runner_ || muxer_task_runner_ == args.task_runner);
    muxer_task_runner_ = args.task_runner;
  }
  auto consumer_endpoint =
      std::make_unique<ConsumerEndpoint>(args.consumer, args.task_runner);
  consumer_connection_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&PerfettoTracingBackend::CreateConsumerConnection,
                     base::Unretained(this), consumer_endpoint->GetWeakPtr()));
  return consumer_endpoint;
}

std::unique_ptr<perfetto::ProducerEndpoint>
PerfettoTracingBackend::ConnectProducer(const ConnectProducerArgs& args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(muxer_sequence_checker_);
  std::unique_ptr<ChromeBaseSharedMemory> shm;
  std::unique_ptr<perfetto::SharedMemoryArbiter> arbiter;
  uint32_t shmem_size_hint = args.shmem_size_hint_bytes;
  uint32_t shmem_page_size_hint = args.shmem_page_size_hint_bytes;
  if (shmem_size_hint == 0)
    shmem_size_hint = kDefaultSMBSizeBytes;
  if (shmem_page_size_hint == 0)
    shmem_page_size_hint = kDefaultSMBPageSizeBytes;

  if (args.use_producer_provided_smb) {
    shm = std::make_unique<ChromeBaseSharedMemory>(shmem_size_hint);
    arbiter = perfetto::SharedMemoryArbiter::CreateUnboundInstance(
        shm.get(), shmem_page_size_hint, ShmemMode::kDefault);
  }

  auto producer_endpoint = std::make_unique<ProducerEndpoint>(
      args.producer_name, args.producer, args.task_runner, shmem_page_size_hint,
      shmem_size_hint, std::move(shm), std::move(arbiter));

  {
    base::AutoLock lock(task_runner_lock_);
    DCHECK(!muxer_task_runner_ || muxer_task_runner_ == args.task_runner);
    muxer_task_runner_ = args.task_runner;
  }
  producer_endpoint_ = producer_endpoint->GetWeakPtr();

  // Return the ProducerEndpoint to the tracing muxer, and then call
  // BindProducerConnectionIfNecessary().
  muxer_task_runner_->PostTask([this] { BindProducerConnectionIfNecessary(); });
  return producer_endpoint;
}

void PerfettoTracingBackend::SetConsumerConnectionFactory(
    ConsumerConnectionFactory factory,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  consumer_connection_factory_ = factory;
  consumer_connection_task_runner_ = task_runner;
}

void PerfettoTracingBackend::OnProducerConnected(
    mojo::PendingRemote<mojom::PerfettoService> perfetto_service) {
  // Can be called on any thread.
  perfetto::base::TaskRunner* task_runner;
  {
    base::AutoLock lock(task_runner_lock_);
    task_runner = muxer_task_runner_;
    perfetto_service_ = std::move(perfetto_service);
  }

  if (task_runner) {
    task_runner->PostTask([this] { BindProducerConnectionIfNecessary(); });
  }
}

void PerfettoTracingBackend::BindProducerConnectionIfNecessary() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(muxer_sequence_checker_);
  if (!producer_endpoint_)
    return;

  mojo::PendingRemote<mojom::PerfettoService> perfetto_service;
  {
    base::AutoLock lock(task_runner_lock_);
    perfetto_service = std::move(perfetto_service_);
  }

  if (perfetto_service) {
    producer_endpoint_->BindConnection(muxer_task_runner_,
                                       std::move(perfetto_service));
  }
}

void PerfettoTracingBackend::CreateConsumerConnection(
    base::WeakPtr<ConsumerEndpoint> consumer_endpoint) {
  DCHECK(consumer_connection_task_runner_->RunsTasksInCurrentSequence());
  auto consumer_host_remote =
      std::make_unique<mojo::PendingRemote<mojom::ConsumerHost>>();
  auto& tracing_service = consumer_connection_factory_();
  tracing_service.BindConsumerHost(
      consumer_host_remote->InitWithNewPipeAndPassReceiver());
  muxer_task_runner_->PostTask(
      [consumer_endpoint, raw_ptr = consumer_host_remote.release()] {
        std::unique_ptr<mojo::PendingRemote<mojom::ConsumerHost>>
            consumer_host_remote(raw_ptr);
        if (!consumer_endpoint) {
          return;
        }
        consumer_endpoint->BindConnection(std::move(*consumer_host_remote));
      });
}

}  // namespace tracing

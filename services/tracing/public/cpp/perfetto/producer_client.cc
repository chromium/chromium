// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/producer_client.h"

#include <utility>

#include "base/auto_reset.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process.h"
#include "base/tracing/tracing_tls.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/traced_process_impl.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/shared_memory_arbiter.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_heap_buffer.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_stream_writer.h"
#include "third_party/perfetto/protos/perfetto/common/track_event_descriptor.pbzero.h"

namespace {
// Result for getting the shared buffer in InitSharedMemoryIfNeeded.
constexpr char kSharedBufferIsValidMetricName[] = "Tracing.SharedBufferIsValid";
}  // namespace

using ShmemMode = perfetto::SharedMemoryArbiter::ShmemMode;

namespace tracing {

ProducerClient::ProducerClient(base::tracing::PerfettoTaskRunner* task_runner)
    : PerfettoProducer(task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProducerClient::~ProducerClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ProducerClient::Connect(
    mojo::PendingRemote<mojom::PerfettoService> perfetto_service) {
  if (!InitSharedMemoryIfNeeded()) {
    LOG(ERROR) << "Failed to setup tracing service connection for this process";
    return;
  }

  base::UnsafeSharedMemoryRegion shm;
  {
    base::AutoLock lock(lock_);
    shm = shared_memory_->CloneRegion();
  }
  CHECK(shm.IsValid());

  mojo::PendingRemote<mojom::ProducerClient> client;
  auto client_receiver = client.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ProducerHost> producer_host_remote;
  mojo::Remote<mojom::PerfettoService>(std::move(perfetto_service))
      ->ConnectToProducerHost(
          std::move(client),
          producer_host_remote.InitWithNewPipeAndPassReceiver(), std::move(shm),
          kSMBPageSizeBytes);
  task_runner()->GetOrCreateTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProducerClient::BindClientAndHostPipesOnSequence,
                     base::Unretained(this), std::move(client_receiver),
                     std::move(producer_host_remote)));
}

void ProducerClient::BindInProcessSharedMemoryArbiter(
    perfetto::TracingService::ProducerEndpoint* producer_endpoint,
    base::tracing::PerfettoTaskRunner* task_runner) {
  DCHECK(!in_process_arbiter_task_runner_);
  in_process_arbiter_task_runner_ = task_runner;

  perfetto::SharedMemoryArbiter* arbiter;
  {
    base::AutoLock lock(lock_);
    // Shared memory should have been created before connecting to ProducerHost.
    DCHECK(shared_memory_arbiter_);
    // |shared_memory_arbiter_| is never destroyed, thus OK to call
    // BindToProducerEndpoint() without holding the lock.
    arbiter = shared_memory_arbiter_.get();
  }
  arbiter->BindToProducerEndpoint(producer_endpoint, task_runner);
}

void ProducerClient::Disconnect() {
  LOG(DFATAL) << "Not implemented yet";
}

void ProducerClient::BindStartupTargetBuffer(
    uint16_t target_buffer_reservation_id,
    perfetto::BufferID startup_target_buffer) {
  // While we should be called on the ProducerClient's task runner, it's
  // possible that the SMA lives on a different sequence (when in process).
  if (in_process_arbiter_task_runner_ &&
      !in_process_arbiter_task_runner_->RunsTasksOnCurrentThread()) {
    // |this| is never destroyed, except in tests.
    in_process_arbiter_task_runner_->GetOrCreateTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProducerClient::BindStartupTargetBuffer,
                       base::Unretained(this), target_buffer_reservation_id,
                       startup_target_buffer));
    return;
  }
  MaybeSharedMemoryArbiter()->BindStartupTargetBuffer(
      target_buffer_reservation_id, startup_target_buffer);
}

void ProducerClient::AbortStartupTracingForReservation(
    uint16_t target_buffer_reservation_id) {
  // While we should be called on the ProducerClient's task runner, it's
  // possible that the SMA lives on a different sequence (when in process).
  if (in_process_arbiter_task_runner_ &&
      !in_process_arbiter_task_runner_->RunsTasksOnCurrentThread()) {
    // |this| is never destroyed, except in tests.
    in_process_arbiter_task_runner_->GetOrCreateTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProducerClient::AbortStartupTracingForReservation,
                       base::Unretained(this), target_buffer_reservation_id));
    return;
  }
  MaybeSharedMemoryArbiter()->AbortStartupTracingForReservation(
      target_buffer_reservation_id);
}

perfetto::SharedMemoryArbiter* ProducerClient::MaybeSharedMemoryArbiter() {
  base::AutoLock lock(lock_);
  // |shared_memory_arbiter_| is never destroyed, thus OK to return a
  // reference here.
  return shared_memory_arbiter_.get();
}

void ProducerClient::NewDataSourceAdded(
    const PerfettoTracedProcess::DataSourceBase* const data_source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!producer_host_) {
    return;
  }
  perfetto::DataSourceDescriptor new_registration;
  new_registration.set_name(data_source->name());
  new_registration.set_will_notify_on_start(true);
  new_registration.set_will_notify_on_stop(true);
  new_registration.set_handles_incremental_state_clear(true);

  // Add categories to the DataSourceDescriptor.
  std::set<std::string> category_set;
  tracing::TracedProcessImpl::GetInstance()->GetCategories(&category_set);
  protozero::HeapBuffered<perfetto::protos::pbzero::TrackEventDescriptor> proto;
  for (const std::string& s : category_set) {
    auto* cat = proto->add_available_categories();
    cat->set_name(s);
    if (s.find(TRACE_DISABLED_BY_DEFAULT("")) == 0) {
      cat->add_tags("slow");
    }
  }
  new_registration.set_track_event_descriptor_raw(proto.SerializeAsString());

  producer_host_->RegisterDataSource(std::move(new_registration));
}

bool ProducerClient::IsTracingActive() {
  base::AutoLock lock(lock_);
  return data_sources_tracing_ > 0 || IsStartupTracingActive();
}

void ProducerClient::OnTracingStart() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(producer_host_);

  // In-process ProducerClient's arbiter is bound via
  // BindInProcessSharedMemoryArbiter() instead.
  if (!in_process_arbiter_task_runner_) {
    perfetto::SharedMemoryArbiter* arbiter;
    {
      base::AutoLock lock(lock_);
      // |shared_memory_arbiter_| is never destroyed, thus OK to call
      // BindToProducerEndpoint() without holding the lock.
      arbiter = shared_memory_arbiter_.get();
    }
    DCHECK(arbiter);
    arbiter->BindToProducerEndpoint(this, task_runner());
  }
}

void ProducerClient::StartDataSource(
    uint64_t id,
    const perfetto::DataSourceConfig& data_source_config,
    StartDataSourceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(oysteine): Support concurrent tracing sessions.
  for (PerfettoTracedProcess::DataSourceBase* data_source :
       PerfettoTracedProcess::Get()->data_sources()) {
    if (data_source->name() == data_source_config.name()) {
      {
        base::AutoLock lock(lock_);
        ++data_sources_tracing_;
      }
      // Now that a data source is active, mark the startup tracing session as
      // taken over by the service.
      OnStartupTracingComplete();
      // ProducerClient should never be denied permission to start, but it will
      // only start tracing once the callback passed below is called.
      bool result = PerfettoTracedProcess::Get()->CanStartTracing(
          this,
          base::BindOnce(
              [](base::WeakPtr<ProducerClient> weak_ptr,
                 PerfettoTracedProcess::DataSourceBase* data_source,
                 perfetto::DataSourceInstanceID id,
                 const perfetto::DataSourceConfig& data_source_config,
                 StartDataSourceCallback callback) {
                if (!weak_ptr) {
                  return;
                }

                DCHECK_CALLED_ON_VALID_SEQUENCE(weak_ptr->sequence_checker_);
                data_source->StartTracing(id, weak_ptr.get(),
                                          data_source_config);

                // TODO(eseckler): Consider plumbing this callback through
                // |data_source|.
                std::move(callback).Run();
              },
              weak_ptr_factory_.GetWeakPtr(), data_source, id,
              data_source_config, std::move(callback)));
      DCHECK(result);
      return;
    }
  }
}

void ProducerClient::StopDataSource(uint64_t id,
                                    StopDataSourceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (PerfettoTracedProcess::DataSourceBase* data_source :
       PerfettoTracedProcess::Get()->data_sources()) {
    if (data_source->data_source_id() == id &&
        data_source->producer() == this) {
      data_source->StopTracing(base::BindOnce(
          [](base::WeakPtr<ProducerClient> weak_ptr,
             StopDataSourceCallback callback, uint64_t id) {
            if (!weak_ptr) {
              std::move(callback).Run();
              return;
            }
            DCHECK_CALLED_ON_VALID_SEQUENCE(weak_ptr->sequence_checker_);
            // Flush any commits that might have been batched by
            // SharedMemoryArbiter.
            weak_ptr->MaybeSharedMemoryArbiter()
                ->FlushPendingCommitDataRequests();
            std::move(callback).Run();
            base::AutoLock lock(weak_ptr->lock_);
            --weak_ptr->data_sources_tracing_;
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), id));
      return;
    }
  }

  DLOG(ERROR) << "Invalid data source ID.";
}

void ProducerClient::Flush(uint64_t flush_request_id,
                           const std::vector<uint64_t>& data_source_ids) {
  pending_replies_for_latest_flush_ = {flush_request_id,
                                       data_source_ids.size()};

  // N^2, optimize once there's more than a couple of possible data sources.
  for (PerfettoTracedProcess::DataSourceBase* data_source :
       PerfettoTracedProcess::Get()->data_sources()) {
    if (base::Contains(data_source_ids, data_source->data_source_id())) {
      data_source->Flush(base::BindRepeating(
          [](base::WeakPtr<ProducerClient> weak_ptr, uint64_t id) {
            if (weak_ptr) {
              weak_ptr->NotifyDataSourceFlushComplete(id);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), flush_request_id));
    }
  }
}

void ProducerClient::ClearIncrementalState() {
  for (PerfettoTracedProcess::DataSourceBase* data_source :
       PerfettoTracedProcess::Get()->data_sources()) {
    data_source->ClearIncrementalState();
  }
}

std::unique_ptr<perfetto::TraceWriter> ProducerClient::CreateTraceWriter(
    perfetto::BufferID target_buffer,
    perfetto::BufferExhaustedPolicy buffer_exhausted_policy) {
  return PerfettoProducer::CreateTraceWriter(target_buffer,
                                             buffer_exhausted_policy);
}

void ProducerClient::CommitData(const perfetto::CommitDataRequest& commit,
                                CommitDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto commit_callback =
      callback ? base::BindOnce([](CommitDataCallback callback) { callback(); },
                                callback)
               : mojom::ProducerHost::CommitDataCallback();

  // We need to make sure the CommitData IPC is sent off without triggering any
  // trace events, as that could stall waiting for SMB chunks to be freed up
  // which requires the tracing service to receive the IPC.
  if (!*base::tracing::GetThreadIsInTraceEvent()) {
    const base::AutoReset<bool> resetter(
        base::tracing::GetThreadIsInTraceEvent(), true);

    producer_host_->CommitData(commit, std::move(commit_callback));
    return;
  }

  producer_host_->CommitData(commit, std::move(commit_callback));
}

void ProducerClient::RegisterTraceWriter(uint32_t writer_id,
                                         uint32_t target_buffer) {
  producer_host_->RegisterTraceWriter(writer_id, target_buffer);
}

void ProducerClient::UnregisterTraceWriter(uint32_t writer_id) {
  producer_host_->UnregisterTraceWriter(writer_id);
}

perfetto::SharedMemory* ProducerClient::shared_memory() const {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

void ProducerClient::NotifyFlushComplete(perfetto::FlushRequestID id) {
  NOTREACHED_IN_MIGRATION();
}

void ProducerClient::RegisterDataSource(const perfetto::DataSourceDescriptor&) {
  NOTREACHED_IN_MIGRATION();
}

void ProducerClient::UpdateDataSource(const perfetto::DataSourceDescriptor&) {
  NOTREACHED_IN_MIGRATION();
}

void ProducerClient::UnregisterDataSource(const std::string& name) {
  NOTREACHED_IN_MIGRATION();
}

void ProducerClient::NotifyDataSourceStopped(
    perfetto::DataSourceInstanceID id) {
  NOTREACHED_IN_MIGRATION();
}

void ProducerClient::NotifyDataSourceStarted(
    perfetto::DataSourceInstanceID id) {
  NOTREACHED_IN_MIGRATION();
}

void ProducerClient::ActivateTriggers(const std::vector<std::string>&) {
  NOTREACHED_IN_MIGRATION();
}

size_t ProducerClient::shared_buffer_page_size_kb() const {
  NOTREACHED_IN_MIGRATION();
  return 0;
}

bool ProducerClient::IsShmemProvidedByProducer() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void ProducerClient::Sync(std::function<void()>) {
  NOTREACHED_IN_MIGRATION();
}

void ProducerClient::BindClientAndHostPipesForTesting(
    mojo::PendingReceiver<mojom::ProducerClient> producer_client_receiver,
    mojo::PendingRemote<mojom::ProducerHost> producer_host_remote) {
  task_runner()->GetOrCreateTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProducerClient::BindClientAndHostPipesOnSequence,
                     base::Unretained(this),
                     std::move(producer_client_receiver),
                     std::move(producer_host_remote)));
}

perfetto::SharedMemory* ProducerClient::shared_memory_for_testing() {
  base::AutoLock lock(lock_);
  // |shared_memory_| is never destroyed except in tests, thus OK to return a
  // reference here.
  return shared_memory_.get();
}

bool ProducerClient::InitSharedMemoryIfNeeded() {
  base::AutoLock lock(lock_);
  if (shared_memory_) {
    return true;
  }

  // The shared memory buffer is always provided by the ProducerClient, but only
  // created upon the first tracing request.
  shared_memory_ =
      std::make_unique<ChromeBaseSharedMemory>(GetPreferredSmbSizeBytes());

  // TODO(crbug.com/40677516): We see shared memory region creation fail on
  // windows in the field. Investigate why this can happen. Gather statistics on
  // failure rates.
  bool valid = shared_memory_->region().IsValid();
  base::UmaHistogramBoolean(kSharedBufferIsValidMetricName, valid);

  if (!valid) {
    LOG(ERROR) << "Failed to create tracing SMB";
    shared_memory_.reset();
    return false;
  }

  shared_memory_arbiter_ = perfetto::SharedMemoryArbiter::CreateUnboundInstance(
      shared_memory_.get(), kSMBPageSizeBytes, ShmemMode::kDefault);
  shared_memory_arbiter_->SetDirectSMBPatchingSupportedByService();
  shared_memory_arbiter_->EnableDirectSMBPatching();
  shared_memory_arbiter_->SetBatchCommitsDuration(
      kShmArbiterBatchCommitDurationMs);
  return true;
}

bool ProducerClient::SetupSharedMemoryForStartupTracing() {
  return InitSharedMemoryIfNeeded();
}

// The Mojo binding should run on the same sequence as the one we get
// callbacks from Perfetto on, to avoid additional PostTasks.
void ProducerClient::BindClientAndHostPipesOnSequence(
    mojo::PendingReceiver<mojom::ProducerClient> producer_client_receiver,
    mojo::PendingRemote<mojom::ProducerHost> producer_host_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!receiver_ || !receiver_->is_bound());

  receiver_ = std::make_unique<mojo::Receiver<mojom::ProducerClient>>(
      this, std::move(producer_client_receiver));
  receiver_->set_disconnect_handler(base::BindOnce(
      [](ProducerClient* producer_client) {
        producer_client->receiver_->reset();
      },
      base::Unretained(this)));

  producer_host_.Bind(std::move(producer_host_info));

  // TODO(oysteine) We register the data sources in reverse as a temporary
  // workaround to make sure that the TraceEventDataSource is registered
  // *after* the MetadataSource, as the logic which waits for trace clients
  // to be "ready" (in the tracing coordinator) waits for the TraceLog to
  // be enabled, which is done by the TraceEventDataSource. We need to register
  // the MetadataSource first to ensure that it's also ready. Once the
  // Perfetto Observer interface is ready, we can remove this.
  const auto& data_sources = PerfettoTracedProcess::Get()->data_sources();
  for (const PerfettoTracedProcess::DataSourceBase* data_source :
       base::Reversed(data_sources)) {
    NewDataSourceAdded(data_source);
  }
}

void ProducerClient::NotifyDataSourceFlushComplete(
    perfetto::FlushRequestID id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_replies_for_latest_flush_.first != id) {
    // Ignore; completed flush was for an earlier request.
    return;
  }

  DCHECK_NE(pending_replies_for_latest_flush_.second, 0u);
  if (--pending_replies_for_latest_flush_.second == 0) {
    MaybeSharedMemoryArbiter()->NotifyFlushComplete(id);
  }
}

}  // namespace tracing

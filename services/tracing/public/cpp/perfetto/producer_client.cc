// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/perfetto/producer_client.h"

#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/process/process.h"
#include "base/task/post_task.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "services/tracing/public/cpp/perfetto/trace_event_data_source.h"
#include "services/tracing/public/cpp/traced_process_impl.h"
#include "services/tracing/public/mojom/constants.mojom.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/shared_memory_arbiter.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/startup_trace_writer_registry.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/trace_writer.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_heap_buffer.h"
#include "third_party/perfetto/include/perfetto/protozero/scattered_stream_writer.h"
#include "third_party/perfetto/protos/perfetto/common/track_event_descriptor.pbzero.h"

namespace tracing {

ProducerClient::ProducerClient(PerfettoTaskRunner* task_runner)
    : PerfettoProducer(task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

ProducerClient::~ProducerClient() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void ProducerClient::Connect(
    mojo::PendingRemote<mojom::PerfettoService> perfetto_service) {
  mojo::PendingRemote<mojom::ProducerClient> client;
  auto client_receiver = client.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<mojom::ProducerHost> producer_host_remote;
  mojo::Remote<mojom::PerfettoService>(std::move(perfetto_service))
      ->ConnectToProducerHost(
          std::move(client),
          producer_host_remote.InitWithNewPipeAndPassReceiver());
  task_runner()->GetOrCreateTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ProducerClient::BindClientAndHostPipesOnSequence,
                     base::Unretained(this), std::move(client_receiver),
                     std::move(producer_host_remote)));
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

void ProducerClient::ResetSequenceForTesting() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
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
  for (auto it = data_sources.crbegin(); it != data_sources.crend(); ++it) {
    NewDataSourceAdded(*it);
  }
}

void ProducerClient::NewDataSourceAdded(
    const PerfettoTracedProcess::DataSourceBase* const data_source) {
  if (!producer_host_) {
    return;
  }
  perfetto::DataSourceDescriptor new_registration;
  new_registration.set_name(data_source->name());
  new_registration.set_will_notify_on_start(true);
  new_registration.set_will_notify_on_stop(true);
  new_registration.set_handles_incremental_state_clear(true);

  // Add categories to the DataSourceDescriptor.
  protozero::ScatteredHeapBuffer buffer;
  protozero::ScatteredStreamWriter stream(&buffer);
  perfetto::protos::pbzero::TrackEventDescriptor proto;
  proto.Reset(&stream);
  buffer.set_writer(&stream);

  std::set<std::string> category_set;
  tracing::TracedProcessImpl::GetInstance()->GetCategories(&category_set);
  for (const std::string& s : category_set) {
    proto.add_available_categories(s.c_str());
  }

  auto raw_proto = buffer.StitchSlices();
  std::string track_event_descriptor_raw(raw_proto.begin(), raw_proto.end());
  new_registration.set_track_event_descriptor_raw(track_event_descriptor_raw);

  producer_host_->RegisterDataSource(std::move(new_registration));
}

perfetto::SharedMemoryArbiter* ProducerClient::GetSharedMemoryArbiter() {
  return in_process_arbiter_ ? in_process_arbiter_
                             : shared_memory_arbiter_.get();
}

bool ProducerClient::IsTracingActive() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return data_sources_tracing_ > 0;
}

void ProducerClient::OnTracingStart(
    mojo::ScopedSharedBufferHandle shared_memory,
    uint64_t shared_memory_buffer_page_size_bytes) {
  // If we're using in-process mode, we don't need to set up our
  // own SharedMemoryArbiter.
  DCHECK(!in_process_arbiter_);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(producer_host_);
  if (!shared_memory_) {
    shared_memory_ =
        std::make_unique<MojoSharedMemory>(std::move(shared_memory));

    shared_memory_arbiter_ = perfetto::SharedMemoryArbiter::CreateInstance(
        shared_memory_.get(), shared_memory_buffer_page_size_bytes, this,
        PerfettoTracedProcess::GetTaskRunner());
  } else {
    // TODO(oysteine): This is assuming the SMB is the same, currently. Swapping
    // out SharedMemoryBuffers would require more thread synchronization.
    DCHECK_EQ(shared_memory_->shared_buffer()->value(), shared_memory->value());
  }
}

void ProducerClient::StartDataSource(
    uint64_t id,
    const perfetto::DataSourceConfig& data_source_config,
    StartDataSourceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(oysteine): Support concurrent tracing sessions.
  for (auto* data_source : PerfettoTracedProcess::Get()->data_sources()) {
    if (data_source->name() == data_source_config.name()) {
      ++data_sources_tracing_;
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
                data_source->StartTracingWithID(id, weak_ptr.get(),
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

  for (auto* data_source : PerfettoTracedProcess::Get()->data_sources()) {
    if (data_source->data_source_id() == id &&
        data_source->producer() == this) {
      data_source->StopTracing(base::BindOnce(
          [](base::WeakPtr<ProducerClient> weak_ptr,
             StopDataSourceCallback callback, uint64_t id) {
            std::move(callback).Run();
            if (weak_ptr) {
              DCHECK_CALLED_ON_VALID_SEQUENCE(weak_ptr->sequence_checker_);
              --weak_ptr->data_sources_tracing_;
            }
          },
          weak_ptr_factory_.GetWeakPtr(), std::move(callback), id));
      return;
    }
  }

  LOG(DFATAL) << "Invalid data source ID.";
}

void ProducerClient::Flush(uint64_t flush_request_id,
                           const std::vector<uint64_t>& data_source_ids) {
  pending_replies_for_latest_flush_ = {flush_request_id,
                                       data_source_ids.size()};

  // N^2, optimize once there's more than a couple of possible data sources.
  for (auto* data_source : PerfettoTracedProcess::Get()->data_sources()) {
    if (std::find(data_source_ids.begin(), data_source_ids.end(),
                  data_source->data_source_id()) != data_source_ids.end()) {
      data_source->Flush(base::BindRepeating(
          [](base::WeakPtr<ProducerClient> weak_ptr, uint64_t id) {
            if (weak_ptr) {
              weak_ptr->NotifyFlushComplete(id);
            }
          },
          weak_ptr_factory_.GetWeakPtr(), flush_request_id));
    }
  }
}

void ProducerClient::ClearIncrementalState() {
  for (auto* data_source : PerfettoTracedProcess::Get()->data_sources()) {
    data_source->ClearIncrementalState();
  }
}

void ProducerClient::RegisterDataSource(const perfetto::DataSourceDescriptor&) {
  NOTREACHED();
}

void ProducerClient::UnregisterDataSource(const std::string& name) {
  NOTREACHED();
}

void ProducerClient::NotifyDataSourceStopped(
    perfetto::DataSourceInstanceID id) {
  NOTREACHED();
}

void ProducerClient::NotifyDataSourceStarted(
    perfetto::DataSourceInstanceID id) {
  NOTREACHED();
}

void ProducerClient::ActivateTriggers(const std::vector<std::string>&) {
  NOTREACHED();
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
  if (!TraceEventDataSource::GetThreadIsInTraceEventTLS()->Get()) {
    AutoThreadLocalBoolean thread_is_in_trace_event(
        TraceEventDataSource::GetThreadIsInTraceEventTLS());

    producer_host_->CommitData(commit, std::move(commit_callback));
    return;
  }

  producer_host_->CommitData(commit, std::move(commit_callback));
}

perfetto::SharedMemory* ProducerClient::shared_memory() const {
  return shared_memory_.get();
}

size_t ProducerClient::shared_buffer_page_size_kb() const {
  NOTREACHED();
  return 0;
}

perfetto::SharedMemoryArbiter* ProducerClient::GetInProcessShmemArbiter() {
  NOTREACHED();
  return nullptr;
}

void ProducerClient::NotifyFlushComplete(perfetto::FlushRequestID id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_replies_for_latest_flush_.first != id) {
    // Ignore; completed flush was for an earlier request.
    return;
  }

  DCHECK_NE(pending_replies_for_latest_flush_.second, 0u);
  if (--pending_replies_for_latest_flush_.second == 0) {
    GetSharedMemoryArbiter()->NotifyFlushComplete(id);
  }
}

void ProducerClient::RegisterTraceWriter(uint32_t writer_id,
                                         uint32_t target_buffer) {
  producer_host_->RegisterTraceWriter(writer_id, target_buffer);
}

void ProducerClient::UnregisterTraceWriter(uint32_t writer_id) {
  producer_host_->UnregisterTraceWriter(writer_id);
}

}  // namespace tracing

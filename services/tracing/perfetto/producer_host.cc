// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/producer_host.h"

#include <utility>

#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "third_party/perfetto/include/perfetto/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_descriptor.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace tracing {

ProducerHost::ProducerHost() = default;
ProducerHost::~ProducerHost() = default;

void ProducerHost::Initialize(mojom::ProducerClientPtr producer_client,
                              mojom::ProducerHostRequest producer_host,
                              perfetto::TracingService* service,
                              const std::string& name) {
  DCHECK(service);
  DCHECK(!producer_endpoint_);
  producer_client_ = std::move(producer_client);
  producer_client_.set_connection_error_handler(
      base::BindOnce(&ProducerHost::OnConnectionError, base::Unretained(this)));

  binding_ = std::make_unique<mojo::Binding<mojom::ProducerHost>>(
      this, std::move(producer_host));
  binding_->set_connection_error_handler(
      base::BindOnce(&ProducerHost::OnConnectionError, base::Unretained(this)));

  // TODO(oysteine): Figure out an uid once we need it.
  // TODO(oysteine): Figure out a good buffer size.
  producer_endpoint_ = service->ConnectProducer(
      this, 0 /* uid */, name,
      4 * 1024 * 1024 /* shared_memory_size_hint_bytes */);
  DCHECK(producer_endpoint_);
}

void ProducerHost::OnConnectionError() {
  // Manually reset to prevent any callbacks from the ProducerEndpoint
  // when we're in a half-destructed state.
  producer_endpoint_.reset();
  // If the ProducerHost is owned by the PerfettoService, let it know
  // we're disconnected to let this be cleaned up. Tests manage lifespan
  // themselves.
  if (connection_error_handler_) {
    std::move(connection_error_handler_).Run();
  }
  // This object *may* be destroyed at this point.
}

void ProducerHost::OnConnect() {
}

void ProducerHost::OnDisconnect() {
  // Deliberately empty, this is invoked by the |service_| business logic after
  // we destroy the |producer_endpoint|.
}

void ProducerHost::OnTracingSetup() {
  MojoSharedMemory* shared_memory =
      static_cast<MojoSharedMemory*>(producer_endpoint_->shared_memory());
  DCHECK(shared_memory);
  DCHECK(producer_client_);
  mojo::ScopedSharedBufferHandle shm = shared_memory->Clone();
  DCHECK(shm.is_valid());

  producer_client_->OnTracingStart(std::move(shm));
}

void ProducerHost::SetupDataSource(perfetto::DataSourceInstanceID,
                                   const perfetto::DataSourceConfig&) {
  // TODO(primiano): plumb call through mojo.
}

void ProducerHost::StartDataSource(perfetto::DataSourceInstanceID id,
                                   const perfetto::DataSourceConfig& config) {
  // TODO(oysteine): Send full DataSourceConfig, not just the name/target_buffer
  // and Chrome Tracing string.
  auto data_source_config = mojom::DataSourceConfig::New();
  data_source_config->name = config.name();
  data_source_config->target_buffer = config.target_buffer();

  data_source_config->trace_config = config.chrome_config().trace_config();
  producer_client_->StartDataSource(id, std::move(data_source_config));
}

void ProducerHost::StopDataSource(perfetto::DataSourceInstanceID id) {
  if (producer_client_) {
    producer_client_->StopDataSource(
        id,
        base::BindOnce(
            [](ProducerHost* producer_host, perfetto::DataSourceInstanceID id) {
              producer_host->producer_endpoint_->NotifyDataSourceStopped(id);
            },
            base::Unretained(this), id));
  }
}

void ProducerHost::Flush(
    perfetto::FlushRequestID id,
    const perfetto::DataSourceInstanceID* raw_data_source_ids,
    size_t num_data_sources) {
  DCHECK(producer_client_);
  std::vector<uint64_t> data_source_ids(raw_data_source_ids,
                                        raw_data_source_ids + num_data_sources);
  DCHECK_EQ(data_source_ids.size(), num_data_sources);
  producer_client_->Flush(id, data_source_ids);
}

// This data can come from a malicious child process. We don't do any
// sanitization here because ProducerEndpoint::CommitData() (And any other
// ProducerEndpoint methods) are designed to deal with malformed / malicious
// inputs.
void ProducerHost::CommitData(mojom::CommitDataRequestPtr data_request) {
  perfetto::CommitDataRequest native_data_request;

  // TODO(oysteine): Set up a TypeTrait for this instead of manual conversion.
  native_data_request.set_flush_request_id(data_request->flush_request_id);

  for (auto& chunk : data_request->chunks_to_move) {
    auto* new_chunk = native_data_request.add_chunks_to_move();
    new_chunk->set_page(chunk->page);
    new_chunk->set_chunk(chunk->chunk);
    new_chunk->set_target_buffer(chunk->target_buffer);
  }

  for (auto& chunk_patch : data_request->chunks_to_patch) {
    auto* new_chunk_patch = native_data_request.add_chunks_to_patch();
    new_chunk_patch->set_target_buffer(chunk_patch->target_buffer);
    new_chunk_patch->set_writer_id(chunk_patch->writer_id);
    new_chunk_patch->set_chunk_id(chunk_patch->chunk_id);

    for (auto& patch : chunk_patch->patches) {
      auto* new_patch = new_chunk_patch->add_patches();
      new_patch->set_offset(patch->offset);
      new_patch->set_data(patch->data);
    }

    new_chunk_patch->set_has_more_patches(chunk_patch->has_more_patches);
  }

  if (on_commit_callback_for_testing_) {
    on_commit_callback_for_testing_.Run(native_data_request);
  }

  // TODO(oysteine): Pass through an optional callback for
  // tests to know when a commit is completed.
  producer_endpoint_->CommitData(native_data_request);
}

void ProducerHost::RegisterDataSource(
    mojom::DataSourceRegistrationPtr registration_info) {
  perfetto::DataSourceDescriptor descriptor;
  descriptor.set_name(registration_info->name);
  descriptor.set_will_notify_on_stop(registration_info->will_notify_on_stop);
  producer_endpoint_->RegisterDataSource(descriptor);
}

void ProducerHost::NotifyFlushComplete(uint64_t flush_request_id) {
  producer_endpoint_->NotifyFlushComplete(flush_request_id);
}

}  // namespace tracing

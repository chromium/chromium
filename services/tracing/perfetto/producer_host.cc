// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/tracing/perfetto/producer_host.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/process/process.h"
#include "base/tracing/perfetto_task_runner.h"
#include "build/build_config.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/client_identity.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_descriptor.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace tracing {

ProducerHost::ProducerHost(base::tracing::PerfettoTaskRunner* task_runner)
    : task_runner_(task_runner) {}

ProducerHost::~ProducerHost() {
  // Manually reset to prevent any callbacks from the ProducerEndpoint
  // when we're in a half-destructed state.
  producer_endpoint_.reset();
}

ProducerHost::InitializationResult ProducerHost::Initialize(
    mojo::PendingRemote<mojom::ProducerClient> producer_client,
    perfetto::TracingService* service,
    const std::string& name,
    base::UnsafeSharedMemoryRegion shared_memory,
    uint64_t shared_memory_buffer_page_size_bytes) {
  DCHECK(service);
  DCHECK(!producer_endpoint_);

  producer_client_.Bind(std::move(producer_client));

  auto shm = std::make_unique<ChromeBaseSharedMemory>(std::move(shared_memory));
  // We may fail to map the buffer provided by the ProducerClient.
  if (!shm->start()) {
    return InitializationResult::kSmbMappingFailed;
  }

  size_t shm_size = shm->size();
  ChromeBaseSharedMemory* shm_raw = shm.get();

  // TODO(oysteine): Figure out a uid once we need it.
  producer_endpoint_ = service->ConnectProducer(
      this,
      perfetto::ClientIdentity(/*uid=*/0,
                               /*pid=*/perfetto::base::kInvalidPid),
      name, shm_size,
      /*in_process=*/false,
      perfetto::TracingService::ProducerSMBScrapingMode::kDefault,
      shared_memory_buffer_page_size_bytes, std::move(shm));

  // In some cases, the service may deny the producer connection (e.g. if too
  // many producers are registered).
  if (!producer_endpoint_) {
    return InitializationResult::kProducerEndpointConstructionFailed;
  }

  // The service will adopt the shared memory buffer provided by the
  // ProducerClient as long as it is correctly sized.
  if (producer_endpoint_->shared_memory() != shm_raw) {
    return InitializationResult::kSmbNotAdopted;
  }

  // TODO(skyostil): Implement arbiter binding for the client API.

  return InitializationResult::kSuccess;
}

void ProducerHost::OnConnect() {
}

void ProducerHost::OnDisconnect() {
  // Deliberately empty, this is invoked by the |service_| business logic after
  // we destroy the |producer_endpoint|.
}

void ProducerHost::OnTracingSetup() {
  producer_client_->OnTracingStart();
}

void ProducerHost::SetupDataSource(perfetto::DataSourceInstanceID,
                                   const perfetto::DataSourceConfig&) {
  // TODO(primiano): plumb call through mojo.
}

void ProducerHost::StartDataSource(perfetto::DataSourceInstanceID id,
                                   const perfetto::DataSourceConfig& config) {
  // The type traits will send the base fields in the DataSourceConfig and also
  // the ChromeConfig other configs are dropped.
  producer_client_->StartDataSource(
      id, config,
      base::BindOnce(
          [](ProducerHost* producer_host, perfetto::DataSourceInstanceID id) {
            producer_host->producer_endpoint_->NotifyDataSourceStarted(id);
          },
          base::Unretained(this), id));
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
    size_t num_data_sources,
    perfetto::FlushFlags /*ignored*/) {
  DCHECK(producer_client_);
  std::vector<uint64_t> data_source_ids(raw_data_source_ids,
                                        raw_data_source_ids + num_data_sources);
  DCHECK_EQ(data_source_ids.size(), num_data_sources);
  producer_client_->Flush(id, data_source_ids);
}

void ProducerHost::ClearIncrementalState(const perfetto::DataSourceInstanceID*,
                                         size_t) {
  DCHECK(producer_client_);
  producer_client_->ClearIncrementalState();
}

// This data can come from a malicious child process. We don't do any
// sanitization here because ProducerEndpoint::CommitData() (And any other
// ProducerEndpoint methods) are designed to deal with malformed / malicious
// inputs.
void ProducerHost::CommitData(const perfetto::CommitDataRequest& data_request,
                              CommitDataCallback callback) {
  if (on_commit_callback_for_testing_) {
    on_commit_callback_for_testing_.Run(data_request);
  }
  // This assumes that CommitData() will execute the callback synchronously.
  producer_endpoint_->CommitData(data_request, [&callback]() {
    std::move(callback).Run();
  });
  DCHECK(!callback);  // Should have been run synchronously above.
}

void ProducerHost::RegisterDataSource(
    const perfetto::DataSourceDescriptor& registration_info) {
  producer_endpoint_->RegisterDataSource(registration_info);
}

void ProducerHost::RegisterTraceWriter(uint32_t writer_id,
                                       uint32_t target_buffer) {
  producer_endpoint_->RegisterTraceWriter(writer_id, target_buffer);
}

void ProducerHost::UnregisterTraceWriter(uint32_t writer_id) {
  producer_endpoint_->UnregisterTraceWriter(writer_id);
}

}  // namespace tracing

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/producer_host.h"

#include <utility>

#include "base/bind.h"
#include "base/process/process.h"
#include "build/build_config.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/public/cpp/perfetto/producer_client.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "services/tracing/public/cpp/tracing_features.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/commit_data_request.h"
#include "third_party/perfetto/include/perfetto/tracing/core/data_source_descriptor.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"

namespace tracing {

// TODO(oysteine): Find a good compromise between performance and
// data granularity (mainly relevant to running with small buffer sizes
// when we use background tracing) on Android.
#if defined(OS_ANDROID)
constexpr size_t kSMBPageSizeInBytes = 4 * 1024;
#else
constexpr size_t kSMBPageSizeInBytes = 32 * 1024;
#endif

ProducerHost::ProducerHost() = default;

ProducerHost::~ProducerHost() {
  // Manually reset to prevent any callbacks from the ProducerEndpoint
  // when we're in a half-destructed state.
  producer_endpoint_.reset();
}

void ProducerHost::Initialize(
    mojo::PendingRemote<mojom::ProducerClient> producer_client,
    perfetto::TracingService* service,
    const std::string& name) {
  DCHECK(service);
  DCHECK(!producer_endpoint_);

  producer_client_.Bind(std::move(producer_client));

  // Attempt to parse the PID out of the producer name.
  // If the Producer is in-process, we:
  // * Signal Perfetto that it should create and manage its own
  // SharedMemoryArbiter
  //   when we call ConnectProducer.
  // * Set the local ProducerClient instance to use this SMA instead of passing
  //   an SMB handle over Mojo and letting it create its own.
  // * After that point, any TraceWriter created by TraceEventDataSource will
  //   use this in-process SMA, and hence be able to synchronously flush full
  //   SMB chunks if we're running on the same sequence as the Perfetto service,
  //   hence we avoid any deadlocking occurring from trace events emitted from
  //   the Perfetto sequence filling up the SMB and stalling while waiting for
  //   Perfetto to free the chunks.
  if (!base::FeatureList::IsEnabled(
          features::kPerfettoForceOutOfProcessProducer)) {
    base::ProcessId pid;
    if (PerfettoService::ParsePidFromProducerName(name, &pid)) {
      is_in_process_ = (pid == base::Process::Current().Pid());
    }
  }

  // TODO(oysteine): Figure out an uid once we need it.
  // TODO(oysteine): Figure out a good buffer size.
  producer_endpoint_ = service->ConnectProducer(
      this, 0 /* uid */, name,
      /*shared_memory_size_hint_bytes=*/4 * 1024 * 1024, is_in_process_,
      perfetto::TracingService::ProducerSMBScrapingMode::kDefault,
      /*shared_memory_page_size_hint_bytes=*/kSMBPageSizeInBytes);
  DCHECK(producer_endpoint_);
}

void ProducerHost::OnConnect() {
}

void ProducerHost::OnDisconnect() {
  // Deliberately empty, this is invoked by the |service_| business logic after
  // we destroy the |producer_endpoint|.
}

void ProducerHost::OnTracingSetup() {
  if (is_in_process_) {
    PerfettoTracedProcess::Get()
        ->producer_client()
        ->set_in_process_shmem_arbiter(
            producer_endpoint_->GetInProcessShmemArbiter());
    return;
  }

  MojoSharedMemory* shared_memory =
      static_cast<MojoSharedMemory*>(producer_endpoint_->shared_memory());
  DCHECK(shared_memory);
  DCHECK(producer_client_);
  mojo::ScopedSharedBufferHandle shm = shared_memory->Clone();
  DCHECK(shm.is_valid());

  producer_client_->OnTracingStart(
      std::move(shm), producer_endpoint_->shared_buffer_page_size_kb() * 1024);
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
    size_t num_data_sources) {
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

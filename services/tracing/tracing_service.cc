// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/tracing_service.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "build/build_config.h"
#include "services/tracing/perfetto/consumer_host.h"
#include "services/tracing/perfetto/perfetto_service.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"

namespace tracing {

namespace {

void OnProcessConnectFailed(PerfettoService* perfetto_service, uint32_t pid) {
  perfetto_service->RemoveActiveServicePidIfNoActiveConnections(pid);
}

}  // namespace

TracingService::TracingService(PerfettoService* perfetto_service)
    : perfetto_service_(perfetto_service ? perfetto_service
                                         : PerfettoService::GetInstance()) {}

TracingService::TracingService(
    mojo::PendingReceiver<mojom::TracingService> receiver)
    : receiver_(this, std::move(receiver)),
      perfetto_service_(PerfettoService::GetInstance()) {}

TracingService::~TracingService() = default;

void TracingService::Initialize(std::vector<mojom::ClientInfoPtr> clients) {
  for (auto& client : clients) {
    AddClient(std::move(client));
  }
  perfetto_service_->SetActiveServicePidsInitialized();
}

void TracingService::AddClient(mojom::ClientInfoPtr client) {
  mojo::Remote<mojom::TracedProcess> process(std::move(client->process));

  perfetto_service_->AddActiveServicePid(client->pid);

  // If the remote traced process goes away before ConnectToTracingService
  // responds, the PID should be removed from the list of active service PID.
  process.set_disconnect_handler(
      base::BindOnce(&OnProcessConnectFailed,
                     base::Unretained(perfetto_service_), client->pid));

  auto new_connection_request = mojom::ConnectToTracingRequest::New();
  perfetto_service_->BindReceiver(
      new_connection_request->perfetto_service.InitWithNewPipeAndPassReceiver(),
      client->pid);

  mojom::TracedProcess* raw_process = process.get();
  raw_process->ConnectToTracingService(
      std::move(new_connection_request),
      base::DoNothingWithBoundArgs(std::move(process)));
}

#if BUILDFLAG(USE_BLINK)
void TracingService::BindConsumerHost(
    mojo::PendingReceiver<mojom::ConsumerHost> receiver) {
  ConsumerHost::BindConsumerReceiver(perfetto_service_, std::move(receiver));
}
#endif

}  // namespace tracing

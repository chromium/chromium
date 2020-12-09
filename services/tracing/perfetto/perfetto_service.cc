// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/perfetto_service.h"

#include <utility>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "mojo/public/cpp/bindings/message.h"
#include "services/tracing/perfetto/consumer_host.h"
#include "services/tracing/perfetto/producer_host.h"
#include "services/tracing/public/cpp/perfetto/shared_memory.h"
#include "third_party/perfetto/include/perfetto/ext/tracing/core/tracing_service.h"

namespace tracing {

namespace {

bool StringToProcessId(const std::string& input, base::ProcessId* output) {
  // Pid is encoded as uint in the string.
  return base::StringToUint(input, reinterpret_cast<uint32_t*>(output));
}

}  // namespace

// static
bool PerfettoService::ParsePidFromProducerName(const std::string& producer_name,
                                               base::ProcessId* pid) {
  if (!base::StartsWith(producer_name, mojom::kPerfettoProducerNamePrefix,
                        base::CompareCase::SENSITIVE)) {
    LOG(DFATAL) << "Unexpected producer name: " << producer_name;
    return false;
  }

  static const size_t kPrefixLength =
      strlen(mojom::kPerfettoProducerNamePrefix);
  if (!StringToProcessId(producer_name.substr(kPrefixLength), pid)) {
    LOG(DFATAL) << "Unexpected producer name: " << producer_name;
    return false;
  }
  return true;
}

// static
PerfettoService* PerfettoService::GetInstance() {
  static base::NoDestructor<PerfettoService> perfetto_service;
  return perfetto_service.get();
}

PerfettoService::PerfettoService(
    scoped_refptr<base::SequencedTaskRunner> task_runner_for_testing)
    : perfetto_task_runner_(task_runner_for_testing
                                ? std::move(task_runner_for_testing)
                                : base::SequencedTaskRunnerHandle::Get()) {
  service_ = perfetto::TracingService::CreateInstance(
      std::make_unique<MojoSharedMemory::Factory>(), &perfetto_task_runner_);
  // Chromium uses scraping of the shared memory chunks to ensure that data
  // from threads without a MessageLoop doesn't get lost.
  service_->SetSMBScrapingEnabled(true);

  receivers_.set_disconnect_handler(base::BindRepeating(
      &PerfettoService::OnServiceDisconnect, base::Unretained(this)));
  producer_receivers_.set_disconnect_handler(base::BindRepeating(
      &PerfettoService::OnProducerHostDisconnect, base::Unretained(this)));
}

PerfettoService::~PerfettoService() = default;

perfetto::TracingService* PerfettoService::GetService() const {
  return service_.get();
}

void PerfettoService::BindReceiver(
    mojo::PendingReceiver<mojom::PerfettoService> receiver,
    uint32_t pid) {
  ++num_active_connections_[pid];
  receivers_.Add(this, std::move(receiver), pid);
}

void PerfettoService::ConnectToProducerHost(
    mojo::PendingRemote<mojom::ProducerClient> producer_client,
    mojo::PendingReceiver<mojom::ProducerHost> producer_host_receiver,
    mojo::ScopedSharedBufferHandle shared_memory,
    uint64_t shared_memory_buffer_page_size_bytes) {
  if (!shared_memory.is_valid()) {
    // Connection requests should always include an SMB.
    mojo::ReportBadMessage("Producer connection request without SMB");
    return;
  }

  auto new_producer = std::make_unique<ProducerHost>(&perfetto_task_runner_);
  uint32_t producer_pid = receivers_.current_context();
  DCHECK(shared_memory.is_valid());
  ProducerHost::InitializationResult result = new_producer->Initialize(
      std::move(producer_client), service_.get(),
      base::StrCat({mojom::kPerfettoProducerNamePrefix,
                    base::NumberToString(producer_pid)}),
      std::move(shared_memory), shared_memory_buffer_page_size_bytes);

  base::UmaHistogramEnumeration("Tracing.ProducerHostInitializationResult",
                                result);

  if (result == ProducerHost::InitializationResult::kSmbNotAdopted) {
    // When everything else succeeds, but the SMB was not accepted, the producer
    // must be misbehaving. SMBs are not accepted only if they are incorrectly
    // sized, but SMB/page sizes are constants in Chromium.
    mojo::ReportBadMessage("Producer connection request with invalid SMB");
    return;
  }

  if (result != ProducerHost::InitializationResult::kSuccess) {
    // In other failure scenarios, the tracing service may have encountered an
    // internal error not caused by a misbehaving producer, e.g. we have too
    // many producers registered or mapping the SMB failed (crbug/1154344). In
    // these cases, we have no choice but to ignore the failure and cancel the
    // producer connection by dropping |new_producer|.
    return;
  }

  ++num_active_connections_[producer_pid];
  producer_receivers_.Add(std::move(new_producer),
                          std::move(producer_host_receiver), producer_pid);
}

void PerfettoService::AddActiveServicePid(base::ProcessId pid) {
  active_service_pids_.insert(pid);
  for (auto* tracing_session : tracing_sessions_) {
    tracing_session->OnActiveServicePidAdded(pid);
  }
}

void PerfettoService::RemoveActiveServicePid(base::ProcessId pid) {
  active_service_pids_.erase(pid);
  num_active_connections_.erase(pid);
  for (auto* tracing_session : tracing_sessions_) {
    tracing_session->OnActiveServicePidRemoved(pid);
  }
}

void PerfettoService::RemoveActiveServicePidIfNoActiveConnections(
    base::ProcessId pid) {
  const auto num_connections_it = num_active_connections_.find(pid);
  if (num_connections_it == num_active_connections_.end() ||
      num_connections_it->second == 0) {
    RemoveActiveServicePid(pid);
  }
}

void PerfettoService::SetActiveServicePidsInitialized() {
  active_service_pids_initialized_ = true;
  for (auto* tracing_session : tracing_sessions_) {
    tracing_session->OnActiveServicePidsInitialized();
  }
}

void PerfettoService::RegisterTracingSession(
    ConsumerHost::TracingSession* tracing_session) {
  tracing_sessions_.insert(tracing_session);
}

void PerfettoService::UnregisterTracingSession(
    ConsumerHost::TracingSession* tracing_session) {
  tracing_sessions_.erase(tracing_session);
}

void PerfettoService::RequestTracingSession(
    mojom::TracingClientPriority priority,
    base::OnceClosure callback) {
  // TODO(oysteine): This currently assumes we only have one concurrent tracing
  // session, which is enforced by all ConsumerHost::BeginTracing calls routing
  // through RequestTracingSession before creating a new TracingSession.
  // Not running the callback means we'll drop any connection requests and deny
  // the creation of the tracing session.
  for (auto* tracing_session : tracing_sessions_) {
    if (!tracing_session->tracing_enabled()) {
      continue;
    }

    if (tracing_session->tracing_priority() > priority) {
      return;
    }

    // If the currently active session is the same or lower priority and it's
    // tracing, then we'll disable it and re-try the request once it's shut
    // down.
    tracing_session->RequestDisableTracing(
        base::BindOnce(&PerfettoService::RequestTracingSession,
                       base::Unretained(PerfettoService::GetInstance()),
                       priority, std::move(callback)));
    return;
  }

  std::move(callback).Run();
}

void PerfettoService::OnServiceDisconnect() {
  OnDisconnectFromProcess(receivers_.current_context());
}

void PerfettoService::OnProducerHostDisconnect() {
  OnDisconnectFromProcess(producer_receivers_.current_context());
}

void PerfettoService::OnDisconnectFromProcess(base::ProcessId pid) {
  int& num_connections = num_active_connections_[pid];
  DCHECK_GT(num_connections, 0);
  --num_connections;
  if (!num_connections)
    RemoveActiveServicePid(pid);
}

}  // namespace tracing

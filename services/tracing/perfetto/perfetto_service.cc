// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/perfetto/perfetto_service.h"

#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
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
  DCHECK(service_);
}

PerfettoService::~PerfettoService() = default;

perfetto::TracingService* PerfettoService::GetService() const {
  return service_.get();
}

void PerfettoService::BindReceiver(
    mojo::PendingReceiver<mojom::PerfettoService> receiver,
    uint32_t pid) {
  receivers_.Add(this, std::move(receiver), pid);
}

void PerfettoService::ConnectToProducerHost(
    mojo::PendingRemote<mojom::ProducerClient> producer_client,
    mojo::PendingReceiver<mojom::ProducerHost> producer_host_receiver) {
  auto new_producer = std::make_unique<ProducerHost>();
  uint32_t producer_pid = receivers_.current_context();
  new_producer->Initialize(std::move(producer_client), service_.get(),
                           base::StrCat({mojom::kPerfettoProducerNamePrefix,
                                         base::NumberToString(producer_pid)}));
  producer_receivers_.Add(std::move(new_producer),
                          std::move(producer_host_receiver));
}

void PerfettoService::AddActiveServicePid(base::ProcessId pid) {
  active_service_pids_.insert(pid);
  for (auto* tracing_session : tracing_sessions_) {
    tracing_session->OnActiveServicePidAdded(pid);
  }
}

void PerfettoService::RemoveActiveServicePid(base::ProcessId pid) {
  active_service_pids_.erase(pid);
  for (auto* tracing_session : tracing_sessions_) {
    tracing_session->OnActiveServicePidRemoved(pid);
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

}  // namespace tracing

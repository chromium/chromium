// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/memory_coordinator/browser_memory_coordinator_bridge.h"

#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

namespace content {

BrowserMemoryCoordinatorBridge::BrowserMemoryCoordinatorBridge(
    MemoryCoordinatorPolicyManager& manager)
    : MemoryCoordinatorPolicy(manager) {}

BrowserMemoryCoordinatorBridge::~BrowserMemoryCoordinatorBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BrowserMemoryCoordinatorBridge::OnConsumerGroupAdded(
    uint32_t consumer_id,
    std::string_view consumer_name,
    base::MemoryConsumerTraits traits,
    ProcessType process_type,
    ChildProcessId child_process_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto [it, inserted] = groups_.try_emplace(
      consumer_id, ConsumerDetails{std::string(consumer_name), traits});
  CHECK(inserted);

  // Buffer the registration and flush it, coalesced with any other pending
  // registrations, on the next loop iteration instead of sending one IPC now.
  // If `registry_host_` isn't connected yet, the registration simply waits in
  // `pending_registrations_` until BindAndPassReceiver() schedules a flush.
  pending_registrations_.insert(consumer_id);
  ScheduleRegistrationFlush();
}

void BrowserMemoryCoordinatorBridge::OnConsumerGroupRemoved(
    uint32_t consumer_id,
    ChildProcessId child_process_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t removed = groups_.erase(consumer_id);
  CHECK_EQ(removed, 1u);

  // If the registration is still pending, it was never sent to the browser, so
  // just drop it: the browser never learns this consumer existed.
  if (pending_registrations_.erase(consumer_id)) {
    return;
  }

  if (registry_host_) {
    registry_host_->Unregister(consumer_id);
  }
}

void BrowserMemoryCoordinatorBridge::UpdateConsumers(
    std::vector<MemoryConsumerUpdate> updates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Ignore updates for consumers whose unregistration is in flight to the
  // browser.
  std::erase_if(updates, [&](const auto& update) {
    return !groups_.contains(update.consumer_id);
  });
  manager().UpdateConsumers(this, std::move(updates));
}

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
void BrowserMemoryCoordinatorBridge::EnableDiagnosticsReporting(
    mojo::PendingRemote<mojom::MemoryCoordinatorDiagnosticsHost> host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!diagnostics_host_.is_bound());

  diagnostics_host_.Bind(std::move(host));
  // The use of Unretained is safe here because `this` owns the remote and will
  // always outlive it.
  diagnostics_host_.set_disconnect_handler(base::BindOnce(
      &BrowserMemoryCoordinatorBridge::OnReportingHostDisconnected,
      base::Unretained(this)));

  manager().AddDiagnosticObserver(this);
}

void BrowserMemoryCoordinatorBridge::OnMemoryLimitChanged(
    uint32_t consumer_id,
    ChildProcessId child_process_id,
    int memory_limit) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (diagnostics_host_) {
    diagnostics_host_->OnMemoryLimitChanged(consumer_id, memory_limit);
  }
}
#endif  // BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)

mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost>
BrowserMemoryCoordinatorBridge::BindAndPassReceiver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!registry_host_);

  auto pending_receiver = registry_host_.BindNewPipeAndPassReceiver();

  // Bind the process-level coordinator and send it to the host.
  registry_host_->BindCoordinator(receiver_.BindNewPipeAndPassRemote());

  // Flush, as a single batched IPC, the consumers that registered before the
  // host connected (they are already buffered in `pending_registrations_`).
  ScheduleRegistrationFlush();

  return pending_receiver;
}

void BrowserMemoryCoordinatorBridge::ScheduleRegistrationFlush() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!registry_host_ || flush_scheduled_ || pending_registrations_.empty()) {
    return;
  }
  flush_scheduled_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&BrowserMemoryCoordinatorBridge::FlushPendingRegistrations,
                     weak_factory_.GetWeakPtr()));
}

void BrowserMemoryCoordinatorBridge::FlushPendingRegistrations() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  flush_scheduled_ = false;
  if (pending_registrations_.empty()) {
    return;
  }

  std::vector<mojom::MemoryConsumerRegistrationPtr> registrations;
  registrations.reserve(pending_registrations_.size());
  for (uint32_t consumer_id : pending_registrations_) {
    const ConsumerDetails& details = groups_.at(consumer_id);
    registrations.push_back(mojom::MemoryConsumerRegistration::New(
        consumer_id, details.consumer_name, details.traits));
  }
  pending_registrations_.clear();

  // Records how many consumer registrations were coalesced into this batched
  // IPC. A value of 1 means no coalescing happened for this flush; larger
  // values quantify how effective the batching is in the field.
  base::UmaHistogramCounts100("Memory.Coordinator.RegistrationBatchSize",
                              static_cast<int>(registrations.size()));

  registry_host_->Register(std::move(registrations));
}

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
void BrowserMemoryCoordinatorBridge::OnReportingHostDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  diagnostics_host_.reset();
  manager().RemoveDiagnosticObserver(this);
}
#endif

}  // namespace content

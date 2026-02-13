// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/memory_coordinator/browser_memory_coordinator_bridge.h"

#include <utility>

#include "base/check_op.h"
#include "content/common/memory_coordinator/memory_coordinator_policy_manager.h"

namespace content {

BrowserMemoryCoordinatorBridge::BrowserMemoryCoordinatorBridge(
    MemoryCoordinatorPolicyManager& manager)
    : MemoryCoordinatorPolicy(manager) {}

BrowserMemoryCoordinatorBridge::~BrowserMemoryCoordinatorBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void BrowserMemoryCoordinatorBridge::OnConsumerGroupAdded(
    std::string_view consumer_id,
    base::MemoryConsumerTraits traits,
    ProcessType process_type,
    ChildProcessId child_process_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto [it, inserted] = groups_.try_emplace(consumer_id, traits);
  CHECK(inserted);

  if (registry_host_) {
    registry_host_->Register(std::string(consumer_id), traits);
  }
}

void BrowserMemoryCoordinatorBridge::OnConsumerGroupRemoved(
    std::string_view consumer_id,
    ChildProcessId child_process_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  size_t removed = groups_.erase(consumer_id);
  CHECK_EQ(removed, 1u);

  if (registry_host_) {
    registry_host_->Unregister(std::string(consumer_id));
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

mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost>
BrowserMemoryCoordinatorBridge::BindAndPassReceiver() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!registry_host_);

  auto pending_receiver = registry_host_.BindNewPipeAndPassReceiver();

  // Bind the process-level coordinator and send it to the host.
  registry_host_->BindCoordinator(receiver_.BindNewPipeAndPassRemote());

  // Notify the browser for consumers that registered early.
  for (auto const& [consumer_id, traits] : groups_) {
    registry_host_->Register(consumer_id, traits);
  }

  return pending_receiver;
}

}  // namespace content

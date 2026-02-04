// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/memory_coordinator/child_memory_consumer_registry.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "content/public/common/process_type.h"

namespace content {

// ChildMemoryConsumerRegistry::ConsumerGroup ----------------------------------

ChildMemoryConsumerRegistry::ConsumerGroup::ConsumerGroup(
    base::MemoryConsumerTraits traits)
    : traits_(traits) {}

ChildMemoryConsumerRegistry::ConsumerGroup::~ConsumerGroup() {
  CHECK(memory_consumers_.empty());
}

void ChildMemoryConsumerRegistry::ConsumerGroup::OnReleaseMemory() {
  for (base::RegisteredMemoryConsumer& consumer : memory_consumers_) {
    consumer.ReleaseMemory();
  }
}

void ChildMemoryConsumerRegistry::ConsumerGroup::OnUpdateMemoryLimit() {
  for (base::RegisteredMemoryConsumer& consumer : memory_consumers_) {
    consumer.UpdateMemoryLimit(memory_limit());
  }
}

void ChildMemoryConsumerRegistry::ConsumerGroup::AddMemoryConsumer(
    base::RegisteredMemoryConsumer consumer) {
  CHECK(!std::ranges::contains(memory_consumers_, consumer));
  memory_consumers_.push_back(consumer);

  // Ensure the added consumer is up to date with the current memory limit
  // applied to this consumer group.
  if (memory_limit() != base::MemoryConsumer::kDefaultMemoryLimit) {
    consumer.UpdateMemoryLimit(memory_limit());
  }
}

void ChildMemoryConsumerRegistry::ConsumerGroup::RemoveMemoryConsumer(
    base::RegisteredMemoryConsumer consumer) {
  size_t removed = std::erase(memory_consumers_, consumer);
  CHECK_EQ(removed, 1u);
}

// ChildMemoryConsumerRegistry -------------------------------------------------

ChildMemoryConsumerRegistry::ChildMemoryConsumerRegistry(
    MemoryConsumerGroupController& controller)
    : controller_(controller) {}

ChildMemoryConsumerRegistry::~ChildMemoryConsumerRegistry() {
  NotifyDestruction();

  CHECK(consumer_groups_.empty());
  CHECK(child_memory_consumers_.empty());
}

void ChildMemoryConsumerRegistry::NotifyReleaseMemory() {
  base::RegisteredMemoryConsumer consumer =
      child_memory_consumers_.current_context();
  consumer.ReleaseMemory();
}

void ChildMemoryConsumerRegistry::NotifyUpdateMemoryLimit(int percentage) {
  base::RegisteredMemoryConsumer consumer =
      child_memory_consumers_.current_context();
  consumer.UpdateMemoryLimit(percentage);
}

// static
mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost>
ChildMemoryConsumerRegistry::BindAndPassReceiver() {
  auto& child_registry = static_cast<ChildMemoryConsumerRegistry&>(
      base::MemoryConsumerRegistry::Get());
  return child_registry.BindAndPassReceiverImpl();
}

mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost>
ChildMemoryConsumerRegistry::BindAndPassReceiverForTesting() {
  return BindAndPassReceiverImpl();
}

void ChildMemoryConsumerRegistry::OnMemoryConsumerAdded(
    std::string_view consumer_id,
    base::MemoryConsumerTraits traits,
    base::RegisteredMemoryConsumer consumer) {
  auto [it, inserted] = consumer_groups_.try_emplace(std::string(consumer_id));
  if (inserted) {
    it->second = std::make_unique<ConsumerGroupAndReceiverId>(traits);
  }

  ConsumerGroup& consumer_group = it->second->consumer_group;

  if (inserted) {
    // First time seeing a consumer with this ID.

    if (registry_host_) {
      // Bind a new pipe to connect with the browser process.
      mojo::PendingRemote<mojom::ChildMemoryConsumer> remote;
      it->second->receiver_id = child_memory_consumers_.Add(
          this, remote.InitWithNewPipeAndPassReceiver(),
          CreateRegisteredMemoryConsumer(&consumer_group));

      // Notify the browser process.
      registry_host_->Register(std::string(consumer_id), traits,
                               std::move(remote));
    }

    controller_->OnConsumerGroupAdded(
        consumer_id, traits, PROCESS_TYPE_UNKNOWN, ChildProcessId(),
        CreateRegisteredMemoryConsumer(&consumer_group));
  }

  CHECK(consumer_group.traits() == traits);

  consumer_group.AddMemoryConsumer(consumer);
}

void ChildMemoryConsumerRegistry::OnMemoryConsumerRemoved(
    std::string_view consumer_id,
    base::RegisteredMemoryConsumer consumer) {
  auto it = consumer_groups_.find(consumer_id);
  CHECK(it != consumer_groups_.end());
  ConsumerGroup& consumer_group = it->second->consumer_group;
  std::optional<mojo::ReceiverId> receiver_id = it->second->receiver_id;

  consumer_group.RemoveMemoryConsumer(consumer);

  if (consumer_group.empty()) {
    // Last consumer with this ID. First remove the connection with the browser
    // process, if there ever was one.
    if (receiver_id) {
      child_memory_consumers_.Remove(*receiver_id);
    }

    controller_->OnConsumerGroupRemoved(consumer_id, ChildProcessId());

    // Also remove the group.
    consumer_groups_.erase(it);
  }
}

mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost>
ChildMemoryConsumerRegistry::BindAndPassReceiverImpl() {
  CHECK(!registry_host_);

  auto pending_receiver = registry_host_.BindNewPipeAndPassReceiver();

  // Notify the browser for consumers that registered early.
  for (auto& [consumer_id, entry] : consumer_groups_) {
    auto& [consumer_group, receiver_id] = *entry;

    // Bind a new pipe to connect with the browser process.
    mojo::PendingRemote<mojom::ChildMemoryConsumer> remote;
    receiver_id = child_memory_consumers_.Add(
        this, remote.InitWithNewPipeAndPassReceiver(),
        CreateRegisteredMemoryConsumer(&consumer_group));

    // Notify the browser process.
    registry_host_->Register(std::string(consumer_id), consumer_group.traits(),
                             std::move(remote));
  }

  return pending_receiver;
}

}  // namespace content

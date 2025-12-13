// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/memory_coordinator/child_memory_consumer_registry.h"

#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"

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
  CHECK(!base::Contains(memory_consumers_, consumer));
  memory_consumers_.push_back(consumer);
}

void ChildMemoryConsumerRegistry::ConsumerGroup::RemoveMemoryConsumer(
    base::RegisteredMemoryConsumer consumer) {
  size_t removed = std::erase(memory_consumers_, consumer);
  CHECK_EQ(removed, 1u);
}

// ChildMemoryConsumerRegistry -------------------------------------------------

ChildMemoryConsumerRegistry::ChildMemoryConsumerRegistry() = default;

ChildMemoryConsumerRegistry::~ChildMemoryConsumerRegistry() {
  NotifyDestruction();

  CHECK(consumer_groups_.empty());
  CHECK(child_memory_consumers_.empty());
  CHECK(consumer_infos_.empty());
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
mojo::PendingReceiver<mojom::BrowserMemoryConsumerRegistry>
ChildMemoryConsumerRegistry::BindAndPassReceiver() {
  auto& child_registry = static_cast<ChildMemoryConsumerRegistry&>(
      base::MemoryConsumerRegistry::Get());
  return child_registry.BindAndPassReceiverImpl();
}

mojo::PendingReceiver<mojom::BrowserMemoryConsumerRegistry>
ChildMemoryConsumerRegistry::BindAndPassReceiverForTesting() {
  return BindAndPassReceiverImpl();
}

void ChildMemoryConsumerRegistry::OnMemoryConsumerAdded(
    std::string_view consumer_id,
    base::MemoryConsumerTraits traits,
    base::RegisteredMemoryConsumer consumer) {
  auto [it, inserted] =
      consumer_groups_.try_emplace(std::string(consumer_id), traits);
  ConsumerGroup& consumer_group = it->second.consumer_group;

  if (inserted) {
    // First time seeing a consumer with this ID.

    if (browser_registry_) {
      // Bind a new pipe to connect with the browser process.
      mojo::PendingRemote<mojom::ChildMemoryConsumer> remote;
      it->second.receiver_id = child_memory_consumers_.Add(
          this, remote.InitWithNewPipeAndPassReceiver(),
          CreateRegisteredMemoryConsumer(&consumer_group));

      // Notify the browser process.
      browser_registry_->RegisterChildMemoryConsumer(std::string(consumer_id),
                                                     traits, std::move(remote));
    }

    // Add to `consumer_infos_` to facilitate iteration by external callers.
    consumer_infos_.emplace_back(
        std::string(consumer_id), traits,
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
  ConsumerGroup& consumer_group = it->second.consumer_group;
  std::optional<mojo::ReceiverId> receiver_id = it->second.receiver_id;

  consumer_group.RemoveMemoryConsumer(consumer);

  if (consumer_group.empty()) {
    // Last consumer with this ID. First remove the connection with the browser
    // process, if there ever was one.
    if (receiver_id) {
      child_memory_consumers_.Remove(*receiver_id);
    }

    // Then clean up from `consumer_infos_`.
    size_t removed = std::erase_if(
        consumer_infos_, [consumer_id](const ConsumerInfo& consumer_info) {
          return consumer_info.consumer_id == consumer_id;
        });
    CHECK_EQ(removed, 1u);

    // Also remove the group.
    consumer_groups_.erase(it);
  }
}

mojo::PendingReceiver<mojom::BrowserMemoryConsumerRegistry>
ChildMemoryConsumerRegistry::BindAndPassReceiverImpl() {
  CHECK(!browser_registry_);

  auto pending_receiver = browser_registry_.BindNewPipeAndPassReceiver();

  // Notify the browser for consumers that registered early.
  for (auto& [consumer_id, consumer_group_and_receiver_id] : consumer_groups_) {
    auto& [consumer_group, receiver_id] = consumer_group_and_receiver_id;

    // Bind a new pipe to connect with the browser process.
    mojo::PendingRemote<mojom::ChildMemoryConsumer> remote;
    receiver_id = child_memory_consumers_.Add(
        this, remote.InitWithNewPipeAndPassReceiver(),
        CreateRegisteredMemoryConsumer(&consumer_group));

    // Notify the browser process.
    browser_registry_->RegisterChildMemoryConsumer(
        std::string(consumer_id), consumer_group.traits(), std::move(remote));
  }

  return pending_receiver;
}

}  // namespace content

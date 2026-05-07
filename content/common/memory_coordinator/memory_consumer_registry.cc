// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/memory_coordinator/memory_consumer_registry.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "content/common/memory_coordinator/constants.h"

namespace content {

// MemoryConsumerRegistry::ConsumerGroup ---------------------------------------

MemoryConsumerRegistry::ConsumerGroup::ConsumerGroup(
    std::optional<base::MemoryConsumerTraits> traits,
    std::string_view consumer_name)
    : traits_(traits), consumer_name_(consumer_name) {}

MemoryConsumerRegistry::ConsumerGroup::~ConsumerGroup() {
  CHECK(memory_consumers_.empty());
}

void MemoryConsumerRegistry::ConsumerGroup::ReleaseMemory() {
  for (base::RegisteredMemoryConsumer& consumer : memory_consumers_) {
    consumer.ReleaseMemory();
  }
}

void MemoryConsumerRegistry::ConsumerGroup::UpdateMemoryLimit(int percentage) {
  memory_limit_ = percentage;
  for (base::RegisteredMemoryConsumer& consumer : memory_consumers_) {
    consumer.UpdateMemoryLimit(memory_limit_);
  }
}

void MemoryConsumerRegistry::ConsumerGroup::AddMemoryConsumer(
    base::RegisteredMemoryConsumer consumer) {
  CHECK(!std::ranges::contains(memory_consumers_, consumer));
  memory_consumers_.push_back(consumer);

  // Ensure the added consumer is up to date with the current memory limit
  // applied to this consumer group.
  if (memory_limit_ != base::MemoryConsumer::kDefaultMemoryLimit) {
    consumer.UpdateMemoryLimitNoNotification(memory_limit_);
  }
}

void MemoryConsumerRegistry::ConsumerGroup::RemoveMemoryConsumer(
    base::RegisteredMemoryConsumer consumer) {
  size_t removed = std::erase(memory_consumers_, consumer);
  CHECK_EQ(removed, 1u);
}

// MemoryConsumerRegistry ------------------------------------------------------

MemoryConsumerRegistry::MemoryConsumerRegistry(
    ProcessType process_type,
    ChildProcessId child_process_id,
    MemoryConsumerGroupController& controller)
    : process_type_(process_type),
      child_process_id_(child_process_id),
      controller_(controller) {
  controller_->AddMemoryConsumerGroupHost(child_process_id_, this);
}

MemoryConsumerRegistry::~MemoryConsumerRegistry() {
  NotifyDestruction();
  controller_->RemoveMemoryConsumerGroupHost(child_process_id_);
  CHECK(consumer_groups_.empty());
}

void MemoryConsumerRegistry::UpdateConsumers(
    std::vector<MemoryConsumerUpdate> updates) {
  for (const auto& update : updates) {
    auto it = consumer_groups_.find(update.consumer_id);
    CHECK(it != consumer_groups_.end());
    if (update.percentage) {
      it->second->UpdateMemoryLimit(*update.percentage);
    }
    if (update.release_memory) {
      it->second->ReleaseMemory();
    }
  }
}

void MemoryConsumerRegistry::OnMemoryConsumerAdded(
    uint32_t consumer_id,
    std::string_view consumer_name,
    std::optional<base::MemoryConsumerTraits> traits,
    base::RegisteredMemoryConsumer consumer) {
  CHECK_LE(consumer_name.size(), kMaxMemoryConsumerNameLength);

  auto [it, inserted] = consumer_groups_.try_emplace(consumer_id);
  std::unique_ptr<ConsumerGroup>& consumer_group = it->second;

  if (inserted) {
    CHECK_LE(consumer_groups_.size(), kMaxMemoryConsumersPerProcess);

    // First time seeing a consumer with this ID.
    consumer_group = std::make_unique<ConsumerGroup>(traits, consumer_name);

    controller_->OnConsumerGroupAdded(consumer_id, consumer_name, traits,
                                      process_type_, child_process_id_);
  }

  CHECK(consumer_group->traits() == traits);

  consumer_group->AddMemoryConsumer(consumer);
}

void MemoryConsumerRegistry::OnMemoryConsumerRemoved(
    uint32_t consumer_id,
    base::RegisteredMemoryConsumer consumer) {
  auto it = consumer_groups_.find(consumer_id);
  CHECK(it != consumer_groups_.end());
  ConsumerGroup& consumer_group = *it->second;

  consumer_group.RemoveMemoryConsumer(consumer);

  if (consumer_group.empty()) {
    // Last consumer with this ID.
    controller_->OnConsumerGroupRemoved(consumer_id, child_process_id_);

    // Also remove the group.
    consumer_groups_.erase(it);
  }
}

}  // namespace content

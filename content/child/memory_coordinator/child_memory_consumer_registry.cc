// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/memory_coordinator/child_memory_consumer_registry.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "content/public/common/child_process_id.h"
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
}

void ChildMemoryConsumerRegistry::OnMemoryConsumerAdded(
    std::string_view consumer_id,
    base::MemoryConsumerTraits traits,
    base::RegisteredMemoryConsumer consumer) {
  auto [it, inserted] = consumer_groups_.try_emplace(consumer_id);
  std::unique_ptr<ConsumerGroup>& consumer_group = it->second;

  if (inserted) {
    // First time seeing a consumer with this ID.
    consumer_group = std::make_unique<ConsumerGroup>(traits);

    controller_->OnConsumerGroupAdded(
        consumer_id, traits, PROCESS_TYPE_UNKNOWN, ChildProcessId(),
        CreateRegisteredMemoryConsumer(consumer_group.get()));
  }

  CHECK(consumer_group->traits() == traits);

  consumer_group->AddMemoryConsumer(consumer);
}

void ChildMemoryConsumerRegistry::OnMemoryConsumerRemoved(
    std::string_view consumer_id,
    base::RegisteredMemoryConsumer consumer) {
  auto it = consumer_groups_.find(consumer_id);
  CHECK(it != consumer_groups_.end());
  ConsumerGroup& consumer_group = *it->second;

  consumer_group.RemoveMemoryConsumer(consumer);

  if (consumer_group.empty()) {
    // Last consumer with this ID.
    controller_->OnConsumerGroupRemoved(consumer_id, ChildProcessId());

    // Also remove the group.
    consumer_groups_.erase(it);
  }
}

}  // namespace content

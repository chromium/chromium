// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/browser_memory_consumer_registry.h"

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"

namespace content {

// BrowserMemoryConsumerRegistry::ConsumerGroup --------------------------------

BrowserMemoryConsumerRegistry::ConsumerGroup::ConsumerGroup(
    base::MemoryConsumerTraits traits)
    : traits_(traits) {}

BrowserMemoryConsumerRegistry::ConsumerGroup::~ConsumerGroup() {
  CHECK(memory_consumers_.empty());
}

void BrowserMemoryConsumerRegistry::ConsumerGroup::ReleaseMemory() {
  for (base::RegisteredMemoryConsumer& consumer : memory_consumers_) {
    consumer.ReleaseMemory();
  }
}

void BrowserMemoryConsumerRegistry::ConsumerGroup::UpdateMemoryLimit(
    int percentage) {
  memory_limit_ = percentage;
  for (base::RegisteredMemoryConsumer& consumer : memory_consumers_) {
    consumer.UpdateMemoryLimit(memory_limit_);
  }
}

void BrowserMemoryConsumerRegistry::ConsumerGroup::AddMemoryConsumer(
    base::RegisteredMemoryConsumer consumer) {
  CHECK(!std::ranges::contains(memory_consumers_, consumer));
  memory_consumers_.push_back(consumer);

  // Ensure the added consumer is up to date with the current memory limit
  // applied to this consumer group.
  if (memory_limit_ != base::MemoryConsumer::kDefaultMemoryLimit) {
    consumer.UpdateMemoryLimit(memory_limit_);
  }
}

void BrowserMemoryConsumerRegistry::ConsumerGroup::RemoveMemoryConsumer(
    base::RegisteredMemoryConsumer consumer) {
  size_t removed = std::erase(memory_consumers_, consumer);
  CHECK_EQ(removed, 1u);
}

// BrowserMemoryConsumerRegistry -----------------------------------------------

BrowserMemoryConsumerRegistry::BrowserMemoryConsumerRegistry(
    MemoryConsumerGroupController& controller)
    : controller_(controller) {
  controller_->AddMemoryConsumerGroupHost(ChildProcessId(), this);
}

BrowserMemoryConsumerRegistry::~BrowserMemoryConsumerRegistry() {
  NotifyDestruction();
  controller_->RemoveMemoryConsumerGroupHost(ChildProcessId());
  CHECK(consumer_groups_.empty());
}

void BrowserMemoryConsumerRegistry::UpdateMemoryLimit(
    std::string_view consumer_id,
    int percentage) {
  auto it = consumer_groups_.find(consumer_id);
  CHECK(it != consumer_groups_.end());

  it->second->UpdateMemoryLimit(percentage);
}

void BrowserMemoryConsumerRegistry::ReleaseMemory(
    std::string_view consumer_id) {
  auto it = consumer_groups_.find(consumer_id);
  CHECK(it != consumer_groups_.end());

  it->second->ReleaseMemory();
}

void BrowserMemoryConsumerRegistry::OnMemoryConsumerAdded(
    std::string_view consumer_id,
    base::MemoryConsumerTraits traits,
    base::RegisteredMemoryConsumer consumer) {
  auto [it, inserted] = consumer_groups_.try_emplace(consumer_id);
  std::unique_ptr<ConsumerGroup>& consumer_group = it->second;

  if (inserted) {
    // First time seeing a consumer with this ID.
    consumer_group = std::make_unique<ConsumerGroup>(traits);

    controller_->OnConsumerGroupAdded(consumer_id, traits, PROCESS_TYPE_BROWSER,
                                      ChildProcessId());
  }

  CHECK(consumer_group->traits() == traits);

  consumer_group->AddMemoryConsumer(consumer);
}

void BrowserMemoryConsumerRegistry::OnMemoryConsumerRemoved(
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

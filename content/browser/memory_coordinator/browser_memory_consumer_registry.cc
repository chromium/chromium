// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/browser_memory_consumer_registry.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"

namespace content {

namespace {

BrowserMemoryConsumerRegistry& GetInstance() {
  auto& instance = static_cast<BrowserMemoryConsumerRegistry&>(
      base::MemoryConsumerRegistry::Get());
  return instance;
}

}  // namespace

// BrowserMemoryConsumerRegistry::ConsumerGroup --------------------------------

BrowserMemoryConsumerRegistry::ConsumerGroup::ConsumerGroup(
    base::MemoryConsumerTraits traits,
    ProcessType process_type)
    : traits_(traits), process_type_(process_type) {}

BrowserMemoryConsumerRegistry::ConsumerGroup::~ConsumerGroup() = default;

void BrowserMemoryConsumerRegistry::ConsumerGroup::OnReleaseMemory() {
  for (base::RegisteredMemoryConsumer& consumer : memory_consumers_) {
    consumer.ReleaseMemory();
  }
}

void BrowserMemoryConsumerRegistry::ConsumerGroup::OnUpdateMemoryLimit() {
  for (base::RegisteredMemoryConsumer& consumer : memory_consumers_) {
    consumer.UpdateMemoryLimit(memory_limit());
  }
}

void BrowserMemoryConsumerRegistry::ConsumerGroup::AddMemoryConsumer(
    base::RegisteredMemoryConsumer consumer) {
  CHECK(!std::ranges::contains(memory_consumers_, consumer));
  memory_consumers_.push_back(consumer);

  // Ensure the added consumer is up to date with the current memory limit
  // applied to this consumer group.
  if (memory_limit() != base::MemoryConsumer::kDefaultMemoryLimit) {
    consumer.UpdateMemoryLimit(memory_limit());
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
    : controller_(controller) {}

BrowserMemoryConsumerRegistry::~BrowserMemoryConsumerRegistry() {
  NotifyDestruction();

  // Clear all references to consumers that live in a child process, as it's not
  // worth the hassle to wait until all disconnect notifications are received.
  absl::erase_if(consumer_groups_, [](const auto& element) {
    return std::get<1>(element.first) != ChildProcessId();
  });

  // This checks that all local consumers have unregistered in time.
  CHECK(consumer_groups_.empty());
}

void BrowserMemoryConsumerRegistry::AddMemoryConsumerFromChildProcess(
    std::string_view consumer_id,
    base::MemoryConsumerTraits traits,
    ProcessType process_type,
    ChildProcessId child_process_id,
    base::MemoryConsumer* consumer) {
  CHECK_NE(process_type, PROCESS_TYPE_BROWSER);
  CHECK(!child_process_id.is_null());
  AddMemoryConsumerImpl(consumer_id, traits, process_type, child_process_id,
                        CreateRegisteredMemoryConsumer(consumer));
}

void BrowserMemoryConsumerRegistry::RemoveMemoryConsumerFromChildProcess(
    std::string_view consumer_id,
    ChildProcessId child_process_id,
    base::MemoryConsumer* consumer) {
  CHECK(!child_process_id.is_null());
  RemoveMemoryConsumerImpl(consumer_id, child_process_id,
                           CreateRegisteredMemoryConsumer(consumer));
}

void BrowserMemoryConsumerRegistry::NotifyReleaseMemoryForTesting() {
  auto& instance = GetInstance();
  for (auto& [key, group] : instance.consumer_groups_) {
    instance.CreateRegisteredMemoryConsumer(group.get()).ReleaseMemory();
  }
}

void BrowserMemoryConsumerRegistry::NotifyUpdateMemoryLimitForTesting(
    int percentage) {
  auto& instance = GetInstance();
  for (auto& [key, group] : instance.consumer_groups_) {
    instance.CreateRegisteredMemoryConsumer(group.get())
        .UpdateMemoryLimit(percentage);
  }
}

void BrowserMemoryConsumerRegistry::OnMemoryConsumerAdded(
    std::string_view consumer_id,
    base::MemoryConsumerTraits traits,
    base::RegisteredMemoryConsumer consumer) {
  AddMemoryConsumerImpl(consumer_id, traits, PROCESS_TYPE_BROWSER,
                        ChildProcessId(), consumer);
}

void BrowserMemoryConsumerRegistry::OnMemoryConsumerRemoved(
    std::string_view consumer_id,
    base::RegisteredMemoryConsumer consumer) {
  RemoveMemoryConsumerImpl(consumer_id, ChildProcessId(), consumer);
}

void BrowserMemoryConsumerRegistry::AddMemoryConsumerImpl(
    std::string_view consumer_id,
    base::MemoryConsumerTraits traits,
    ProcessType process_type,
    ChildProcessId child_process_id,
    base::RegisteredMemoryConsumer consumer) {
  auto [it, inserted] = consumer_groups_.try_emplace(
      ConsumerGroupKey(consumer_id, child_process_id), nullptr);

  if (inserted) {
    it->second = std::make_unique<ConsumerGroup>(traits, process_type);
  }

  ConsumerGroup& consumer_group = *it->second;

  if (inserted) {
    // First time seeing a consumer with this ID in this process. Notify the
    // controller.
    controller_->OnConsumerGroupAdded(
        consumer_id, traits, process_type, child_process_id,
        CreateRegisteredMemoryConsumer(&consumer_group));
  }

  CHECK(consumer_group.traits() == traits);
  CHECK_EQ(consumer_group.process_type(), process_type);

  consumer_group.AddMemoryConsumer(consumer);
}

void BrowserMemoryConsumerRegistry::RemoveMemoryConsumerImpl(
    std::string_view consumer_id,
    ChildProcessId child_process_id,
    base::RegisteredMemoryConsumer consumer) {
  auto it =
      consumer_groups_.find(ConsumerGroupKey(consumer_id, child_process_id));
  CHECK(it != consumer_groups_.end());
  ConsumerGroup& consumer_group = *it->second;

  consumer_group.RemoveMemoryConsumer(consumer);

  if (consumer_group.empty()) {
    // Last consumer with this ID. Notify the controller.
    controller_->OnConsumerGroupRemoved(consumer_id, child_process_id);

    // Also remove the group.
    consumer_groups_.erase(it);
  }
}

}  // namespace content

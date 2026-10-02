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
    base::MemoryConsumerTraits traits,
    std::string_view consumer_name)
    : traits_(traits), consumer_name_(consumer_name) {}

MemoryConsumerRegistry::ConsumerGroup::~ConsumerGroup() {
  CHECK(memory_consumers_.empty());
}

void MemoryConsumerRegistry::ConsumerGroup::ReleaseMemory() {
  for (base::MemoryConsumer& consumer : memory_consumers_) {
    base::MemoryConsumerRegistry::NotifyReleaseMemory(&consumer);
  }
}

void MemoryConsumerRegistry::ConsumerGroup::UpdateMemoryLimit(
    base::MemoryLimit memory_limit) {
  memory_limit_ = memory_limit;
  for (base::MemoryConsumer& consumer : memory_consumers_) {
    base::MemoryConsumerRegistry::NotifyUpdateMemoryLimit(&consumer,
                                                          memory_limit_);
  }
}

void MemoryConsumerRegistry::ConsumerGroup::AddMemoryConsumer(
    base::MemoryConsumer* consumer) {
  CHECK(!memory_consumers_.HasObserver(consumer));
  memory_consumers_.AddObserver(consumer);

  // Ensure the added consumer is up to date with the current memory limit
  // applied to this consumer group. The consumer's callback is suppressed
  // while it is being registered.
  if (memory_limit_ != base::MemoryLimit::Default()) {
    base::MemoryConsumerRegistry::NotifyUpdateMemoryLimit(consumer,
                                                          memory_limit_);
  }
}

void MemoryConsumerRegistry::ConsumerGroup::RemoveMemoryConsumer(
    base::MemoryConsumer* consumer) {
  CHECK(memory_consumers_.HasObserver(consumer));
  memory_consumers_.RemoveObserver(consumer);
}

// MemoryConsumerRegistry ------------------------------------------------------

MemoryConsumerRegistry::MemoryConsumerRegistry(
    ProcessType process_type,
    ChildProcessId child_process_id,
    MemoryConsumerGroupController& controller)
    : process_type_(process_type),
      child_process_id_(child_process_id),
      controller_(controller) {
  controller_->AddMemoryConsumerGroupHost(process_type_, child_process_id_,
                                          this);
}

MemoryConsumerRegistry::~MemoryConsumerRegistry() {
  NotifyDestruction();
  controller_->RemoveMemoryConsumerGroupHost(child_process_id_);
  CHECK(consumer_groups_.empty());
}

void MemoryConsumerRegistry::UpdateConsumers(
    std::vector<MemoryConsumerUpdate> updates) {
  // Re-entrant updates are allowed for groups that are not part of a batch
  // already in flight, e.g. to give a newly registered group its initial
  // limit while another group is releasing memory. Updating a group that is
  // already in flight would corrupt the iteration over its consumers.
  for (const auto& update : updates) {
    CHECK(!IsUpdating(update.consumer_id))
        << "Re-entrant UpdateConsumers() for consumer group '"
        << consumer_groups_.at(update.consumer_id)->consumer_name() << "' (id "
        << update.consumer_id
        << ") while it is already being updated. A group may not be updated "
           "again until the batch that contains it completes.";
  }

  // While this batch is in flight, its groups are kept alive even if their
  // last consumer unregisters (see OnMemoryConsumerRemoved()).
  in_flight_updates_.push_back(updates);
  for (const auto& update : updates) {
    // Look up by key each time: re-entrant registrations may rehash
    // `consumer_groups_`. The group object itself is stable.
    auto it = consumer_groups_.find(update.consumer_id);
    CHECK(it != consumer_groups_.end())
        << "UpdateConsumers() for unknown consumer group id "
        << update.consumer_id;
    ConsumerGroup& consumer_group = *it->second;
    if (update.memory_limit) {
      consumer_group.UpdateMemoryLimit(*update.memory_limit);
    }
    if (update.release_memory) {
      consumer_group.ReleaseMemory();
    }
  }
  in_flight_updates_.pop_back();

  // Empty groups are normally erased immediately in OnMemoryConsumerRemoved(),
  // so an empty group here means its last consumer unregistered during this
  // batch and none re-registered since.
  for (const auto& update : updates) {
    auto it = consumer_groups_.find(update.consumer_id);
    if (it != consumer_groups_.end() && it->second->empty()) {
      controller_->OnConsumerGroupRemoved(update.consumer_id,
                                          child_process_id_);
      consumer_groups_.erase(it);
    }
  }
}

void MemoryConsumerRegistry::SetOverrideLimit(uint32_t consumer_id,
                                              base::MemoryLimit memory_limit) {
  if (consumer_groups_.contains(consumer_id)) {
    UpdateConsumers({{consumer_id, memory_limit, false}});
  }
}

void MemoryConsumerRegistry::ClearOverrideLimit(
    uint32_t consumer_id,
    base::MemoryLimit policy_limit) {
  if (consumer_groups_.contains(consumer_id)) {
    UpdateConsumers({{consumer_id, policy_limit, false}});
  }
}

void MemoryConsumerRegistry::OnMemoryConsumerAdded(
    uint32_t consumer_id,
    std::string_view consumer_name,
    base::MemoryConsumerTraits traits,
    base::MemoryConsumer* consumer) {
  CHECK_LE(consumer_name.size(), kMaxMemoryConsumerNameLength);

  auto [it, inserted] = consumer_groups_.try_emplace(consumer_id);
  std::unique_ptr<ConsumerGroup>& consumer_group = it->second;

  if (inserted) {
    CHECK_LE(consumer_groups_.size(), kMaxMemoryConsumersPerProcess);

    // First time seeing a consumer with this ID.
    consumer_group = std::make_unique<ConsumerGroup>(traits, consumer_name);
  }

  CHECK(consumer_group->traits() == traits);

  // Add the consumer before notifying the controller so that the group is
  // never observed empty. The controller may synchronously push an initial
  // limit through UpdateConsumers(); the consumer's callbacks are suppressed
  // while it is being registered, so only its memory_limit() is updated.
  consumer_group->AddMemoryConsumer(consumer);

  if (inserted) {
    // `it` may be invalidated by re-entrant registrations from here on.
    controller_->OnConsumerGroupAdded(consumer_id, consumer_name, traits,
                                      child_process_id_);
  }
}

void MemoryConsumerRegistry::OnMemoryConsumerRemoved(
    uint32_t consumer_id,
    base::MemoryConsumer* consumer) {
  auto it = consumer_groups_.find(consumer_id);
  CHECK(it != consumer_groups_.end());
  ConsumerGroup& consumer_group = *it->second;

  consumer_group.RemoveMemoryConsumer(consumer);

  // If the group is part of an in-flight UpdateConsumers() batch, leave it
  // alive: it may be iterating over its consumers right now. UpdateConsumers()
  // erases it once the batch completes if it is still empty.
  if (consumer_group.empty() && !IsUpdating(consumer_id)) {
    // Last consumer with this ID.
    controller_->OnConsumerGroupRemoved(consumer_id, child_process_id_);

    // Also remove the group.
    consumer_groups_.erase(it);
  }
}

bool MemoryConsumerRegistry::IsUpdating(uint32_t consumer_id) const {
  // `in_flight_updates_` is empty outside of UpdateConsumers() and holds only
  // a handful of small batches otherwise, so a linear scan is appropriate.
  return std::ranges::any_of(in_flight_updates_, [&](const auto& updates) {
    return std::ranges::any_of(updates, [&](const auto& update) {
      return update.consumer_id == consumer_id;
    });
  });
}

}  // namespace content

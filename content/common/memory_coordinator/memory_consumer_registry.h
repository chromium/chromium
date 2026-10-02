// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_CONSUMER_REGISTRY_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_CONSUMER_REGISTRY_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory/raw_span.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_consumer_registry.h"
#include "base/memory_coordinator/traits.h"
#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_consumer_group_controller.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {

// An implementation of MemoryConsumerRegistry that groups consumers with the
// same ID and registers the group with a MemoryConsumerGroupController.
class CONTENT_EXPORT MemoryConsumerRegistry
    : public base::MemoryConsumerRegistry,
      public MemoryConsumerGroupHost {
 public:
  MemoryConsumerRegistry(ProcessType process_type,
                         ChildProcessId child_process_id,
                         MemoryConsumerGroupController& controller);
  ~MemoryConsumerRegistry() override;

  // MemoryConsumerGroupHost:
  void UpdateConsumers(std::vector<MemoryConsumerUpdate> updates) override;
  void SetOverrideLimit(uint32_t consumer_id,
                        base::MemoryLimit memory_limit) override;
  void ClearOverrideLimit(uint32_t consumer_id,
                          base::MemoryLimit policy_limit) override;

  // Returns the number of consumers with different IDs.
  size_t size() const { return consumer_groups_.size(); }

 private:
  // Groups all consumers with the same consumer ID to ensure they are treated
  // identically.
  class ConsumerGroup {
   public:
    explicit ConsumerGroup(base::MemoryConsumerTraits traits,
                           std::string_view consumer_name);

    ~ConsumerGroup();

    void ReleaseMemory();
    void UpdateMemoryLimit(base::MemoryLimit memory_limit);

    // Adds/removes a consumer.
    void AddMemoryConsumer(base::MemoryConsumer* consumer);
    void RemoveMemoryConsumer(base::MemoryConsumer* consumer);

    const std::string& consumer_name() const { return consumer_name_; }

    bool empty() const { return memory_consumers_.empty(); }

    base::MemoryConsumerTraits traits() const { return traits_; }

   private:
    base::MemoryConsumerTraits traits_;

    base::MemoryLimit memory_limit_ = base::MemoryLimit::Default();

    // Consumers added during an iteration are not notified by it: they already
    // received the current limit when they registered.
    base::ObserverList<base::MemoryConsumer> memory_consumers_{
        base::ObserverListPolicy::EXISTING_ONLY};
    std::string consumer_name_;
  };

  // base::MemoryConsumerRegistry:
  void OnMemoryConsumerAdded(uint32_t consumer_id,
                             std::string_view consumer_name,
                             base::MemoryConsumerTraits traits,
                             base::MemoryConsumer* consumer) override;
  void OnMemoryConsumerRemoved(uint32_t consumer_id,
                               base::MemoryConsumer* consumer) override;

  // Returns true if the group with `consumer_id` is part of a batch currently
  // being applied by UpdateConsumers().
  bool IsUpdating(uint32_t consumer_id) const;

  const ProcessType process_type_;
  const ChildProcessId child_process_id_;
  const raw_ref<MemoryConsumerGroupController> controller_;

  // Contains groups of all MemoryConsumers with the same consumer ID.
  absl::flat_hash_map<uint32_t, std::unique_ptr<ConsumerGroup>>
      consumer_groups_;

  // The batches currently being applied by UpdateConsumers(), outermost first.
  // Each span refers to the `updates` argument of a live UpdateConsumers()
  // frame. A group that appears in any of these must not be destroyed (its
  // consumers may be iterated over) and must not be updated re-entrantly.
  std::vector<base::raw_span<const MemoryConsumerUpdate>> in_flight_updates_;
};

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_CONSUMER_REGISTRY_H_

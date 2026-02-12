// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_COORDINATOR_BROWSER_MEMORY_CONSUMER_REGISTRY_H_
#define CONTENT_BROWSER_MEMORY_COORDINATOR_BROWSER_MEMORY_CONSUMER_REGISTRY_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ref.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_consumer_registry.h"
#include "base/memory_coordinator/traits.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_consumer_group_controller.h"
#include "content/common/memory_coordinator/memory_consumer_group_host.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {

// The MemoryConsumerRegistry implementation that lives in the browser process.
// It groups consumers with the same ID and registers the group with a
// MemoryConsumerGroupController. Note that this class only holds locally
// registered memory consumers.
class CONTENT_EXPORT BrowserMemoryConsumerRegistry
    : public base::MemoryConsumerRegistry,
      public MemoryConsumerGroupHost {
 public:
  explicit BrowserMemoryConsumerRegistry(
      MemoryConsumerGroupController& controller);
  ~BrowserMemoryConsumerRegistry() override;

  // MemoryConsumerGroupHost:
  void UpdateMemoryLimit(std::string_view consumer_id, int percentage) override;
  void ReleaseMemory(std::string_view consumer_id) override;

  // Returns the number of consumers with different IDs.
  size_t size() const { return consumer_groups_.size(); }

 private:
  // Groups all consumers with the same consumer ID to ensure they are treated
  // identically.
  class ConsumerGroup {
   public:
    explicit ConsumerGroup(base::MemoryConsumerTraits traits);

    ~ConsumerGroup();

    void ReleaseMemory();
    void UpdateMemoryLimit(int percentage);

    // Adds/removes a consumer.
    void AddMemoryConsumer(base::RegisteredMemoryConsumer consumer);
    void RemoveMemoryConsumer(base::RegisteredMemoryConsumer consumer);

    bool empty() const { return memory_consumers_.empty(); }

    base::MemoryConsumerTraits traits() const { return traits_; }

   private:
    base::MemoryConsumerTraits traits_;

    int memory_limit_ = base::MemoryConsumer::kDefaultMemoryLimit;

    std::vector<base::RegisteredMemoryConsumer> memory_consumers_;
  };

  // base::MemoryConsumerRegistry:
  void OnMemoryConsumerAdded(std::string_view consumer_id,
                             base::MemoryConsumerTraits traits,
                             base::RegisteredMemoryConsumer consumer) override;
  void OnMemoryConsumerRemoved(
      std::string_view consumer_id,
      base::RegisteredMemoryConsumer consumer) override;

  const raw_ref<MemoryConsumerGroupController> controller_;

  // Contains groups of all MemoryConsumers with the same consumer ID.
  absl::flat_hash_map<std::string, std::unique_ptr<ConsumerGroup>>
      consumer_groups_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_COORDINATOR_BROWSER_MEMORY_CONSUMER_REGISTRY_H_

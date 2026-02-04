// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_MEMORY_COORDINATOR_CHILD_MEMORY_CONSUMER_REGISTRY_H_
#define CONTENT_CHILD_MEMORY_COORDINATOR_CHILD_MEMORY_CONSUMER_REGISTRY_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_consumer_registry.h"
#include "base/memory_coordinator/traits.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_consumer_group_controller.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {

// This is an implementation of the MemoryConsumerRegistry meant to live in a
// child process. Consumers added to this registry are seamlessly registered
// with the browser's registry through the
// mojom::ChildMemoryConsumerRegistryHost interface.
class CONTENT_EXPORT ChildMemoryConsumerRegistry
    : public base::MemoryConsumerRegistry,
      public mojom::ChildMemoryConsumer {
 public:
  explicit ChildMemoryConsumerRegistry(
      MemoryConsumerGroupController& controller);
  ~ChildMemoryConsumerRegistry() override;

  // mojom::ChildMemoryConsumer:
  void NotifyReleaseMemory() override;
  void NotifyUpdateMemoryLimit(int percentage) override;

  // Returns the number of consumers with different IDs.
  size_t size() const { return consumer_groups_.size(); }

  // Allows connecting this process's global instance with the browser process.
  static mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost>
  BindAndPassReceiver();

  // Non-static version of the above, allowing to connect the browser and child
  // registries if the caller has a pointer to the registry.
  mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost>
  BindAndPassReceiverForTesting();

 private:
  // An implementation of MemoryConsumer that groups all consumers with the same
  // consumer ID to ensure they are treated identically.
  class ConsumerGroup : public base::MemoryConsumer {
   public:
    explicit ConsumerGroup(base::MemoryConsumerTraits traits);

    ~ConsumerGroup() override;

    // base::MemoryConsumer:
    void OnReleaseMemory() override;
    void OnUpdateMemoryLimit() override;

    // Adds/removes a consumer.
    void AddMemoryConsumer(base::RegisteredMemoryConsumer consumer);
    void RemoveMemoryConsumer(base::RegisteredMemoryConsumer consumer);

    bool empty() const { return memory_consumers_.empty(); }

    base::MemoryConsumerTraits traits() const { return traits_; }

   private:
    base::MemoryConsumerTraits traits_;

    std::vector<base::RegisteredMemoryConsumer> memory_consumers_;
  };

  // base::MemoryConsumerRegistry:
  void OnMemoryConsumerAdded(std::string_view consumer_id,
                             base::MemoryConsumerTraits traits,
                             base::RegisteredMemoryConsumer consumer) override;
  void OnMemoryConsumerRemoved(
      std::string_view consumer_id,
      base::RegisteredMemoryConsumer consumer) override;

  mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost>
  BindAndPassReceiverImpl();

  // Used to register consumers in the child process with the browser process.
  mojo::Remote<mojom::ChildMemoryConsumerRegistryHost> registry_host_;

  const raw_ref<MemoryConsumerGroupController> controller_;

  // Contains groups of all MemoryConsumers with the same consumer ID, and their
  // associated mojo::ReceiverId.
  struct ConsumerGroupAndReceiverId {
    template <class... Args>
    explicit ConsumerGroupAndReceiverId(Args&&... args)
        : consumer_group(std::forward<Args>(args)...) {}

    ConsumerGroup consumer_group;
    std::optional<mojo::ReceiverId> receiver_id;
  };
  absl::flat_hash_map<std::string, std::unique_ptr<ConsumerGroupAndReceiverId>>
      consumer_groups_;

  // For each ConsumerGroup, a mojom::ChildMemoryConsumer connection with the
  // browser process exists and is bound in this ReceiverSet.
  mojo::ReceiverSet<mojom::ChildMemoryConsumer, base::RegisteredMemoryConsumer>
      child_memory_consumers_;
};

}  // namespace content

#endif  // CONTENT_CHILD_MEMORY_COORDINATOR_CHILD_MEMORY_CONSUMER_REGISTRY_H_

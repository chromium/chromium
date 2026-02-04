// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_COORDINATOR_BROWSER_MEMORY_CONSUMER_REGISTRY_H_
#define CONTENT_BROWSER_MEMORY_COORDINATOR_BROWSER_MEMORY_CONSUMER_REGISTRY_H_

#include "base/memory/raw_ref.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_consumer_registry.h"
#include "base/memory_coordinator/traits.h"
#include "content/browser/memory_coordinator/child_memory_consumer_registry_host.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_consumer_group_controller.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/process_type.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {

// The MemoryConsumerRegistry implementation that lives in the browser process.
// This implementation acts as the main registry, as all the registries in child
// processes automatically register their MemoryConsumers with this one. This
// allows policies in the browser process to affect memory usage in child
// processes.
class CONTENT_EXPORT BrowserMemoryConsumerRegistry
    : public base::MemoryConsumerRegistry,
      public ChildMemoryConsumerRegistryHost::Delegate {
 public:
  explicit BrowserMemoryConsumerRegistry(
      MemoryConsumerGroupController& controller);
  ~BrowserMemoryConsumerRegistry() override;

  // ChildMemoryConsumerRegistryHost::Delegate:
  void AddMemoryConsumerFromChildProcess(
      std::string_view consumer_id,
      base::MemoryConsumerTraits traits,
      ProcessType process_type,
      ChildProcessId child_process_id,
      base::MemoryConsumer* consumer) override;
  void RemoveMemoryConsumerFromChildProcess(
      std::string_view consumer_id,
      ChildProcessId child_process_id,
      base::MemoryConsumer* consumer) override;

  static void NotifyReleaseMemoryForTesting();
  static void NotifyUpdateMemoryLimitForTesting(int percentage);

 private:
  // An implementation of MemoryConsumer that groups all consumers with the same
  // consumer ID and process ID to ensure they are treated identically.
  class ConsumerGroup : public base::MemoryConsumer {
   public:
    ConsumerGroup(base::MemoryConsumerTraits traits, ProcessType process_type);
    ~ConsumerGroup() override;

    // base::MemoryConsumer:
    void OnReleaseMemory() override;
    void OnUpdateMemoryLimit() override;

    // Adds/removes a consumer.
    void AddMemoryConsumer(base::RegisteredMemoryConsumer consumer);
    void RemoveMemoryConsumer(base::RegisteredMemoryConsumer consumer);

    bool empty() const { return memory_consumers_.empty(); }

    base::MemoryConsumerTraits traits() const { return traits_; }
    ProcessType process_type() const { return process_type_; }

   private:
    base::MemoryConsumerTraits traits_;
    ProcessType process_type_;

    std::vector<base::RegisteredMemoryConsumer> memory_consumers_;
  };

  // base::MemoryConsumerRegistry:
  void OnMemoryConsumerAdded(std::string_view consumer_id,
                             base::MemoryConsumerTraits traits,
                             base::RegisteredMemoryConsumer consumer) override;
  void OnMemoryConsumerRemoved(
      std::string_view consumer_id,
      base::RegisteredMemoryConsumer consumer) override;

  // Adds/removes a base::RegisteredMemoryConsumer. This consumer could be a
  // local consumer that lives in the browser process, or a consumer that lives
  // in a child process.
  void AddMemoryConsumerImpl(std::string_view consumer_id,
                             base::MemoryConsumerTraits traits,
                             ProcessType process_type,
                             ChildProcessId child_process_id,
                             base::RegisteredMemoryConsumer consumer);
  void RemoveMemoryConsumerImpl(std::string_view consumer_id,
                                ChildProcessId child_process_id,
                                base::RegisteredMemoryConsumer consumer);

  using ConsumerGroupKey = std::tuple<std::string, ChildProcessId>;

  // Contains groups of all MemoryConsumers with the same consumer ID and
  // process ID.
  absl::flat_hash_map<ConsumerGroupKey, std::unique_ptr<ConsumerGroup>>
      consumer_groups_;

  const raw_ref<MemoryConsumerGroupController> controller_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_COORDINATOR_BROWSER_MEMORY_CONSUMER_REGISTRY_H_

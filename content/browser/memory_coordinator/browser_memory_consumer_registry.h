// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MEMORY_COORDINATOR_BROWSER_MEMORY_CONSUMER_REGISTRY_H_
#define CONTENT_BROWSER_MEMORY_COORDINATOR_BROWSER_MEMORY_CONSUMER_REGISTRY_H_

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_consumer_registry.h"
#include "base/memory_coordinator/traits.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/mojom/memory_coordinator.mojom.h"
#include "content/public/browser/child_process_id.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace content {

// Binds `pending_receiver` with the global BrowserMemoryConsumerRegistry.
void CONTENT_EXPORT BindBrowserMemoryConsumerRegistry(
    ProcessType process_type,
    ChildProcessId child_process_id,
    mojo::PendingReceiver<mojom::BrowserMemoryConsumerRegistry>
        pending_receiver);

// The MemoryConsumerRegistry implementation that lives in the browser process.
// This implementation acts as the main registry, as all the registries in child
// processes automatically register their MemoryConsumers with this one. This
// allows policies in the browser process to affect memory usage in child
// processes.
class CONTENT_EXPORT BrowserMemoryConsumerRegistry
    : public base::MemoryConsumerRegistry,
      public mojom::BrowserMemoryConsumerRegistry {
 public:
  BrowserMemoryConsumerRegistry();
  ~BrowserMemoryConsumerRegistry() override;

  // mojom::BrowserMemoryConsumerRegistry:
  void RegisterChildMemoryConsumer(
      const std::string& consumer_id,
      base::MemoryConsumerTraits traits,
      mojo::PendingRemote<mojom::ChildMemoryConsumer> remote_consumer) override;

  // Binds `pending_receiver` to `this`.
  void Bind(ProcessType process_type,
            ChildProcessId child_process_id,
            mojo::PendingReceiver<mojom::BrowserMemoryConsumerRegistry>
                pending_receiver);

  // Details about a group of consumers of the same type.
  struct ConsumerInfo {
    ConsumerInfo(std::string consumer_id,
                 base::MemoryConsumerTraits traits,
                 ProcessType process_type,
                 ChildProcessId child_process_id,
                 base::RegisteredMemoryConsumer consumer);

    ConsumerInfo(ConsumerInfo&&);
    ConsumerInfo& operator=(ConsumerInfo&&);

    // An ID that uniquely identify the group.
    std::string consumer_id;
    // The traits of the consumer group. See "base/memory_coordinator/traits.h"
    // for a description of all possible traits.
    base::MemoryConsumerTraits traits;
    // The type of the process hosting the consumers of the group.
    ProcessType process_type;
    // The ID of the process hosting the consumers of the group.
    ChildProcessId child_process_id;
    // The interface to notify this consumer group.
    base::RegisteredMemoryConsumer consumer;
  };
  using value_type = ConsumerInfo;

  using iterator = std::vector<ConsumerInfo>::iterator;
  using const_iterator = std::vector<ConsumerInfo>::const_iterator;

  iterator begin() { return consumer_infos_.begin(); }
  iterator end() { return consumer_infos_.end(); }
  const_iterator begin() const { return consumer_infos_.begin(); }
  const_iterator end() const { return consumer_infos_.end(); }
  size_t size() const { return consumer_infos_.size(); }

 private:
  // An implementation of base::MemoryConsumer that encapsulates a connection to
  // to a mojom::ChildMemoryConsumer in a child process. This enables uniform
  // handling of local and remote MemoryConsumers.
  class ChildMemoryConsumer : public base::MemoryConsumer {
   public:
    ChildMemoryConsumer(
        mojo::PendingRemote<mojom::ChildMemoryConsumer> remote_consumer,
        base::OnceCallback<void(ChildMemoryConsumer*)> on_disconnect_handler);
    ~ChildMemoryConsumer() override;

    // base::MemoryConsumer:
    void OnReleaseMemory() override;
    void OnUpdateMemoryLimit() override;

   private:
    mojo::Remote<mojom::ChildMemoryConsumer> remote_consumer_;
  };

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

  void OnChildMemoryConsumerDisconnected(
      const std::string& consumer_id,
      ChildProcessId child_process_id,
      ChildMemoryConsumer* child_memory_consumer);

  struct ChildRegistryContext {
    ProcessType process_type;
    ChildProcessId child_process_id;
  };
  mojo::ReceiverSet<mojom::BrowserMemoryConsumerRegistry, ChildRegistryContext>
      receivers_;

  using ConsumerGroupKey = std::tuple<std::string, ChildProcessId>;

  // Holds a ChildMemoryConsumer for each consumer group that lives in a child
  // process.
  std::map<ConsumerGroupKey, ChildMemoryConsumer, std::less<>>
      child_memory_consumers_;

  // Contains groups of all MemoryConsumers with the same consumer ID and
  // process ID.
  std::map<ConsumerGroupKey, ConsumerGroup, std::less<>> consumer_groups_;

  // For each ConsumerGroup, this holds a corresponding ConsumerInfo entry. This
  // exists to facilitate iteration over existing MemoryConsumers.
  std::vector<ConsumerInfo> consumer_infos_;
};

namespace test {

void CONTENT_EXPORT NotifyReleaseMemoryForTesting();
void CONTENT_EXPORT NotifyUpdateMemoryLimitForTesting(int percentage);

}  // namespace test

}  // namespace content

#endif  // CONTENT_BROWSER_MEMORY_COORDINATOR_BROWSER_MEMORY_CONSUMER_REGISTRY_H_

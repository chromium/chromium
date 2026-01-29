// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/child_memory_consumer_registry_host.h"

#include <utility>

#include "base/functional/bind.h"
#include "content/browser/memory_coordinator/browser_memory_consumer_registry.h"

namespace content {

// ChildMemoryConsumerRegistryHost::ChildMemoryConsumer ------------------------

ChildMemoryConsumerRegistryHost::ChildMemoryConsumer::ChildMemoryConsumer(
    mojo::PendingRemote<mojom::ChildMemoryConsumer> remote_consumer,
    base::OnceCallback<void(ChildMemoryConsumer*)> on_disconnect_handler)
    : remote_consumer_(std::move(remote_consumer)) {
  remote_consumer_.set_disconnect_handler(
      base::BindOnce(std::move(on_disconnect_handler), this));
}

ChildMemoryConsumerRegistryHost::ChildMemoryConsumer::~ChildMemoryConsumer() =
    default;

void ChildMemoryConsumerRegistryHost::ChildMemoryConsumer::OnReleaseMemory() {
  remote_consumer_->NotifyReleaseMemory();
}

void ChildMemoryConsumerRegistryHost::ChildMemoryConsumer::
    OnUpdateMemoryLimit() {
  remote_consumer_->NotifyUpdateMemoryLimit(memory_limit());
}

// ChildMemoryConsumerRegistryHost ---------------------------------------------

ChildMemoryConsumerRegistryHost::ChildMemoryConsumerRegistryHost(
    Delegate& delegate)
    : delegate_(delegate) {}

ChildMemoryConsumerRegistryHost::~ChildMemoryConsumerRegistryHost() {
  for (auto& [key, child_memory_consumer] : child_memory_consumers_) {
    delegate_->RemoveMemoryConsumerFromChildProcess(
        key.first, key.second, child_memory_consumer.get());
  }
}

void ChildMemoryConsumerRegistryHost::Bind(
    ProcessType process_type,
    ChildProcessId child_process_id,
    mojo::PendingReceiver<mojom::ChildMemoryConsumerRegistryHost> receiver) {
  receivers_.Add(this, std::move(receiver),
                 ConnectionContext{process_type, child_process_id});
}

void ChildMemoryConsumerRegistryHost::Register(
    const std::string& consumer_id,
    base::MemoryConsumerTraits traits,
    mojo::PendingRemote<mojom::ChildMemoryConsumer> remote_consumer) {
  const ConnectionContext& context = receivers_.current_context();

  // In some edge cases related to RPH reuse, there might already be a
  // registered consumer group for this consumer_id. Simply overwrite it.
  auto [it, inserted] = child_memory_consumers_.try_emplace(
      ConsumerKey(consumer_id, context.child_process_id));
  std::unique_ptr<ChildMemoryConsumer>& child_memory_consumer = it->second;

  if (!inserted) {
    delegate_->RemoveMemoryConsumerFromChildProcess(
        consumer_id, context.child_process_id, child_memory_consumer.get());
  }

  child_memory_consumer.reset();
  child_memory_consumer = std::make_unique<ChildMemoryConsumer>(
      std::move(remote_consumer),
      base::BindOnce(
          &ChildMemoryConsumerRegistryHost::OnChildMemoryConsumerDisconnected,
          base::Unretained(this), context.child_process_id, consumer_id));

  delegate_->AddMemoryConsumerFromChildProcess(
      consumer_id, traits, context.process_type, context.child_process_id,
      child_memory_consumer.get());
}

void ChildMemoryConsumerRegistryHost::OnChildMemoryConsumerDisconnected(
    ChildProcessId child_process_id,
    const std::string& consumer_id,
    ChildMemoryConsumer* child_memory_consumer) {
  delegate_->RemoveMemoryConsumerFromChildProcess(consumer_id, child_process_id,
                                                  child_memory_consumer);

  size_t removed =
      child_memory_consumers_.erase(ConsumerKey(consumer_id, child_process_id));
  CHECK_EQ(removed, 1u);
}

}  // namespace content

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/child_memory_consumer_registry_host.h"

#include <utility>

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/message.h"

namespace content {

// ChildMemoryConsumerRegistryHost::ChildMemoryConsumer -----------------------

// An implementation of base::MemoryConsumer that proxy calls to the process's
// mojom::ChildMemoryCoordinator. This enables uniform handling of local and
// remote MemoryConsumers.
class ChildMemoryConsumerRegistryHost::ChildMemoryConsumer
    : public base::MemoryConsumer {
 public:
  ChildMemoryConsumer(ChildMemoryConsumerRegistryHost& host,
                      std::string consumer_id)
      : host_(host), consumer_id_(std::move(consumer_id)) {}
  ~ChildMemoryConsumer() override = default;

  // base::MemoryConsumer:
  void OnReleaseMemory() override { host_->NotifyReleaseMemory(consumer_id_); }
  void OnUpdateMemoryLimit() override {
    host_->NotifyUpdateMemoryLimit(consumer_id_, memory_limit());
  }

 private:
  const raw_ref<ChildMemoryConsumerRegistryHost> host_;
  const std::string consumer_id_;
};

// ChildMemoryConsumerRegistryHost --------------------------------------------

ChildMemoryConsumerRegistryHost::ChildMemoryConsumerRegistryHost(
    Delegate& delegate,
    ProcessType process_type,
    ChildProcessId child_process_id)
    : delegate_(delegate),
      process_type_(process_type),
      child_process_id_(child_process_id) {}

ChildMemoryConsumerRegistryHost::~ChildMemoryConsumerRegistryHost() {
  for (auto& [consumer_id, consumer] : consumers_) {
    delegate_->RemoveMemoryConsumerFromChildProcess(
        consumer_id, child_process_id_, consumer.get());
  }
}

void ChildMemoryConsumerRegistryHost::SetDisconnectHandler(
    base::OnceClosure handler) {
  CHECK(!disconnect_handler_);
  disconnect_handler_ = std::move(handler);
}

void ChildMemoryConsumerRegistryHost::BindCoordinator(
    mojo::PendingRemote<mojom::ChildMemoryCoordinator> coordinator_remote) {
  if (coordinator_remote_.is_bound()) {
    mojo::ReportBadMessage("BindCoordinator called more than once");
    return;
  }
  CHECK(disconnect_handler_);
  coordinator_remote_.Bind(std::move(coordinator_remote));
  coordinator_remote_.set_disconnect_handler(std::move(disconnect_handler_));
}

void ChildMemoryConsumerRegistryHost::Register(
    const std::string& consumer_id,
    base::MemoryConsumerTraits traits) {
  if (!coordinator_remote_.is_bound()) {
    mojo::ReportBadMessage("Register called before BindCoordinator");
    return;
  }

  auto [it, inserted] = consumers_.try_emplace(consumer_id);
  if (!inserted) {
    mojo::ReportBadMessage("Register called for an existing consumer_id");
    return;
  }
  std::unique_ptr<ChildMemoryConsumer>& child_memory_consumer = it->second;

  child_memory_consumer =
      std::make_unique<ChildMemoryConsumer>(*this, consumer_id);
  delegate_->AddMemoryConsumerFromChildProcess(consumer_id, traits,
                                               process_type_, child_process_id_,
                                               child_memory_consumer.get());
}

void ChildMemoryConsumerRegistryHost::Unregister(
    const std::string& consumer_id) {
  auto it = consumers_.find(consumer_id);
  if (it == consumers_.end()) {
    mojo::ReportBadMessage("Unregister called for a non-existing consumer_id");
    return;
  }

  delegate_->RemoveMemoryConsumerFromChildProcess(
      consumer_id, child_process_id_, it->second.get());
  consumers_.erase(it);
}

void ChildMemoryConsumerRegistryHost::NotifyReleaseMemory(
    const std::string& consumer_id) {
  coordinator_remote_->NotifyReleaseMemory(consumer_id);
}

void ChildMemoryConsumerRegistryHost::NotifyUpdateMemoryLimit(
    const std::string& consumer_id,
    int percentage) {
  coordinator_remote_->NotifyUpdateMemoryLimit(consumer_id, percentage);
}

}  // namespace content

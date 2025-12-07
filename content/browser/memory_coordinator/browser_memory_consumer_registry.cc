// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/memory_coordinator/browser_memory_consumer_registry.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"

namespace content {

namespace {

BrowserMemoryConsumerRegistry& GetInstance() {
  auto& instance = static_cast<BrowserMemoryConsumerRegistry&>(
      base::MemoryConsumerRegistry::Get());
  return instance;
}

}  // namespace

void BindBrowserMemoryConsumerRegistry(
    ProcessType process_type,
    ChildProcessId child_process_id,
    mojo::PendingReceiver<mojom::BrowserMemoryConsumerRegistry>
        pending_receiver) {
  auto& instance = GetInstance();
  instance.Bind(process_type, child_process_id, std::move(pending_receiver));
}

// BrowserMemoryConsumerRegistry::ConsumerInfo ---------------------------------

BrowserMemoryConsumerRegistry::ConsumerInfo::ConsumerInfo(
    std::string consumer_id,
    base::MemoryConsumerTraits traits,
    ProcessType process_type,
    ChildProcessId child_process_id,
    base::RegisteredMemoryConsumer consumer)
    : consumer_id(std::move(consumer_id)),
      traits(traits),
      process_type(process_type),
      child_process_id(child_process_id),
      consumer(consumer) {}

BrowserMemoryConsumerRegistry::ConsumerInfo::ConsumerInfo(ConsumerInfo&&) =
    default;

BrowserMemoryConsumerRegistry::ConsumerInfo&
BrowserMemoryConsumerRegistry::ConsumerInfo::operator=(ConsumerInfo&&) =
    default;

// BrowserMemoryConsumerRegistry::ChildMemoryConsumer --------------------------

BrowserMemoryConsumerRegistry::ChildMemoryConsumer::ChildMemoryConsumer(
    mojo::PendingRemote<mojom::ChildMemoryConsumer> remote_consumer,
    base::OnceCallback<void(ChildMemoryConsumer*)> on_disconnect_handler)
    : remote_consumer_(std::move(remote_consumer)) {
  remote_consumer_.set_disconnect_handler(
      base::BindOnce(std::move(on_disconnect_handler), this));
}

BrowserMemoryConsumerRegistry::ChildMemoryConsumer::~ChildMemoryConsumer() =
    default;

void BrowserMemoryConsumerRegistry::ChildMemoryConsumer::OnReleaseMemory() {
  remote_consumer_->NotifyReleaseMemory();
}

void BrowserMemoryConsumerRegistry::ChildMemoryConsumer::OnUpdateMemoryLimit() {
  remote_consumer_->NotifyUpdateMemoryLimit(memory_limit());
}

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
  CHECK(!base::Contains(memory_consumers_, consumer));
  memory_consumers_.push_back(consumer);
}
void BrowserMemoryConsumerRegistry::ConsumerGroup::RemoveMemoryConsumer(
    base::RegisteredMemoryConsumer consumer) {
  size_t removed = std::erase(memory_consumers_, consumer);
  CHECK_EQ(removed, 1u);
}

// BrowserMemoryConsumerRegistry -----------------------------------------------

BrowserMemoryConsumerRegistry::BrowserMemoryConsumerRegistry() = default;

BrowserMemoryConsumerRegistry::~BrowserMemoryConsumerRegistry() {
  NotifyDestruction();

  // Clear all references to consumers that live in a child process, as it's not
  // worth the hassle to wait until all disconnect notifications are received.

  // `consumer_infos_` must be cleared before `consumer_groups_` to avoid a
  // dangling pointer.
  std::erase_if(consumer_infos_, [](const auto& consumer_info) {
    return consumer_info.process_type != content::PROCESS_TYPE_BROWSER;
  });

  // `consumer_groups_` must be cleared before `child_memory_consumers_` to
  // avoid a dangling pointer.
  std::erase_if(consumer_groups_, [](const auto& element) {
    return std::get<1>(element.first) != ChildProcessId();
  });
  child_memory_consumers_.clear();
  receivers_.Clear();

  // This checks that all local consumers have unregistered in time.
  CHECK(consumer_groups_.empty());
  CHECK(consumer_infos_.empty());
}

void BrowserMemoryConsumerRegistry::Bind(
    ProcessType process_type,
    ChildProcessId child_process_id,
    mojo::PendingReceiver<mojom::BrowserMemoryConsumerRegistry>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver),
                 {process_type, child_process_id});
}

void BrowserMemoryConsumerRegistry::RegisterChildMemoryConsumer(
    const std::string& consumer_id,
    base::MemoryConsumerTraits traits,
    mojo::PendingRemote<mojom::ChildMemoryConsumer> remote_consumer) {
  ChildProcessId child_process_id =
      receivers_.current_context().child_process_id;

  // In some edge cases related to RPH reuse, there might already be a
  // registered consumer group for this ChildProcessId. Simply overwrite it.
  // Note that the value type (ChildMemoryConsumer) is not copyable or movable,
  // so it's not possible to simply overwrite the previous value. We must remove
  // and emplace afterwards.
  auto consumer_group_key = std::tie(consumer_id, child_process_id);
  auto it = child_memory_consumers_.lower_bound(consumer_group_key);
  if (it != child_memory_consumers_.end() && it->first == consumer_group_key) {
    ChildMemoryConsumer& child_memory_consumer = it->second;
    RemoveMemoryConsumerImpl(
        consumer_id, child_process_id,
        CreateRegisteredMemoryConsumer(&child_memory_consumer));

    it = child_memory_consumers_.erase(it);
  }

  it = child_memory_consumers_.emplace_hint(
      it, std::piecewise_construct,
      std::forward_as_tuple(consumer_id, child_process_id),
      std::forward_as_tuple(
          std::move(remote_consumer),
          base::BindOnce(
              &BrowserMemoryConsumerRegistry::OnChildMemoryConsumerDisconnected,
              base::Unretained(this), consumer_id, child_process_id)));

  ProcessType process_type = receivers_.current_context().process_type;

  AddMemoryConsumerImpl(consumer_id, traits, process_type, child_process_id,
                        CreateRegisteredMemoryConsumer(&it->second));
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
  auto [it, inserted] = consumer_groups_.emplace(
      std::piecewise_construct,
      std::forward_as_tuple(consumer_id, child_process_id),
      std::forward_as_tuple(traits, process_type));
  ConsumerGroup& consumer_group = it->second;

  if (inserted) {
    // First time seeing a consumer with this ID in this process. Add to
    // `consumer_infos_` to facilitate iteration by external callers.
    consumer_infos_.emplace_back(
        std::string(consumer_id), traits, process_type, child_process_id,
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
  auto it = consumer_groups_.find(std::tie(consumer_id, child_process_id));
  CHECK(it != consumer_groups_.end());
  ConsumerGroup& consumer_group = it->second;

  consumer_group.RemoveMemoryConsumer(consumer);

  if (consumer_group.empty()) {
    // Last consumer with this ID. Clean up from `consumer_infos_`.
    size_t removed = std::erase_if(
        consumer_infos_,
        [consumer_id, child_process_id](const ConsumerInfo& consumer_info) {
          return consumer_info.consumer_id == consumer_id &&
                 consumer_info.child_process_id == child_process_id;
        });
    CHECK_EQ(removed, 1u);

    // Also remove the group.
    consumer_groups_.erase(it);
  }
}

void BrowserMemoryConsumerRegistry::OnChildMemoryConsumerDisconnected(
    const std::string& consumer_id,
    ChildProcessId child_process_id,
    ChildMemoryConsumer* child_memory_consumer) {
  RemoveMemoryConsumerImpl(
      consumer_id, child_process_id,
      CreateRegisteredMemoryConsumer(child_memory_consumer));

  size_t removed =
      child_memory_consumers_.erase(std::tie(consumer_id, child_process_id));
  CHECK_EQ(removed, 1u);
}

namespace test {

void NotifyReleaseMemoryForTesting() {
  for (auto& consumer_info : GetInstance()) {
    consumer_info.consumer.ReleaseMemory();
  }
}

void NotifyUpdateMemoryLimitForTesting(int percentage) {
  for (auto& consumer_info : GetInstance()) {
    consumer_info.consumer.UpdateMemoryLimit(percentage);
  }
}

}  // namespace test

}  // namespace content

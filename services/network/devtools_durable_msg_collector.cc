// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/devtools_durable_msg_collector.h"

#include "services/network/devtools_durable_msg_collector_manager.h"

namespace network {

DevtoolsDurableMessageCollector::DevtoolsDurableMessageCollector(
    base::WeakPtr<DevtoolsDurableMessageCollectorManager> manager)
    : manager_(manager) {
  manager_->OnCollectorCreated(this);
}

DevtoolsDurableMessageCollector::~DevtoolsDurableMessageCollector() {
  // DevtoolsDurableMessage destructor calls back into this class via
  // WillRemoveBytes(). Explicitly clear the map to avoid a destruction
  // ordering bug.
  request_id_to_message_map_.clear();
  CHECK_EQ(cur_buffer_size_, 0);
  if (manager_) {
    manager_->OnCollectorDestroyed(this);
  }
}

void DevtoolsDurableMessageCollector::Configure(
    mojom::NetworkDurableMessageConfigPtr mojo_config,
    ConfigureCallback callback) {
  max_buffer_size_ =
      std::max(max_buffer_size_,
               static_cast<int64_t>(mojo_config->http_storage_max_size));
  std::move(callback).Run();
}

void DevtoolsDurableMessageCollector::Retrieve(
    const std::string& devtools_request_id,
    RetrieveCallback callback) {
  auto message = request_id_to_message_map_.find(devtools_request_id);
  if (message != request_id_to_message_map_.end() &&
      message->second->is_complete()) {
    return std::move(callback).Run(
        std::make_optional(message->second->Retrieve()));
  }

  return std::move(callback).Run(std::nullopt);
}


base::WeakPtr<DevtoolsDurableMessage>
DevtoolsDurableMessageCollector::CreateDurableMessage(
    const std::string& devtools_request_id) {
  auto [it, inserted] = request_id_to_message_map_.insert_or_assign(
      devtools_request_id,
      std::make_unique<DevtoolsDurableMessage>(devtools_request_id, *this));
  auto& message = it->second;

  // Mark eviction order.
  message_queue_.push(message->GetWeakPtr());

  if (manager_) {
    manager_->OnCollectorAddedMessage();
  }

  return message->GetWeakPtr();
}

void DevtoolsDurableMessageCollector::WillAddBytes(
    DevtoolsDurableMessage& message,
    int64_t size) {
  if (size < 0 || size > max_buffer_size_) {
    // This body cannot be stored with the current set limits.
    // If the beginning of this body was already stored, evict it and bail.
    EvictMessage(message);
    return;
  }

  // Evict prior items if we're short on storage buffer, locally or globally.
  while (!message_queue_.empty()) {
    bool local_ok = size <= max_buffer_size_ - cur_buffer_size_;
    bool global_ok = manager_ ? manager_->CanAccommodate(size) : true;
    if (local_ok && global_ok) {
      break;
    }

    auto evict_message = message_queue_.front();
    message_queue_.pop();
    if (evict_message) {
      bool is_current_message_evicted = (evict_message.get() == &message);
      EvictMessage(*evict_message);
      if (is_current_message_evicted) {
        // The message being called with is now evicted, and remaining bytes
        // will not be stored.
        return;
      }
    }
  }

  // Final check: if we're STILL out of global space (because our local queue
  // is empty, but other collectors are hogging global space), we drop *this*
  // message.
  bool local_ok = size <= max_buffer_size_ - cur_buffer_size_;
  bool global_ok = manager_ ? manager_->CanAccommodate(size) : true;
  if (!local_ok || !global_ok) {
    EvictMessage(message);
    return;
  }

  cur_buffer_size_ += size;
  if (manager_) {
    manager_->OnCollectorAddedBytes(size);
  }
}

void DevtoolsDurableMessageCollector::WillRemoveBytes(
    DevtoolsDurableMessage& message) {
  int64_t size = message.encoded_byte_size();
  cur_buffer_size_ -= size;
  CHECK_GE(cur_buffer_size_, 0);
  if (manager_) {
    manager_->OnCollectorRemovedBytes(size);
  }
}

void DevtoolsDurableMessageCollector::WillDestroyMessage(
    DevtoolsDurableMessage& message) {
  if (manager_) {
    manager_->OnCollectorRemovedMessage();
  }
}

void DevtoolsDurableMessageCollector::EvictMessage(
    const DevtoolsDurableMessage& message) {
  request_id_to_message_map_.erase(message.request_id());
}

void DevtoolsDurableMessageCollector::EnableForProfile(
    const base::UnguessableToken& profile_id,
    EnableForProfileCallback callback) {
  if (manager_) {
    manager_->EnableForProfile(profile_id, *this);
  }
  std::move(callback).Run();
}

void DevtoolsDurableMessageCollector::DisableForProfile(
    const base::UnguessableToken& profile_id,
    DisableForProfileCallback callback) {
  if (manager_) {
    manager_->DisableForProfile(profile_id, *this);
  }
  std::move(callback).Run();
}

}  // namespace network

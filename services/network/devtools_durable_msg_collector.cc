// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/devtools_durable_msg_collector.h"

namespace network {

DevtoolsDurableMessageCollector::DevtoolsDurableMessageCollector(
    base::OnceClosure profile_last_disconnect_handler)
    : profile_last_disconnect_handler_(
          std::move(profile_last_disconnect_handler)) {
  CHECK(profile_last_disconnect_handler_);
  receivers_.set_disconnect_handler(
      base::BindRepeating(&DevtoolsDurableMessageCollector::OnMojoDisconnect,
                          base::Unretained(this)));
}

DevtoolsDurableMessageCollector::~DevtoolsDurableMessageCollector() {
  // DevtoolsDurableMessage destructor calls back into this class via
  // WillRemoveBytes(). Explicitly clear the map to avoid a destruction
  // ordering bug.
  request_id_to_message_map_.clear();
}

void DevtoolsDurableMessageCollector::Configure(
    mojom::NetworkDurableMessageConfigPtr mojo_config) {
  max_buffer_size_ =
      std::max(max_buffer_size_,
               static_cast<int64_t>(mojo_config->http_storage_max_size));
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

void DevtoolsDurableMessageCollector::AddReceiver(
    mojo::PendingReceiver<mojom::DurableMessageCollector> receiver) {
  // Ensure we are not reusing the object past disconnect callback.
  CHECK(profile_last_disconnect_handler_);
  receivers_.Add(this, std::move(receiver));
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

  return message->GetWeakPtr();
}

void DevtoolsDurableMessageCollector::OnMojoDisconnect() {
  if (receivers_.empty()) {
    std::move(profile_last_disconnect_handler_).Run();
  }
}

void DevtoolsDurableMessageCollector::WillAddBytes(
    DevtoolsDurableMessage& message,
    int64_t size) {
  if (size > max_buffer_size_) {
    // This body cannot be stored with the current set limits.
    // If the beginning of this body was already stored, evict it and bail.
    EvictMessage(message);
    return;
  }

  // Evict prior items if we're short on storage buffer.
  while (!message_queue_.empty() &&
         cur_buffer_size_ + size > max_buffer_size_) {
    auto evict_message = message_queue_.front();
    message_queue_.pop();
    if (evict_message) {
      bool is_current_message_evicted = (evict_message.get() == &message);
      EvictMessage(*evict_message);
      if (is_current_message_evicted) {
        // The message being callec with is now evicted, and remaining bytes
        // will not be stored.
        return;
      }
    }
  }

  cur_buffer_size_ += size;
}

void DevtoolsDurableMessageCollector::WillRemoveBytes(
    DevtoolsDurableMessage& message) {
  cur_buffer_size_ -= message.encoded_byte_size();
  CHECK_GE(cur_buffer_size_, 0);
}

void DevtoolsDurableMessageCollector::EvictMessage(
    const DevtoolsDurableMessage& message) {
  request_id_to_message_map_.erase(message.request_id());
}

}  // namespace network

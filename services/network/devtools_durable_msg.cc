// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/devtools_durable_msg.h"

namespace network {

DevtoolsDurableMessage::DevtoolsDurableMessage(
    std::string request_id,
    DevtoolsDurableMessageAccountingDelegate& accounting_delegate)
    : request_id_(std::move(request_id)),
      accounting_delegate_(accounting_delegate) {}

DevtoolsDurableMessage::~DevtoolsDurableMessage() {
  accounting_delegate_->WillRemoveBytes(*this);
}

void DevtoolsDurableMessage::AddBytes(base::span<const uint8_t> bytes,
                                      size_t encoded_byte_size) {
  CHECK(!is_complete_);
  base::WeakPtr<DevtoolsDurableMessage> self = GetWeakPtr();
  accounting_delegate_->WillAddBytes(*this, encoded_byte_size);
  if (!self) {
    return;
  }

  bytes_.insert(bytes_.end(), bytes.begin(), bytes.end());
  encoded_byte_size_ += encoded_byte_size;
}

bool DevtoolsDurableMessage::CopyTo(base::span<uint8_t> destination) const {
  CHECK(is_complete_);
  if (destination.size() < byte_size()) {
    return false;
  }
  destination.copy_prefix_from(bytes_);
  return true;
}

void DevtoolsDurableMessage::MarkComplete() {
  is_complete_ = true;
}

}  // namespace network

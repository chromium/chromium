// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/multiple_durable_message_writer_impl.h"

#include "services/network/devtools_durable_msg.h"

namespace network {

MultipleDurableMessageWriterImpl::MultipleDurableMessageWriterImpl(
    std::vector<base::WeakPtr<DevtoolsDurableMessage>> messages)
    : messages_(std::move(messages)) {}

MultipleDurableMessageWriterImpl::~MultipleDurableMessageWriterImpl() = default;

void MultipleDurableMessageWriterImpl::AddBytes(base::span<const uint8_t> bytes,
                                                size_t encoded_size) {
  for (const auto& durable_message : messages_) {
    if (durable_message) {
      durable_message->AddBytes(bytes, encoded_size);
    }
  }
}

void MultipleDurableMessageWriterImpl::MarkComplete() {
  for (const auto& durable_message : messages_) {
    if (durable_message) {
      durable_message->MarkComplete();
    }
  }
}

void MultipleDurableMessageWriterImpl::SetClientDecodingTypes(
    std::vector<net::SourceStreamType> types) {
  for (const auto& durable_message : messages_) {
    if (durable_message) {
      durable_message->set_client_decoding_types(types);
    }
  }
}

}  // namespace network

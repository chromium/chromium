// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_H_
#define SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_H_

#include <stdint.h>

#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "net/filter/source_stream_type.h"
#include "services/network/devtools_durable_msg_accounting_delegate.h"

namespace mojo_base {
class BigBuffer;
}

namespace network {

class COMPONENT_EXPORT(NETWORK_SERVICE) DevtoolsDurableMessage {
 public:
  DevtoolsDurableMessage(
      std::string request_id,
      DevtoolsDurableMessageAccountingDelegate& accounting_delegate);
  DevtoolsDurableMessage(const DevtoolsDurableMessage&) = delete;
  DevtoolsDurableMessage& operator=(const DevtoolsDurableMessage&) = delete;
  ~DevtoolsDurableMessage();

  bool is_complete() const { return is_complete_; }
  const std::string& request_id() const { return request_id_; }
  size_t encoded_byte_size() const { return encoded_byte_size_; }
  size_t byte_size_for_testing() const { return bytes_.size(); }

  // Appends bytes to the message. May delete `this` if accounting_delegate_
  // decides to evict the current message.
  void AddBytes(base::span<const uint8_t> bytes, size_t encoded_size);
  void MarkComplete();
  mojo_base::BigBuffer Retrieve() const;
  base::WeakPtr<DevtoolsDurableMessage> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }
  void set_client_decoding_types(std::vector<net::SourceStreamType> types) {
    client_decoding_types_ = std::move(types);
  }

  // Test-only methods
  const std::vector<net::SourceStreamType>& GetClientDecodingTypesForTesting() {
    return client_decoding_types_;
  }

 private:
  std::vector<uint8_t> bytes_;
  bool is_complete_ = false;
  size_t encoded_byte_size_ = 0;
  const std::string request_id_;
  std::vector<net::SourceStreamType> client_decoding_types_;

  const raw_ref<DevtoolsDurableMessageAccountingDelegate> accounting_delegate_;
  base::WeakPtrFactory<DevtoolsDurableMessage> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_H_

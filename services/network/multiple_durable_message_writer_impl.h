// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_MULTIPLE_DURABLE_MESSAGE_WRITER_IMPL_H_
#define SERVICES_NETWORK_MULTIPLE_DURABLE_MESSAGE_WRITER_IMPL_H_

#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "net/filter/source_stream_type.h"
#include "services/network/devtools_durable_msg_writer.h"

namespace network {

class DevtoolsDurableMessage;

// MultipleDurableMessageWriterImpl is a DevtoolsDurableMessageWriter that
// writes the same bytes to multiple DevtoolsDurableMessage objects,
// each representing a different Durable Message collector (typically
// multiple debugging sessions) interested in the response body being collected.
class COMPONENT_EXPORT(NETWORK_SERVICE) MultipleDurableMessageWriterImpl
    : public DevtoolsDurableMessageWriter {
 public:
  explicit MultipleDurableMessageWriterImpl(
      std::vector<base::WeakPtr<DevtoolsDurableMessage>> messages);
  ~MultipleDurableMessageWriterImpl() override;

  // DevtoolsDurableMessageWriter implementation:
  void AddBytes(base::span<const uint8_t> bytes, size_t encoded_size) override;
  void MarkComplete() override;
  void SetClientDecodingTypes(
      std::vector<net::SourceStreamType> types) override;

 private:
  std::vector<base::WeakPtr<DevtoolsDurableMessage>> messages_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_MULTIPLE_DURABLE_MESSAGE_WRITER_IMPL_H_

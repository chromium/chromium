// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_WRITER_H_
#define SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_WRITER_H_

#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "net/filter/source_stream_type.h"

namespace network {

// Interface for a Durable Message writer, that will be passed into a URLLoader
// to collect response bodies for DevTools Durable Message collection.
// Typical implementation of this interface are handed over to URLLoader as
// WeakPtrs, to allow eviction when collector determines that the message can
// no longer be collected within allowed limits.
class COMPONENT_EXPORT(NETWORK_SERVICE) DevtoolsDurableMessageWriter {
 public:
  virtual ~DevtoolsDurableMessageWriter() = default;

  // Adds bytes to the message. Durable Messages are collected and stored
  // as they are seen by URLLoader, which may or may not be decoded.
  // Accounting for Durable Messages are  done per per encoded size.
  // https://w3c.github.io/webdriver-bidi/#command-network-addDataCollector
  // `encoded_size` represents the over-the-wire size of the chunk that is
  // being added, and this can be different from bytes.size() if the network
  // stack has already removed encoding.
  virtual void AddBytes(base::span<const uint8_t> bytes,
                        size_t encoded_size) = 0;

  // Mark that this message has completed writing. This usually means the
  // message will be available for decoding and retrieval by the Durable
  // Message collector.
  virtual void MarkComplete() = 0;

  // Set the client decoding types, if URLLoader has identified any.
  // Since we are storing encoded bytes, we need to know the decoding types
  // in order to decode the bytes on retrieval.
  virtual void SetClientDecodingTypes(
      std::vector<net::SourceStreamType> types) = 0;
};

}  // namespace network

#endif  // SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_WRITER_H_

// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_ENDPOINT_H_
#define NET_QUIC_QUIC_ENDPOINT_H_

#include "base/values.h"
#include "net/base/connection_endpoint_metadata.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

// Represents a single QUIC endpoint and the information necessary to attempt
// a QUIC session.
struct NET_EXPORT_PRIVATE QuicEndpoint {
  QuicEndpoint(quic::ParsedQuicVersion quic_version,
               IPEndPoint ip_endpoint,
               ConnectionEndpointMetadata metadata);

  QuicEndpoint(QuicEndpoint&&) = default;
  QuicEndpoint& operator=(QuicEndpoint&&) = default;
  QuicEndpoint(const QuicEndpoint&) = default;
  QuicEndpoint& operator=(const QuicEndpoint&) = default;

  ~QuicEndpoint();

  quic::ParsedQuicVersion quic_version = quic::ParsedQuicVersion::Unsupported();
  IPEndPoint ip_endpoint;
  ConnectionEndpointMetadata metadata;

  base::Value::Dict ToValue() const;
};

}  // namespace net

#endif  // NET_QUIC_QUIC_ENDPOINT_H_

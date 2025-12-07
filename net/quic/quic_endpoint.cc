// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_endpoint.h"

#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

QuicEndpoint::QuicEndpoint(quic::ParsedQuicVersion quic_version,
                           IPEndPoint ip_endpoint,
                           ConnectionEndpointMetadata metadata)
    : quic_version(quic_version),
      ip_endpoint(ip_endpoint),
      metadata(metadata) {}

QuicEndpoint::~QuicEndpoint() = default;

base::Value::Dict QuicEndpoint::ToValue() const {
  base::Value::Dict dict;
  dict.Set("quic_version", quic::ParsedQuicVersionToString(quic_version));
  dict.Set("ip_endpoint", ip_endpoint.ToString());
  dict.Set("metadata", metadata.ToValue());
  return dict;
}

}  // namespace net

// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_CONNECTION_ENDPOINT_METADATA_H_
#define NET_BASE_CONNECTION_ENDPOINT_METADATA_H_

#include <stdint.h>

#include <string>
#include <vector>

namespace net {

// Metadata used to create UDP/TCP/TLS/etc connections or information about such
// a connection.
struct ConnectionEndpointMetadata {
  // Expected to be parsed/consumed only by BoringSSL code and thus passed
  // around here only as a raw byte array.
  using EchConfigList = std::vector<uint8_t>;

  ConnectionEndpointMetadata();
  ~ConnectionEndpointMetadata();

  ConnectionEndpointMetadata(const ConnectionEndpointMetadata&);
  ConnectionEndpointMetadata& operator=(const ConnectionEndpointMetadata&) =
      default;
  ConnectionEndpointMetadata(ConnectionEndpointMetadata&&);
  ConnectionEndpointMetadata& operator=(ConnectionEndpointMetadata&&) = default;

  // ALPN strings for protocols supported by the endpoint. Empty for default
  // non-protocol endpoint.
  std::vector<std::string> supported_protocol_alpns;

  // If not empty, TLS Encrypted Client Hello config for the service.
  EchConfigList ech_config_list;
};

}  // namespace net

#endif  // NET_BASE_CONNECTION_ENDPOINT_METADATA_H_

// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/quic/quic_simple_server_backend_factory.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_flags.h"
#include "net/tools/quic/quic_http_proxy_backend_stream.h"

DEFINE_QUIC_COMMAND_LINE_FLAG(
    std::string,
    quic_mode,
    "cache",
    "Specifies the mode for the server to operate in. Either "
    "'cache' or 'proxy'");

DEFINE_QUIC_COMMAND_LINE_FLAG(
    std::string,
    quic_proxy_backend_url,
    "",
    "URL with http/https, IP address or host name and the port number of the "
    "backend server.");

namespace net {

std::unique_ptr<quic::QuicSimpleServerBackend>
QuicSimpleServerBackendFactory::CreateBackend() {
  if (GetQuicFlag(FLAGS_quic_mode) == "cache") {
    quic::QuicToyServer::MemoryCacheBackendFactory backend_factory;
    return backend_factory.CreateBackend();
  }

  if (GetQuicFlag(FLAGS_quic_mode) != "proxy") {
    LOG(ERROR) << "unknown --mode. cache or proxy are valid mode of operation";
    return nullptr;
  }

  auto backend = std::make_unique<net::QuicHttpProxyBackend>();
  std::string url = GetQuicFlag(FLAGS_quic_proxy_backend_url);
  if (!url.empty() && !backend->InitializeBackend(url)) {
    LOG(ERROR) << "--quic_proxy_backend_url " << url << " is not valid !";
    return nullptr;
  }
  return backend;
}

}  // namespace net

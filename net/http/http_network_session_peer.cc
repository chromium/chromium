// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/http/http_network_session_peer.h"

#include "net/http/http_network_session.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/transport_client_socket_pool.h"

namespace net {

HttpNetworkSessionPeer::HttpNetworkSessionPeer(HttpNetworkSession* session)
    : session_(session) {}

HttpNetworkSessionPeer::~HttpNetworkSessionPeer() = default;

void HttpNetworkSessionPeer::SetClientSocketPoolManager(
    std::unique_ptr<ClientSocketPoolManager> socket_pool_manager) {
  session_->normal_socket_pool_manager_.swap(socket_pool_manager);
}

void HttpNetworkSessionPeer::SetHttpStreamFactory(
    std::unique_ptr<HttpStreamFactory> http_stream_factory) {
  session_->http_stream_factory_.swap(http_stream_factory);
}

HttpNetworkSessionParams* HttpNetworkSessionPeer::params() {
  return &(session_->params_);
}

}  // namespace net

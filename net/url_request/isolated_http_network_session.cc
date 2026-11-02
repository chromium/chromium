// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/isolated_http_network_session.h"

#include <memory>

#include "net/http/http_network_session.h"
#include "net/http/http_server_properties.h"
#include "net/socket/client_socket_factory.h"
#include "net/url_request/url_request_context.h"

namespace net {

std::unique_ptr<HttpNetworkSession> CreateIsolatedHttpNetworkSession(
    const URLRequestContext* context,
    ClientSocketFactory* client_socket_factory,
    HttpServerProperties* http_server_properties,
    const HttpNetworkSessionParams* params_override) {
  CHECK(context);
  CHECK(client_socket_factory);
  CHECK(http_server_properties);
  // Sharing the primary context's HttpServerProperties would let
  // simulation-induced failures poison global state (e.g. a QUIC timeout
  // under simulated delay marking an origin's alternative service broken
  // for unthrottled requests), so callers must supply a separate instance.
  CHECK_NE(http_server_properties, context->http_server_properties());

  // Build a session context that shares everything with the primary session
  // but uses the provided socket factory. Copying the primary session's
  // context wholesale means a field added to HttpNetworkSessionContext later
  // is inherited here automatically, instead of silently staying default in
  // isolated sessions.
  const HttpNetworkSessionContext* primary_context =
      context->GetNetworkSessionContext();
  CHECK(primary_context);
  HttpNetworkSessionContext session_context = *primary_context;

  // Now apply what makes this session isolated.
  session_context.client_socket_factory = client_socket_factory;
  session_context.http_server_properties = http_server_properties;
  // Explicitly drop network_quality_estimator and
  // socket_performance_watcher_factory: an isolated session exists to
  // simulate network conditions, and feeding its socket-level observations
  // into the shared estimator would contaminate the global network-quality
  // estimate with simulated RTT and throughput.
  session_context.network_quality_estimator = nullptr;
  session_context.socket_performance_watcher_factory = nullptr;

  // Use the provided params, or copy the primary session's.
  const HttpNetworkSessionParams* params =
      params_override ? params_override : context->GetNetworkSessionParams();
  return std::make_unique<HttpNetworkSession>(*params, session_context);
}

}  // namespace net

// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_ISOLATED_HTTP_NETWORK_SESSION_H_
#define NET_URL_REQUEST_ISOLATED_HTTP_NETWORK_SESSION_H_

#include <memory>

#include "net/base/net_export.h"
#include "net/http/http_network_session.h"

namespace net {

class ClientSocketFactory;
class HttpServerProperties;
class URLRequestContext;

// Creates an HttpNetworkSession that shares the session-level dependencies
// of the given URLRequestContext (host resolver, cert verifier, transport
// security state, proxy resolution service, SSL config service, QUIC
// context, NetLog, Reporting/NEL services), but uses a different
// ClientSocketFactory, the caller-provided HttpServerProperties (which must
// be a separate instance from the context's, so simulation-induced failures
// cannot poison global alternative-service state), and has its own
// independent socket pools. The session is deliberately not wired to the
// context's NetworkQualityEstimator: simulated traffic must not feed the
// global network-quality estimate.
// Request-level services that are not part of HttpNetworkSessionContext -
// notably the cookie store and the HTTP cache - are not shared through this
// session; callers wire those up themselves (e.g. the network service pairs
// this session with its own isolated in-memory HttpCache).
//
// This enables per-consumer socket pool isolation (e.g., per-tab throttling
// in DevTools) while sharing the network state above.
//
// The returned session holds raw pointers to objects owned by the
// URLRequestContext, so the URLRequestContext, `http_server_properties` and
// `client_socket_factory` (stored unowned in the session's context) must all
// outlive it.
//
// `context` must have an HttpNetworkSession: the isolated session copies its
// session context and, unless `params_override` is provided, its params.
NET_EXPORT std::unique_ptr<HttpNetworkSession> CreateIsolatedHttpNetworkSession(
    const URLRequestContext* context,
    ClientSocketFactory* client_socket_factory,
    HttpServerProperties* http_server_properties,
    const HttpNetworkSessionParams* params_override = nullptr);

}  // namespace net

#endif  // NET_URL_REQUEST_ISOLATED_HTTP_NETWORK_SESSION_H_

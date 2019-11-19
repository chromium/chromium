// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/websocket_factory.h"

#include "base/bind.h"
#include "net/base/url_util.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/websocket.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace network {

WebSocketFactory::WebSocketFactory(NetworkContext* context)
    : context_(context) {}

WebSocketFactory::~WebSocketFactory() {
  // Subtle: This is important to avoid WebSocketFactory::Remove calls during
  // |connections_| destruction.
  connections_.clear();
}

void WebSocketFactory::CreateWebSocket(
    const GURL& url,
    const std::vector<std::string>& requested_protocols,
    const GURL& site_for_cookies,
    const net::NetworkIsolationKey& network_isolation_key,
    std::vector<mojom::HttpHeaderPtr> additional_headers,
    int32_t process_id,
    int32_t render_frame_id,
    const url::Origin& origin,
    uint32_t options,
    mojo::PendingRemote<mojom::WebSocketHandshakeClient> handshake_client,
    mojo::PendingRemote<mojom::AuthenticationHandler> auth_handler,
    mojo::PendingRemote<mojom::TrustedHeaderClient> header_client) {
  if (throttler_.HasTooManyPendingConnections(process_id)) {
    // Too many websockets!
    mojo::Remote<mojom::WebSocketHandshakeClient> handshake_client_remote(
        std::move(handshake_client));
    handshake_client_remote.ResetWithReason(
        mojom::WebSocket::kInsufficientResources,
        "Error in connection establishment: net::ERR_INSUFFICIENT_RESOURCES");
    return;
  }
  WebSocket::HasRawHeadersAccess has_raw_headers_access(
      context_->network_service()->HasRawHeadersAccess(
          process_id, net::ChangeWebSocketSchemeToHttpScheme(url)));
  connections_.insert(std::make_unique<WebSocket>(
      this, url, requested_protocols, site_for_cookies, network_isolation_key,
      std::move(additional_headers), process_id, render_frame_id, origin,
      options, has_raw_headers_access, std::move(handshake_client),
      std::move(auth_handler), std::move(header_client),
      throttler_.IssuePendingConnectionTracker(process_id),
      throttler_.CalculateDelay(process_id)));
}

net::URLRequestContext* WebSocketFactory::GetURLRequestContext() {
  return context_->url_request_context();
}

void WebSocketFactory::OnSSLCertificateError(
    base::OnceCallback<void(int)> callback,
    const GURL& url,
    int process_id,
    int render_frame_id,
    int net_error,
    const net::SSLInfo& ssl_info,
    bool fatal) {
  context_->client()->OnSSLCertificateError(process_id, render_frame_id, url,
                                            net_error, ssl_info, fatal,
                                            std::move(callback));
}

void WebSocketFactory::Remove(WebSocket* impl) {
  auto it = connections_.find(impl);
  if (it == connections_.end()) {
    // This is possible when this function is called inside the WebSocket
    // destructor.
    return;
  }
  connections_.erase(it);
}

}  // namespace network

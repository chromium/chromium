// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/websocket_factory.h"

#include "base/functional/bind.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/base/isolation_info.h"
#include "net/base/url_util.h"
#include "net/storage_access_api/status.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
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
  // `connections_` destruction.
  WebSocketSet connections = std::move(connections_);
}

void WebSocketFactory::CreateWebSocket(
    const GURL& url,
    const std::vector<std::string>& requested_protocols,
    const net::SiteForCookies& site_for_cookies,
    net::StorageAccessApiStatus storage_access_api_status,
    const net::IsolationInfo& isolation_info,
    std::vector<mojom::HttpHeaderPtr> additional_headers,
    int32_t process_id,
    const url::Origin& origin,
    uint32_t options,
    net::NetworkTrafficAnnotationTag traffic_annotation,
    mojo::PendingRemote<mojom::WebSocketHandshakeClient> handshake_client,
    mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_observer,
    mojo::PendingRemote<mojom::WebSocketAuthenticationHandler> auth_handler,
    mojo::PendingRemote<mojom::TrustedHeaderClient> header_client,
    const std::optional<base::UnguessableToken>& throttling_profile_id) {
  if (isolation_info.request_type() !=
      net::IsolationInfo::RequestType::kOther) {
    mojo::ReportBadMessage(
        "WebSocket's IsolationInfo::RequestType must be kOther");
    return;
  }

  // If `require_network_anonymization_key` is set, `isolation_info` must not be
  // empty.
  if (context_->require_network_anonymization_key()) {
    DCHECK(!isolation_info.IsEmpty());
  }

  if (throttler_.HasTooManyPendingConnections(process_id)) {
    // Too many websockets!
    mojo::Remote<mojom::WebSocketHandshakeClient> handshake_client_remote(
        std::move(handshake_client));
    handshake_client_remote->OnFailure("Insufficient resources",
                                       net::ERR_INSUFFICIENT_RESOURCES, -1);
    handshake_client_remote.reset();
    return;
  }
  if (isolation_info.nonce().has_value() &&
      !context_->IsNetworkForNonceAndUrlAllowed(*isolation_info.nonce(), url)) {
    mojo::Remote<mojom::WebSocketHandshakeClient> handshake_client_remote(
        std::move(handshake_client));
    handshake_client_remote->OnFailure("Network access revoked",
                                       net::ERR_NETWORK_ACCESS_REVOKED, -1);
    handshake_client_remote.reset();
    return;
  }
  WebSocket::HasRawHeadersAccess has_raw_headers_access(
      context_->network_service()->HasRawHeadersAccess(
          process_id, net::ChangeWebSocketSchemeToHttpScheme(url)));
  connections_.insert(std::make_unique<WebSocket>(
      this, url, requested_protocols, site_for_cookies,
      storage_access_api_status, isolation_info, std::move(additional_headers),
      origin, options, traffic_annotation, has_raw_headers_access,
      std::move(handshake_client), std::move(url_loader_network_observer),
      std::move(auth_handler), std::move(header_client),
      throttler_.IssuePendingConnectionTracker(process_id),
      throttler_.CalculateDelay(process_id), throttling_profile_id));
}

net::URLRequestContext* WebSocketFactory::GetURLRequestContext() {
  return context_->url_request_context();
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

void WebSocketFactory::RemoveIfNonceMatches(
    const base::UnguessableToken& nonce) {
  std::erase_if(connections_, [&nonce](const auto& connection) {
    return connection->RevokeIfNonceMatches(nonce);
  });
}

}  // namespace network

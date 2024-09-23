// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEBSOCKET_FACTORY_H_
#define SERVICES_NETWORK_WEBSOCKET_FACTORY_H_

#include <set>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "services/network/websocket.h"
#include "services/network/websocket_throttler.h"

class GURL;

namespace net {
class IsolationInfo;
class SiteForCookies;
class SSLInfo;
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace network {

class NetworkContext;
class WebSocket;

class WebSocketFactory final {
 public:
  explicit WebSocketFactory(NetworkContext* context);

  WebSocketFactory(const WebSocketFactory&) = delete;
  WebSocketFactory& operator=(const WebSocketFactory&) = delete;

  ~WebSocketFactory();

  void CreateWebSocket(
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
      const std::optional<base::UnguessableToken>& throttling_profile_id);

  // Returns a URLRequestContext associated with this factory.
  net::URLRequestContext* GetURLRequestContext();

  // Called when a WebSocket sees a SSL certificate error.
  void OnSSLCertificateError(base::OnceCallback<void(int)> callback,
                             const GURL& url,
                             int process_id,
                             int render_frame_id,
                             int net_error,
                             const net::SSLInfo& ssl_info,
                             bool fatal);

  // Removes and deletes |impl|.
  void Remove(WebSocket* impl);

  // Close existing WebSocket connections when network access is revoked from a
  // fenced frame. The frame's associated WebSockets are identified via their
  // IsolationInfo's nonce.
  void RemoveIfNonceMatches(const base::UnguessableToken& nonce);

 private:
  using WebSocketSet =
      std::set<std::unique_ptr<WebSocket>, base::UniquePtrComparator>;
  // The connections held by this factory.
  WebSocketSet connections_;

  WebSocketThrottler throttler_;

  // |context_| outlives this object.
  const raw_ptr<NetworkContext> context_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEBSOCKET_FACTORY_H_

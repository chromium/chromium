// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEBSOCKET_FACTORY_H_
#define SERVICES_NETWORK_WEBSOCKET_FACTORY_H_

#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/memory/weak_ptr.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "services/network/websocket.h"
#include "services/network/websocket_throttler.h"

class GURL;

namespace net {
class SSLInfo;
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
  ~WebSocketFactory();

  void CreateWebSocket(
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
      mojo::PendingRemote<mojom::TrustedHeaderClient> header_client);

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

 private:
  // The connections held by this factory.
  std::set<std::unique_ptr<WebSocket>, base::UniquePtrComparator> connections_;

  WebSocketThrottler throttler_;

  // |context_| outlives this object.
  NetworkContext* const context_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketFactory);
};

}  // namespace network

#endif  // SERVICES_NETWORK_WEBSOCKET_FACTORY_H_

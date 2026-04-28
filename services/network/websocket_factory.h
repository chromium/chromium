// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_WEBSOCKET_FACTORY_H_
#define SERVICES_NETWORK_WEBSOCKET_FACTORY_H_

#include <set>
#include <vector>

#include "base/component_export.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "net/log/net_log.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "services/network/websocket.h"
#include "services/network/websocket_throttler.h"

class GURL;

namespace net {
class IsolationInfo;
class OriginatingProcessId;
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace url {
class Origin;
}  // namespace url

namespace network {

class NetworkContext;
class WebSocket;

class COMPONENT_EXPORT(NETWORK_SERVICE) WebSocketFactory final {
 public:
  explicit WebSocketFactory(NetworkContext* context);

  WebSocketFactory(const WebSocketFactory&) = delete;
  WebSocketFactory& operator=(const WebSocketFactory&) = delete;

  ~WebSocketFactory();

  void CreateWebSocket(
      const GURL& url,
      const std::vector<std::string>& requested_protocols,
      net::StorageAccessApiStatus storage_access_api_status,
      const net::IsolationInfo& isolation_info,
      std::vector<mojom::HttpHeaderPtr> additional_headers,
      const network::OriginatingProcessId& process_id,
      const url::Origin& origin,
      network::mojom::ClientSecurityStatePtr client_security_state,
      uint32_t options,
      net::NetworkTrafficAnnotationTag traffic_annotation,
      mojo::PendingRemote<mojom::WebSocketHandshakeClient> handshake_client,
      mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_observer,
      mojo::PendingRemote<mojom::WebSocketAuthenticationHandler> auth_handler,
      mojo::PendingRemote<mojom::TrustedHeaderClient> header_client,
      const std::optional<base::UnguessableToken>& throttling_profile_id,
      const std::optional<base::UnguessableToken>& network_restrictions_id);

  // Returns a URLRequestContext associated with this factory.
  net::URLRequestContext* GetURLRequestContext();

  // Removes and deletes |impl|.
  void Remove(WebSocket* impl);

  // Creates synthetic WEBSOCKET_ALIVE NetLog entries for pre-existing
  // WebSocket connections. Called when NetLog capture starts to provide
  // visibility into active WebSockets that were created before logging began.
  // Pending/throttled connections (where the channel hasn't been created yet)
  // are silently skipped. Each entry uses the WebSocket's original creation
  // time, which predates the log start, so they appear with negative time
  // offsets in the export.
  void CreateNetLogEntriesForActiveConnections(
      net::NetLog::ThreadSafeObserver* observer) const;

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

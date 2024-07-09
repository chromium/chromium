// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBSOCKETS_WEBSOCKET_CONNECTOR_IMPL_H_
#define CONTENT_BROWSER_WEBSOCKETS_WEBSOCKET_CONNECTOR_IMPL_H_

#include <optional>
#include <string>
#include <vector>

#include "base/unguessable_token.h"
#include "content/public/browser/content_browser_client.h"
#include "net/base/isolation_info.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/websocket.mojom.h"
#include "third_party/blink/public/mojom/websockets/websocket_connector.mojom.h"
#include "url/origin.h"

class GURL;

namespace content {

class WebSocketConnectorImpl final : public blink::mojom::WebSocketConnector {
 public:
  using WebSocketFactory = ContentBrowserClient::WebSocketFactory;

  // Called on the UI thread.
  // - For frames, |frame_id| should be their own id.
  // - For dedicated workers, |frame_id| should be its response document's
  //   frame's id.
  // - For shared workers and service workers, |frame_id| should be
  //   MSG_ROUTING_NONE because they do not have a frame.
  WebSocketConnectorImpl(int process_id,
                         int frame_id,
                         const url::Origin& origin,
                         const net::IsolationInfo& isolation_info);
  ~WebSocketConnectorImpl() override;

  // WebSocketConnector implementation
  void Connect(const GURL& url,
               const std::vector<std::string>& requested_protocols,
               const net::SiteForCookies& site_for_cookies,
               const std::optional<std::string>& user_agent,
               net::StorageAccessApiStatus storage_access_api_status,
               mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
                   handshake_client,
               const std::optional<base::UnguessableToken>&
                   throttling_profile_id) override;

 private:
  static void ConnectCalledByContentBrowserClient(
      const std::vector<std::string>& requested_protocols,
      const net::SiteForCookies& site_for_cookies,
      net::StorageAccessApiStatus storage_access_api_status,
      const net::IsolationInfo& isolation_info,
      int process_id,
      int frame_id,
      const url::Origin& origin,
      uint32_t options,
      std::optional<base::UnguessableToken> throttling_profile_id,
      const GURL& url,
      std::vector<network::mojom::HttpHeaderPtr> additional_headers,
      mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
          handshake_client,
      mojo::PendingRemote<network::mojom::WebSocketAuthenticationHandler>
          auth_handler,
      mojo::PendingRemote<network::mojom::TrustedHeaderClient>
          trusted_header_client);

  const int process_id_;
  const int frame_id_;
  const url::Origin origin_;
  const net::IsolationInfo isolation_info_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBSOCKETS_WEBSOCKET_CONNECTOR_IMPL_H_

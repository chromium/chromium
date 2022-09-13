// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_PROXYING_WEBTRANSPORT_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_PROXYING_WEBTRANSPORT_H_

#include "content/public/browser/content_browser_client.h"
#include "extensions/browser/api/web_request/web_request_api.h"

class GURL;

namespace extensions {

// Starts proxying WebTransport handshake if the extensions want to listen it
// by overrinding `handshake_client`.
void StartWebRequestProxyingWebTransport(
    content::RenderProcessHost& render_process_host,
    int frame_routing_id,
    const GURL& url,
    const url::Origin& initiator_origin,
    mojo::PendingRemote<network::mojom::WebTransportHandshakeClient>
        handshake_client,
    int64_t request_id,
    WebRequestAPI::ProxySet& proxies,
    content::ContentBrowserClient::WillCreateWebTransportCallback callback);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_PROXYING_WEBTRANSPORT_H_
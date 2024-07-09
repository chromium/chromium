// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/websockets/websocket_connector_impl.h"

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/cpp/features.h"
#include "url/gurl.h"

namespace content {

namespace {

url::Origin MaybeTreatLocalOriginAsOpaque(const url::Origin& origin) {
  if (base::Contains(url::GetLocalSchemes(), origin.scheme()) &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAllowFileAccessFromFiles)) {
    // For local origins we should use an opaque origin unless
    // "--allow-file-access-from-files" is specified. This should have been
    // done in content::RenderFrameHost. See https://crbug.com/1206736 for
    // details.
    return origin.DeriveNewOpaqueOrigin();
  }
  return origin;
}

constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("websocket_stream", R"(
        semantics {
          sender: "WebSocket Handshake"
          description:
            "Renderer process initiated WebSocket handshake. The WebSocket "
            "handshake is used to establish a connection between a web page "
            "and a consenting server for bi-directional communication."
          trigger:
            "A handshake is performed every time a new connection is "
            "established via the Javascript or PPAPI WebSocket API. Any web "
            "page or extension can create a WebSocket connection."
          data: "The path and sub-protocols requested when the WebSocket was "
                "created, plus the origin of the creating page."
          destination: OTHER
        }
        policy {
          cookies_allowed: YES
          cookies_store: "user or per-app cookie store"
          setting: "These requests cannot be disabled."
          policy_exception_justification:
            "Not implemented. WebSocket is a core web platform API."
        })");
}

WebSocketConnectorImpl::WebSocketConnectorImpl(
    int process_id,
    int frame_id,
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info)
    : process_id_(process_id),
      frame_id_(frame_id),
      origin_(MaybeTreatLocalOriginAsOpaque(origin)),
      isolation_info_(isolation_info) {}

WebSocketConnectorImpl::~WebSocketConnectorImpl() = default;

void WebSocketConnectorImpl::Connect(
    const GURL& url,
    const std::vector<std::string>& requested_protocols,
    const net::SiteForCookies& site_for_cookies,
    const std::optional<std::string>& user_agent,
    net::StorageAccessApiStatus storage_access_api_status,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client,
    const std::optional<base::UnguessableToken>& throttling_profile_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderProcessHost* process = RenderProcessHost::FromID(process_id_);
  if (!process) {
    return;
  }

  RenderFrameHost* frame = RenderFrameHost::FromID(process_id_, frame_id_);
  const uint32_t options =
      GetContentClient()->browser()->GetWebSocketOptions(frame);

  if (GetContentClient()->browser()->WillInterceptWebSocket(frame)) {
    GetContentClient()->browser()->CreateWebSocket(
        frame,
        base::BindOnce(ConnectCalledByContentBrowserClient, requested_protocols,
                       site_for_cookies, storage_access_api_status,
                       isolation_info_, process_id_, frame_id_, origin_,
                       options, std::move(throttling_profile_id)),
        url, site_for_cookies, user_agent, std::move(handshake_client));
    return;
  }
  std::vector<network::mojom::HttpHeaderPtr> headers;
  if (user_agent) {
    headers.push_back(network::mojom::HttpHeader::New(
        net::HttpRequestHeaders::kUserAgent, *user_agent));
  }
  process->GetStoragePartition()->GetNetworkContext()->CreateWebSocket(
      url, requested_protocols, site_for_cookies, storage_access_api_status,
      isolation_info_, std::move(headers), process_id_, origin_, options,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      std::move(handshake_client),
      process->GetStoragePartition()->CreateURLLoaderNetworkObserverForFrame(
          process_id_, frame_id_),
      mojo::NullRemote(), mojo::NullRemote(), std::move(throttling_profile_id));
}

void WebSocketConnectorImpl::ConnectCalledByContentBrowserClient(
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
        trusted_header_client) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RenderProcessHost* process = RenderProcessHost::FromID(process_id);
  if (!process) {
    return;
  }
  process->GetStoragePartition()->GetNetworkContext()->CreateWebSocket(
      url, requested_protocols, site_for_cookies, storage_access_api_status,
      isolation_info, std::move(additional_headers), process_id, origin,
      options, net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      std::move(handshake_client),
      process->GetStoragePartition()->CreateURLLoaderNetworkObserverForFrame(
          process_id, frame_id),
      std::move(auth_handler), std::move(trusted_header_client),
      std::move(throttling_profile_id));
}

}  // namespace content

// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/websockets/websocket_connector_impl.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/not_fatal_until.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/child_process_id_util.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "mojo/public/cpp/bindings/message.h"
#include "net/http/http_request_headers.h"
#include "net/storage_access_api/status.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/websocket_utils.h"
#include "url/gurl.h"

namespace content {

namespace {

url::Origin MaybeTreatLocalOriginAsOpaque(const url::Origin& origin) {
  if (std::ranges::contains(url::GetLocalSchemes(), origin.scheme()) &&
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
            "established via the Javascript. Any web page or extension can "
            "create a WebSocket connection."
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
    const content::GlobalRenderFrameHostId& frame_id,
    WeakDocumentPtr weak_document,
    const url::Origin& origin,
    const net::IsolationInfo& isolation_info,
    network::mojom::ClientSecurityStatePtr client_security_state,
    net::StorageAccessApiStatus storage_access_api_status,
    const base::UnguessableToken& network_restrictions_id,
    std::optional<base::UnguessableToken> devtools_worker_token)
    : frame_id_(frame_id),
      weak_document_(std::move(weak_document)),
      origin_(MaybeTreatLocalOriginAsOpaque(origin)),
      isolation_info_(isolation_info),
      client_security_state_(std::move(client_security_state)),
      storage_access_api_status_(storage_access_api_status),
      network_restrictions_id_(network_restrictions_id),
      devtools_worker_token_(std::move(devtools_worker_token)) {
  CHECK(!network_restrictions_id.is_empty(), base::NotFatalUntil::M165);
}

WebSocketConnectorImpl::~WebSocketConnectorImpl() = default;

void WebSocketConnectorImpl::Connect(
    const GURL& url,
    const std::vector<std::string>& requested_protocols,
    const std::optional<std::string>& user_agent,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client,
    const std::optional<base::UnguessableToken>& throttling_profile_id,
    network::mojom::IPAddressSpace target_address_space) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI),
        base::NotFatalUntil::M159);

  // If the connector was created for a RenderFrame or a DedicatedWorker
  // (where `frame_routing_id` is not kRoutingIdNone), it is scoped to a
  // specific creator document. If that document is no longer valid (e.g., due
  // to a navigation committing a new document in the same frame, or frame
  // destruction), abort the connection. For Shared and Service Workers
  // (`frame_routing_id` is kRoutingIdNone), they operate independently of any
  // document lifecycle, so we skip this check.
  if (frame_id_.frame_routing_id != IPC::mojom::kRoutingIdNone &&
      !weak_document_.AsRenderFrameHostIfValid()) {
    return;
  }

  if (auto error = network::VerifyWebSocketConnectParameters(
          url, requested_protocols, isolation_info_)) {
    mojo::ReportBadMessage(*error);
    return;
  }

  RenderProcessHost* process = RenderProcessHost::FromID(frame_id_.child_id);
  if (!process) {
    return;
  }

  RenderFrameHost* frame = RenderFrameHost::FromID(frame_id_);
  content::ContentBrowserClient::WebSocketOptions options =
      GetContentClient()->browser()->GetWebSocketOptions(frame);

  content::ContentBrowserClient::WebSocketFactory factory = base::BindOnce(
      ConnectCalledByContentBrowserClient, requested_protocols,
      storage_access_api_status_, isolation_info_, frame_id_,
      devtools_worker_token_, origin_, client_security_state_->Clone(),
      options.options, std::move(throttling_profile_id),
      network_restrictions_id_, target_address_space);

  if (GetContentClient()->browser()->WillInterceptWebSocket(frame)) {
    GetContentClient()->browser()->CreateWebSocket(
        frame, std::move(factory), url, isolation_info_.site_for_cookies(),
        user_agent, std::move(handshake_client), std::move(options));
    return;
  }

  net::HttpRequestHeaders headers;
  if (user_agent) {
    headers.SetHeader(net::HttpRequestHeaders::kUserAgent, *user_agent);
  }
  devtools_instrumentation::ApplyExtraHeadersForWebSocket(
      frame_id_, devtools_worker_token_, &headers);

  std::vector<network::mojom::HttpHeaderPtr> additional_headers;
  for (net::HttpRequestHeaders::Iterator it(headers); it.GetNext();) {
    additional_headers.push_back(
        network::mojom::HttpHeader::New(it.name(), it.value()));
  }

  std::move(factory).Run(url, std::move(additional_headers),
                         std::move(handshake_client), mojo::NullRemote(),
                         std::move(options.header_client));
}

void WebSocketConnectorImpl::ConnectCalledByContentBrowserClient(
    const std::vector<std::string>& requested_protocols,
    net::StorageAccessApiStatus storage_access_api_status,
    const net::IsolationInfo& isolation_info,
    const content::GlobalRenderFrameHostId& frame_id,
    std::optional<base::UnguessableToken> devtools_worker_token,
    const url::Origin& origin,
    network::mojom::ClientSecurityStatePtr client_security_state,
    uint32_t options,
    std::optional<base::UnguessableToken> throttling_profile_id,
    const base::UnguessableToken& network_restrictions_id,
    network::mojom::IPAddressSpace target_address_space,
    const GURL& url,
    std::vector<network::mojom::HttpHeaderPtr> additional_headers,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client,
    mojo::PendingRemote<network::mojom::WebSocketAuthenticationHandler>
        auth_handler,
    mojo::PendingRemote<network::mojom::TrustedHeaderClient>
        trusted_header_client) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI),
        base::NotFatalUntil::M159);
  RenderProcessHost* process = RenderProcessHost::FromID(frame_id.child_id);
  if (!process) {
    return;
  }

  net::HttpRequestHeaders extra_headers;
  devtools_instrumentation::ApplyExtraHeadersForWebSocket(
      frame_id, devtools_worker_token, &extra_headers);
  std::erase_if(additional_headers, [&](const auto& header) {
    return extra_headers.HasHeader(header->name);
  });
  for (net::HttpRequestHeaders::Iterator it(extra_headers); it.GetNext();) {
    additional_headers.push_back(
        network::mojom::HttpHeader::New(it.name(), it.value()));
  }

  content::StoragePartition* storage_partition = process->GetStoragePartition();

  mojo::PendingRemote<network::mojom::URLLoaderNetworkServiceObserver>
      url_loader_network_service_observer =
          frame_id.frame_routing_id == IPC::mojom::kRoutingIdNone
              ? static_cast<StoragePartitionImpl*>(storage_partition)
                    ->CreateURLLoaderNetworkObserverForServiceOrSharedWorker(
                        ToOriginatingProcessId(frame_id.child_id), origin)
              : storage_partition->CreateURLLoaderNetworkObserverForFrame(
                    frame_id);

  storage_partition->GetNetworkContext()->CreateWebSocket(
      url, requested_protocols, storage_access_api_status, isolation_info,
      std::move(additional_headers), ToOriginatingProcessId(frame_id.child_id),
      origin, std::move(client_security_state), options,
      net::MutableNetworkTrafficAnnotationTag(kTrafficAnnotation),
      std::move(handshake_client),
      std::move(url_loader_network_service_observer), std::move(auth_handler),
      std::move(trusted_header_client), std::move(throttling_profile_id),
      network_restrictions_id, target_address_space);
}

}  // namespace content

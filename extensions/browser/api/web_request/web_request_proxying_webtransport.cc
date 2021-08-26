// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_proxying_webtransport.h"

#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "services/network/public/mojom/web_transport.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace extensions {

namespace {

using network::mojom::WebTransportHandshakeClient;
using CreateCallback =
    content::ContentBrowserClient::WillCreateWebTransportCallback;

class WebTransportHandshakeProxy : public WebRequestAPI::Proxy {
 public:
  WebTransportHandshakeProxy(
      mojo::PendingRemote<WebTransportHandshakeClient> handshake_client,
      WebRequestAPI::ProxySet& proxies,
      content::BrowserContext* browser_context,
      WebRequestInfoInitParams params,
      CreateCallback create_callback)
      : handshake_client_(std::move(handshake_client)),
        proxies_(proxies),
        browser_context_(browser_context),
        info_(std::move(params)),
        create_callback_(std::move(create_callback)) {
    DCHECK(create_callback_);
    DCHECK(handshake_client_);
  }
  ~WebTransportHandshakeProxy() override {
    // This is important to ensure that no outstanding blocking requests
    // continue to reference state owned by this object.
    ExtensionWebRequestEventRouter::GetInstance()->OnRequestWillBeDestroyed(
        browser_context_, &info_);
  }

  void Start() {
    bool should_collapse_initiator = false;
    const int result =
        ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRequest(
            browser_context_, &info_,
            base::BindOnce(&WebTransportHandshakeProxy::OnCompleted,
                           base::Unretained(this)),
            &redirect_url_, &should_collapse_initiator);
    // It doesn't make sense to collapse WebTransport requests since they won't
    // be associated with a DOM element.
    DCHECK(!should_collapse_initiator);

    if (result == net::ERR_BLOCKED_BY_CLIENT) {
      // TODO(crbug.com/1240935): Implement blocking case. You also should fix
      // framework.js's flakiness.
      OnCompleted(net::OK);
      return;
    }

    if (result == net::ERR_IO_PENDING)
      return;

    DCHECK_EQ(net::OK, result);
    OnCompleted(net::OK);
  }

  // TODO(crbug.com/1240935): Implement onBeforeSendHeaders
  // TODO(crbug.com/1240935): Implement onSendHeaders
  // TODO(crbug.com/1240935): Implement
  // network::mojom::WebTransportHandshakeClient and Start proxy. This blocks
  // following TODOs.
  // TODO(crbug.com/1240935): Implement WebRequestAPI::onHeadersReceived
  // TODO(crbug.com/1240935): Implement WebRequestAPI::onResponseStarted
  // TODO(crbug.com/1240935): Implement WebRequestAPI::onCompleted
  // TODO(crbug.com/1240935): Implement WebRequestAPI::OnErrorOccurred.

  void OnCompleted(int error_code) {
    absl::optional<network::mojom::WebTransportErrorPtr> error;
    if (error_code != net::OK) {
      error = network::mojom::WebTransportError::New(
          error_code, quic::QUIC_INTERNAL_ERROR, "Blocked by an extension",
          false);
    }
    std::move(create_callback_)
        .Run(std::move(handshake_client_), std::move(error));
    // Delete `this`.
    proxies_.RemoveProxy(this);
  }

 private:
  mojo::PendingRemote<WebTransportHandshakeClient> handshake_client_;
  // Weak reference to the ProxySet. This is safe as `proxies_` owns this
  // object.
  WebRequestAPI::ProxySet& proxies_;
  content::BrowserContext* browser_context_;
  WebRequestInfo info_;
  GURL redirect_url_;
  CreateCallback create_callback_;
};

}  // namespace

void StartWebRequestProxyingWebTransport(
    content::RenderFrameHost& frame,
    const GURL& url,
    mojo::PendingRemote<WebTransportHandshakeClient> handshake_client,
    int64_t request_id,
    WebRequestAPI::ProxySet& proxies,
    content::ContentBrowserClient::WillCreateWebTransportCallback callback) {
  content::BrowserContext* browser_context =
      frame.GetProcess()->GetBrowserContext();

  // Filling ResourceRequest fields required to create WebRequestInfoInitParams.
  network::ResourceRequest request;
  request.url = url;
  request.request_initiator = frame.GetLastCommittedOrigin();

  const auto* web_contents = content::WebContents::FromRenderFrameHost(&frame);
  const ukm::SourceIdObj& ukm_source_id =
      web_contents ? ukm::SourceIdObj::FromInt64(
                         ukm::GetSourceIdForWebContentsDocument(web_contents))
                   : ukm::kInvalidSourceIdObj;

  WebRequestInfoInitParams params = WebRequestInfoInitParams(
      request_id, frame.GetProcess()->GetID(), frame.GetRoutingID(),
      // TODO(crbug.com/1243521): Set appropriate view_routing_id.
      /*navigation_ui_data=*/nullptr, /*view_routing_id=*/MSG_ROUTING_NONE,
      request,
      /*is_download=*/false,
      /*is_async=*/true,
      /*is_service_worker_script=*/false,
      /*navigation_id=*/absl::nullopt, ukm_source_id);
  params.web_request_type = WebRequestResourceType::WEB_TRANSPORT;

  auto proxy = std::make_unique<WebTransportHandshakeProxy>(
      std::move(handshake_client), proxies, browser_context, std::move(params),
      std::move(callback));
  auto* raw_proxy = proxy.get();
  proxies.AddProxy(std::move(proxy));
  raw_proxy->Start();
}

}  // namespace extensions

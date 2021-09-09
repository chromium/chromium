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

class WebTransportHandshakeProxy : public WebRequestAPI::Proxy,
                                   public WebTransportHandshakeClient {
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
    DCHECK(handshake_client_);
    DCHECK(create_callback_);
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
            base::BindOnce(&WebTransportHandshakeProxy::StartProxyWhenNoError,
                           base::Unretained(this)),
            &redirect_url_, &should_collapse_initiator);
    // It doesn't make sense to collapse WebTransport requests since they won't
    // be associated with a DOM element.
    DCHECK(!should_collapse_initiator);

    if (result == net::ERR_IO_PENDING)
      return;

    DCHECK(result == net::OK || result == net::ERR_BLOCKED_BY_CLIENT) << result;
    StartProxyWhenNoError(result);
  }

  // Below two events should be triggered before proxing.
  // TODO(crbug.com/1240935): Implement onBeforeSendHeaders
  // TODO(crbug.com/1240935): Implement onSendHeaders

  void StartProxyWhenNoError(int error_code) {
    if (error_code != net::OK) {
      auto webtransport_error = network::mojom::WebTransportError::New(
          error_code, quic::QUIC_INTERNAL_ERROR, "Blocked by an extension",
          false);
      std::move(create_callback_)
          .Run(std::move(handshake_client_), std::move(webtransport_error));
      OnCompleted(error_code);
      // `this` is deleted.
      return;
    }

    // Set up proxing.
    remote_.Bind(std::move(handshake_client_));
    remote_.set_disconnect_handler(
        base::BindOnce(&WebTransportHandshakeProxy::OnCompleted,
                       base::Unretained(this), net::ERR_ABORTED));
    std::move(create_callback_)
        .Run(receiver_.BindNewPipeAndPassRemote(), absl::nullopt);
    receiver_.set_disconnect_handler(
        base::BindOnce(&WebTransportHandshakeProxy::OnCompleted,
                       base::Unretained(this), net::ERR_ABORTED));
  }

  // WebTransportHandshakeClient implementation:
  // Proxing should be finished with either of below functions.
  void OnConnectionEstablished(
      mojo::PendingRemote<network::mojom::WebTransport> transport,
      mojo::PendingReceiver<network::mojom::WebTransportClient> client)
      override {
    remote_->OnConnectionEstablished(std::move(transport), std::move(client));

    OnCompleted(net::OK);
    // `this` is deleted.
  }
  void OnHandshakeFailed(
      const absl::optional<net::WebTransportError>& error) override {
    remote_->OnHandshakeFailed(error);

    int error_code = net::ERR_ABORTED;
    if (error.has_value()) {
      int webtransport_error_code = error.value().net_error;
      if (webtransport_error_code != net::OK)
        error_code = webtransport_error_code;
    }
    OnCompleted(error_code);
    // `this` is deleted.
  }

  // TODO(crbug.com/1240935): Implement WebRequestAPI::onHeadersReceived
  // TODO(crbug.com/1240935): Implement WebRequestAPI::onResponseStarted
  // TODO(crbug.com/1240935): Implement WebRequestAPI::onCompleted

  void OnCompleted(int error_code) {
    if (error_code != net::OK) {
      ExtensionWebRequestEventRouter::GetInstance()->OnErrorOccurred(
          browser_context_, &info_, /*started=*/true, error_code);
    }

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
  mojo::Remote<WebTransportHandshakeClient> remote_;
  mojo::Receiver<WebTransportHandshakeClient> receiver_{this};
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
  request.method = net::HttpRequestHeaders::kConnectMethod;
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

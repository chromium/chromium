// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_proxying_webtransport.h"

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/api/web_request/extension_web_request_event_router.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/api/web_request/web_request_info.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/public/mojom/web_transport.mojom.h"
#include "url/gurl.h"

namespace extensions {

namespace {

using network::mojom::WebTransportHandshakeClient;
using CreateCallback =
    content::ContentBrowserClient::WillCreateWebTransportCallback;

net::HttpRequestHeaders GetRequestHeaders() {
  // We don't attach certain headers:
  //  1. We cannot store pseudo-headers to `request_headers_` and they can be
  //     accessed via other ways, e.g., "url" for :scheme, :authority and
  //     :path.
  //  2. We don't attach the "origin" header, to be aligned with the usual
  //     loading case. Extension authors can use the "initiator" property to
  //     observe it.
  auto headers = net::HttpRequestHeaders();
  // TODO(crbug.com/40194454): Share the code with
  // DedicatedWebTransportHttp3Client::DoSendRequest.
  headers.SetHeader("sec-webtransport-http3-draft02", "1");
  return headers;
}

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
    WebRequestEventRouter::Get(browser_context_)
        ->OnRequestWillBeDestroyed(browser_context_, &info_);
  }

  void Start() {
    bool should_collapse_initiator = false;
    // Since WebTransport doesn't support redirect, 'redirect_url' is ignored
    // even if extensions assigned it.
    const int result =
        WebRequestEventRouter::Get(browser_context_)
            ->OnBeforeRequest(
                browser_context_, &info_,
                base::BindOnce(
                    &WebTransportHandshakeProxy::OnBeforeRequestCompleted,
                    base::Unretained(this)),
                &redirect_url_, &should_collapse_initiator);
    // It doesn't make sense to collapse WebTransport requests since they won't
    // be associated with a DOM element.
    CHECK(!should_collapse_initiator);

    if (result == net::ERR_IO_PENDING) {
      return;
    }

    DCHECK(result == net::OK || result == net::ERR_BLOCKED_BY_CLIENT) << result;
    OnBeforeRequestCompleted(result);
  }

  void OnBeforeRequestCompleted(int error_code) {
    if (error_code != net::OK) {
      OnError(error_code);
      // `this` is deleted.
      return;
    }

    request_headers_ = GetRequestHeaders();
    const int result =
        WebRequestEventRouter::Get(browser_context_)
            ->OnBeforeSendHeaders(
                browser_context_, &info_,
                base::BindOnce(
                    &WebTransportHandshakeProxy::OnBeforeSendHeadersCompleted,
                    base::Unretained(this)),
                &request_headers_);
    if (result == net::ERR_IO_PENDING) {
      return;
    }

    DCHECK(result == net::OK || result == net::ERR_BLOCKED_BY_CLIENT) << result;
    // See the comments in the OnBeforeSendHeadersCompleted to see why
    // we pass empty values.
    OnBeforeSendHeadersCompleted({}, {}, result);
  }

  void OnBeforeSendHeadersCompleted(
      const std::set<std::string>& removed_headers,
      const std::set<std::string>& set_headers,
      int error_code) {
    if (error_code != net::OK) {
      OnError(error_code);
      // `this` is deleted.
      return;
    }

    // We don't allow extension authors to add/remove/change request headers,
    // as that may lead to a WebTransport over HTTP/3 protocol violation. We may
    // change this policy once https://github.com/w3c/webtransport/issues/263 is
    // resolved.
    WebRequestEventRouter::Get(browser_context_)
        ->OnSendHeaders(browser_context_, &info_, GetRequestHeaders());

    // Set up proxing.
    remote_.Bind(std::move(handshake_client_));
    remote_.set_disconnect_handler(
        base::BindOnce(&WebTransportHandshakeProxy::OnError,
                       base::Unretained(this), net::ERR_ABORTED));
    std::move(create_callback_)
        .Run(receiver_.BindNewPipeAndPassRemote(), std::nullopt);
    receiver_.set_disconnect_handler(
        base::BindOnce(&WebTransportHandshakeProxy::OnError,
                       base::Unretained(this), net::ERR_ABORTED));
  }

  // WebTransportHandshakeClient implementation:
  // Proxing should be finished with either of below functions.
  void OnConnectionEstablished(
      mojo::PendingRemote<network::mojom::WebTransport> transport,
      mojo::PendingReceiver<network::mojom::WebTransportClient> client,
      const scoped_refptr<net::HttpResponseHeaders>& response_headers,
      network::mojom::WebTransportStatsPtr initial_stats) override {
    receiver_.reset();
    pending_transport_ = std::move(transport);
    pending_client_ = std::move(client);
    initial_stats_ = std::move(initial_stats);
    response_headers_ = response_headers;

    bool should_collapse_initiator = false;

    // Since WebTransport doesn't support redirect, 'redirect_url' is ignored
    // even if extensions assigned it.
    const int result =
        WebRequestEventRouter::Get(browser_context_)
            ->OnHeadersReceived(
                browser_context_, &info_,
                base::BindOnce(
                    &WebTransportHandshakeProxy::OnHeadersReceivedCompleted,
                    base::Unretained(this)),
                response_headers_.get(), &override_headers_, &redirect_url_,
                &should_collapse_initiator);

    // It doesn't make sense to collapse WebTransport requests since they won't
    // be associated with a DOM element.
    CHECK(!should_collapse_initiator);

    if (result == net::ERR_IO_PENDING) {
      return;
    }

    DCHECK(result == net::OK || result == net::ERR_BLOCKED_BY_CLIENT) << result;
    OnHeadersReceivedCompleted(result);
  }

  void OnHeadersReceivedCompleted(int error_code) {
    if (error_code != net::OK) {
      OnError(error_code);
      return;
    }

    network::mojom::URLResponseHead response;
    response.headers =
        override_headers_ ? override_headers_ : response_headers_;
    DCHECK(response.headers);
    // TODO(crbug.com/40791652): Assign actual server IP 'response';
    response.remote_endpoint = net::IPEndPoint();
    // Web transport doesn't use the http cache.
    response.was_fetched_via_cache = false;
    info_.AddResponseInfoFromResourceResponse(response);

    WebRequestEventRouter::Get(browser_context_)
        ->OnResponseStarted(browser_context_, &info_, net::OK);

    remote_->OnConnectionEstablished(
        std::move(pending_transport_), std::move(pending_client_),
        response.headers, std::move(initial_stats_));

    OnCompleted();
    // `this` is deleted.
  }

  void OnHandshakeFailed(
      const std::optional<net::WebTransportError>& error) override {
    remote_->OnHandshakeFailed(error);

    int error_code = net::ERR_ABORTED;
    if (error.has_value()) {
      error_code = error->net_error;
    }
    OnError(error_code);
    // `this` is deleted.
  }

  void OnError(int error_code) {
    DCHECK_NE(error_code, net::OK);
    if (create_callback_) {
      auto webtransport_error = network::mojom::WebTransportError::New(
          error_code, quic::QUIC_INTERNAL_ERROR, "Blocked by an extension",
          false);
      std::move(create_callback_)
          .Run(std::move(handshake_client_), std::move(webtransport_error));
    }
    WebRequestEventRouter::Get(browser_context_)
        ->OnErrorOccurred(browser_context_, &info_, /*started=*/true,
                          error_code);

    proxies_->RemoveProxy(this);
    // `this` is deleted.
  }

  void OnCompleted() {
    WebRequestEventRouter::Get(browser_context_)
        ->OnCompleted(browser_context_, &info_, net::OK);
    // Delete `this`.
    proxies_->RemoveProxy(this);
  }

 private:
  // WebRequestAPI::Proxy:
  void OnDNRExtensionUnloaded(const Extension* extension) override {
    info_.EraseDNRActionsForExtension(extension->id());
  }

  mojo::PendingRemote<WebTransportHandshakeClient> handshake_client_;
  // Weak reference to the ProxySet. This is safe as `proxies_` owns this
  // object.
  const raw_ref<WebRequestAPI::ProxySet> proxies_;
  raw_ptr<content::BrowserContext> browser_context_;
  WebRequestInfo info_;
  net::HttpRequestHeaders request_headers_;
  GURL redirect_url_;
  mojo::Remote<WebTransportHandshakeClient> remote_;
  mojo::Receiver<WebTransportHandshakeClient> receiver_{this};
  scoped_refptr<net::HttpResponseHeaders> response_headers_;
  scoped_refptr<net::HttpResponseHeaders> override_headers_;
  mojo::PendingRemote<network::mojom::WebTransport> pending_transport_;
  mojo::PendingReceiver<network::mojom::WebTransportClient> pending_client_;
  network::mojom::WebTransportStatsPtr initial_stats_;

  CreateCallback create_callback_;
};

}  // namespace

void StartWebRequestProxyingWebTransport(
    content::RenderProcessHost& render_process_host,
    int frame_routing_id,
    const GURL& url,
    const url::Origin& initiator_origin,
    mojo::PendingRemote<WebTransportHandshakeClient> handshake_client,
    int64_t request_id,
    WebRequestAPI::ProxySet& proxies,
    content::ContentBrowserClient::WillCreateWebTransportCallback callback) {
  content::BrowserContext* browser_context =
      render_process_host.GetBrowserContext();

  // Filling ResourceRequest fields required to create WebRequestInfoInitParams.
  network::ResourceRequest request;
  request.method = net::HttpRequestHeaders::kConnectMethod;
  request.url = url;
  request.request_initiator = initiator_origin;

  const int process_id = render_process_host.GetID();

  WebRequestInfoInitParams params =
      WebRequestInfoInitParams(request_id, process_id, frame_routing_id,
                               /*navigation_ui_data=*/nullptr, request,
                               /*is_download=*/false,
                               /*is_async=*/true,
                               /*is_service_worker_script=*/false,
                               /*navigation_id=*/std::nullopt);
  params.web_request_type = WebRequestResourceType::WEB_TRANSPORT;

  auto proxy = std::make_unique<WebTransportHandshakeProxy>(
      std::move(handshake_client), proxies, browser_context, std::move(params),
      std::move(callback));
  auto* raw_proxy = proxy.get();
  proxies.AddProxy(std::move(proxy));
  raw_proxy->Start();
}

}  // namespace extensions

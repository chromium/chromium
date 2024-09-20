// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_proxying_websocket.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/web_request/extension_web_request_event_router.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/api/web_request/web_request_api.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "net/base/ip_endpoint.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_util.h"

namespace extensions {
namespace {

// This shutdown notifier makes sure the proxy is destroyed if an incognito
// browser context is destroyed. This is needed because WebRequestAPI only
// clears the proxies when the original browser context is destroyed.
class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  ShutdownNotifierFactory(const ShutdownNotifierFactory&) = delete;
  ShutdownNotifierFactory& operator=(const ShutdownNotifierFactory&) = delete;

  static ShutdownNotifierFactory* GetInstance() {
    static base::NoDestructor<ShutdownNotifierFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "WebRequestProxyingWebSocket") {
    DependsOn(PermissionHelper::GetFactoryInstance());
  }
  ~ShutdownNotifierFactory() override {}

  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override {
    return ExtensionsBrowserClient::Get()->GetContextOwnInstance(
        context, /*force_guest_profile=*/true);
  }
};

}  // namespace

WebRequestProxyingWebSocket::WebRequestProxyingWebSocket(
    WebSocketFactory factory,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client,
    bool has_extra_headers,
    int process_id,
    int render_frame_id,
    content::BrowserContext* browser_context,
    WebRequestAPI::RequestIDGenerator* request_id_generator,
    WebRequestAPI::ProxySet* proxies)
    : factory_(std::move(factory)),
      browser_context_(browser_context),
      forwarding_handshake_client_(std::move(handshake_client)),
      request_headers_(request.headers),
      response_(network::mojom::URLResponseHead::New()),
      has_extra_headers_(has_extra_headers),
      info_(WebRequestInfoInitParams(
          request_id_generator->Generate(MSG_ROUTING_NONE, 0),
          process_id,
          render_frame_id,
          nullptr,
          request,
          /*is_download=*/false,
          /*is_async=*/true,
          /*is_service_worker_script=*/false,
          /*navigation_id=*/std::nullopt)),
      proxies_(proxies) {
  // base::Unretained is safe here because the callback will be canceled when
  // |shutdown_notifier_subscription_| is destroyed, and |proxies_| owns this.
  shutdown_notifier_subscription_ =
      ShutdownNotifierFactory::GetInstance()
          ->Get(browser_context)
          ->Subscribe(base::BindRepeating(&WebRequestAPI::ProxySet::RemoveProxy,
                                          base::Unretained(proxies_), this));
}

WebRequestProxyingWebSocket::~WebRequestProxyingWebSocket() {
  // This is important to ensure that no outstanding blocking requests continue
  // to reference state owned by this object.
  WebRequestEventRouter::Get(browser_context_)
      ->OnRequestWillBeDestroyed(browser_context_, &info_);
  if (on_before_send_headers_callback_) {
    std::move(on_before_send_headers_callback_)
        .Run(net::ERR_ABORTED, std::nullopt);
  }
  if (on_headers_received_callback_) {
    std::move(on_headers_received_callback_)
        .Run(net::ERR_ABORTED, std::nullopt, std::nullopt);
  }
}

void WebRequestProxyingWebSocket::Start() {
  // If the header client will be used, we start the request immediately, and
  // OnBeforeSendHeaders and OnSendHeaders will be handled there. Otherwise,
  // send these events before the request starts.
  base::RepeatingCallback<void(int)> continuation;
  if (has_extra_headers_) {
    continuation = base::BindRepeating(
        &WebRequestProxyingWebSocket::ContinueToStartRequest,
        weak_factory_.GetWeakPtr());
  } else {
    continuation = base::BindRepeating(
        &WebRequestProxyingWebSocket::OnBeforeRequestComplete,
        weak_factory_.GetWeakPtr());
  }

  // TODO(yhirano): Consider having throttling here (probably with aligned with
  // WebRequestProxyingURLLoaderFactory).
  bool should_collapse_initiator = false;
  int result =
      WebRequestEventRouter::Get(browser_context_)
          ->OnBeforeRequest(browser_context_, &info_, continuation,
                            &redirect_url_, &should_collapse_initiator);

  // It doesn't make sense to collapse WebSocket requests since they won't be
  // associated with a DOM element.
  CHECK(!should_collapse_initiator);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  if (result == net::ERR_IO_PENDING) {
    return;
  }

  DCHECK_EQ(net::OK, result);
  continuation.Run(net::OK);
}

void WebRequestProxyingWebSocket::OnOpeningHandshakeStarted(
    network::mojom::WebSocketHandshakeRequestPtr request) {
  DCHECK(forwarding_handshake_client_);
  forwarding_handshake_client_->OnOpeningHandshakeStarted(std::move(request));
}

void WebRequestProxyingWebSocket::OnFailure(const std::string&,
                                            int net_error,
                                            int response_code) {}

void WebRequestProxyingWebSocket::ContinueToHeadersReceived() {
  auto continuation = base::BindRepeating(
      &WebRequestProxyingWebSocket::OnHeadersReceivedComplete,
      weak_factory_.GetWeakPtr());
  bool should_collapse_initiator = false;
  int result =
      WebRequestEventRouter::Get(browser_context_)
          ->OnHeadersReceived(browser_context_, &info_, continuation,
                              response_->headers.get(), &override_headers_,
                              &redirect_url_, &should_collapse_initiator);

  // It doesn't make sense to collapse WebSocket requests since they won't be
  // associated with a DOM element.
  CHECK(!should_collapse_initiator);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  PauseIncomingMethodCallProcessing();
  if (result == net::ERR_IO_PENDING) {
    return;
  }

  DCHECK_EQ(net::OK, result);
  OnHeadersReceivedComplete(net::OK);
}

void WebRequestProxyingWebSocket::OnConnectionEstablished(
    mojo::PendingRemote<network::mojom::WebSocket> websocket,
    mojo::PendingReceiver<network::mojom::WebSocketClient> client_receiver,
    network::mojom::WebSocketHandshakeResponsePtr response,
    mojo::ScopedDataPipeConsumerHandle readable,
    mojo::ScopedDataPipeProducerHandle writable) {
  DCHECK(forwarding_handshake_client_);
  DCHECK(!is_done_);
  is_done_ = true;
  websocket_ = std::move(websocket);
  client_receiver_ = std::move(client_receiver);
  handshake_response_ = std::move(response);
  readable_ = std::move(readable);
  writable_ = std::move(writable);

  response_->remote_endpoint = handshake_response_->remote_endpoint;

  // response_->headers will be set in OnBeforeSendHeaders if
  // |receiver_as_header_client_| is set.
  if (receiver_as_header_client_.is_bound()) {
    ContinueToCompleted();
    return;
  }

  response_->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(base::StringPrintf(
          "HTTP/%d.%d %d %s", handshake_response_->http_version.major_value(),
          handshake_response_->http_version.minor_value(),
          handshake_response_->status_code,
          handshake_response_->status_text.c_str()));
  for (const auto& header : handshake_response_->headers) {
    response_->headers->AddHeader(header->name, header->value);
  }

  ContinueToHeadersReceived();
}

void WebRequestProxyingWebSocket::ContinueToCompleted() {
  DCHECK(forwarding_handshake_client_);
  DCHECK(is_done_);
  WebRequestEventRouter::Get(browser_context_)
      ->OnCompleted(browser_context_, &info_, net::ERR_WS_UPGRADE);
  forwarding_handshake_client_->OnConnectionEstablished(
      std::move(websocket_), std::move(client_receiver_),
      std::move(handshake_response_), std::move(readable_),
      std::move(writable_));

  // Deletes |this|.
  proxies_->RemoveProxy(this);
}

void WebRequestProxyingWebSocket::OnAuthRequired(
    const net::AuthChallengeInfo& auth_info,
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    const net::IPEndPoint& remote_endpoint,
    OnAuthRequiredCallback callback) {
  if (!callback) {
    OnError(net::ERR_FAILED);
    return;
  }

  response_->headers = headers;
  response_->remote_endpoint = remote_endpoint;
  auth_required_callback_ = std::move(callback);

  auto continuation = base::BindRepeating(
      &WebRequestProxyingWebSocket::OnHeadersReceivedCompleteForAuth,
      weak_factory_.GetWeakPtr(), auth_info);
  bool should_collapse_initiator = false;
  int result =
      WebRequestEventRouter::Get(browser_context_)
          ->OnHeadersReceived(browser_context_, &info_, continuation,
                              response_->headers.get(), &override_headers_,
                              &redirect_url_, &should_collapse_initiator);

  // It doesn't make sense to collapse WebSocket requests since they won't be
  // associated with a DOM element.
  CHECK(!should_collapse_initiator);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  PauseIncomingMethodCallProcessing();
  if (result == net::ERR_IO_PENDING) {
    return;
  }

  DCHECK_EQ(net::OK, result);
  OnHeadersReceivedCompleteForAuth(auth_info, net::OK);
}

void WebRequestProxyingWebSocket::OnBeforeSendHeaders(
    const net::HttpRequestHeaders& headers,
    OnBeforeSendHeadersCallback callback) {
  DCHECK(receiver_as_header_client_.is_bound());

  request_headers_ = headers;
  on_before_send_headers_callback_ = std::move(callback);
  OnBeforeRequestComplete(net::OK);
}

void WebRequestProxyingWebSocket::OnHeadersReceived(
    const std::string& headers,
    const net::IPEndPoint& endpoint,
    OnHeadersReceivedCallback callback) {
  DCHECK(receiver_as_header_client_.is_bound());

  on_headers_received_callback_ = std::move(callback);
  response_->headers = base::MakeRefCounted<net::HttpResponseHeaders>(headers);

  ContinueToHeadersReceived();
}

void WebRequestProxyingWebSocket::OnDNRExtensionUnloaded(
    const Extension* extension) {
  info_.EraseDNRActionsForExtension(extension->id());
}

void WebRequestProxyingWebSocket::StartProxying(
    WebSocketFactory factory,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const std::optional<std::string>& user_agent,
    mojo::PendingRemote<network::mojom::WebSocketHandshakeClient>
        handshake_client,
    bool has_extra_headers,
    int process_id,
    int render_frame_id,
    WebRequestAPI::RequestIDGenerator* request_id_generator,
    const url::Origin& origin,
    content::BrowserContext* browser_context,
    WebRequestAPI::ProxySet* proxies) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  network::ResourceRequest request;
  request.url = url;
  request.site_for_cookies = site_for_cookies;
  if (user_agent) {
    request.headers.SetHeader(net::HttpRequestHeaders::kUserAgent, *user_agent);
  }
  request.request_initiator = origin;

  auto proxy = std::make_unique<WebRequestProxyingWebSocket>(
      std::move(factory), request, std::move(handshake_client),
      has_extra_headers, process_id, render_frame_id, browser_context,
      request_id_generator, proxies);

  auto* raw_proxy = proxy.get();
  proxies->AddProxy(std::move(proxy));
  raw_proxy->Start();
}

void WebRequestProxyingWebSocket::OnBeforeRequestComplete(int error_code) {
  DCHECK(receiver_as_header_client_.is_bound() ||
         !receiver_as_handshake_client_.is_bound());
  DCHECK(info_.url.SchemeIsWSOrWSS());
  if (error_code != net::OK) {
    OnError(error_code);
    return;
  }

  auto continuation = base::BindRepeating(
      &WebRequestProxyingWebSocket::OnBeforeSendHeadersComplete,
      weak_factory_.GetWeakPtr());

  int result = WebRequestEventRouter::Get(browser_context_)
                   ->OnBeforeSendHeaders(browser_context_, &info_, continuation,
                                         &request_headers_);

  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    OnError(result);
    return;
  }

  if (result == net::ERR_IO_PENDING) {
    return;
  }

  DCHECK_EQ(net::OK, result);
  OnBeforeSendHeadersComplete(std::set<std::string>(), std::set<std::string>(),
                              net::OK);
}

void WebRequestProxyingWebSocket::OnBeforeSendHeadersComplete(
    const std::set<std::string>& removed_headers,
    const std::set<std::string>& set_headers,
    int error_code) {
  DCHECK(receiver_as_header_client_.is_bound() ||
         !receiver_as_handshake_client_.is_bound());
  if (error_code != net::OK) {
    OnError(error_code);
    return;
  }

  if (receiver_as_header_client_.is_bound()) {
    DCHECK(on_before_send_headers_callback_);
    std::move(on_before_send_headers_callback_)
        .Run(error_code, request_headers_);
  }

  WebRequestEventRouter::Get(browser_context_)
      ->OnSendHeaders(browser_context_, &info_, request_headers_);

  if (!receiver_as_header_client_.is_bound()) {
    ContinueToStartRequest(net::OK);
  }
}

void WebRequestProxyingWebSocket::ContinueToStartRequest(int error_code) {
  if (error_code != net::OK) {
    OnError(error_code);
    return;
  }

  base::flat_set<std::string> used_header_names;
  std::vector<network::mojom::HttpHeaderPtr> additional_headers;
  for (net::HttpRequestHeaders::Iterator it(request_headers_); it.GetNext();) {
    additional_headers.push_back(
        network::mojom::HttpHeader::New(it.name(), it.value()));
    used_header_names.insert(base::ToLowerASCII(it.name()));
  }
  for (const auto& header : additional_headers_) {
    if (!used_header_names.contains(base::ToLowerASCII(header->name))) {
      additional_headers.push_back(
          network::mojom::HttpHeader::New(header->name, header->value));
    }
  }

  mojo::PendingRemote<network::mojom::TrustedHeaderClient>
      trusted_header_client = mojo::NullRemote();
  if (has_extra_headers_) {
    trusted_header_client =
        receiver_as_header_client_.BindNewPipeAndPassRemote();
  }

  std::move(factory_).Run(
      info_.url, std::move(additional_headers),
      receiver_as_handshake_client_.BindNewPipeAndPassRemote(),
      receiver_as_auth_handler_.BindNewPipeAndPassRemote(),
      std::move(trusted_header_client));

  // Here we detect mojo connection errors on |receiver_as_handshake_client_|.
  // See also CreateWebSocket in
  // //network/services/public/mojom/network_context.mojom.
  receiver_as_handshake_client_.set_disconnect_with_reason_handler(
      base::BindOnce(
          &WebRequestProxyingWebSocket::OnMojoConnectionErrorWithCustomReason,
          base::Unretained(this)));
  forwarding_handshake_client_.set_disconnect_handler(
      base::BindOnce(&WebRequestProxyingWebSocket::OnMojoConnectionError,
                     base::Unretained(this)));
}

void WebRequestProxyingWebSocket::OnHeadersReceivedComplete(int error_code) {
  if (error_code != net::OK) {
    OnError(error_code);
    return;
  }

  if (on_headers_received_callback_) {
    std::optional<std::string> headers;
    if (override_headers_) {
      headers = override_headers_->raw_headers();
    }
    std::move(on_headers_received_callback_)
        .Run(net::OK, headers, std::nullopt);
  }

  if (override_headers_) {
    response_->headers = override_headers_;
    override_headers_ = nullptr;
  }

  ResumeIncomingMethodCallProcessing();
  info_.AddResponseInfoFromResourceResponse(*response_);
  WebRequestEventRouter::Get(browser_context_)
      ->OnResponseStarted(browser_context_, &info_, net::OK);

  if (!receiver_as_header_client_.is_bound()) {
    ContinueToCompleted();
  }
}

void WebRequestProxyingWebSocket::OnAuthRequiredComplete(
    WebRequestEventRouter::AuthRequiredResponse rv) {
  DCHECK(auth_required_callback_);
  ResumeIncomingMethodCallProcessing();
  switch (rv) {
    case WebRequestEventRouter::AuthRequiredResponse::
        AUTH_REQUIRED_RESPONSE_NO_ACTION:
    case WebRequestEventRouter::AuthRequiredResponse::
        AUTH_REQUIRED_RESPONSE_CANCEL_AUTH:
      std::move(auth_required_callback_).Run(std::nullopt);
      break;

    case WebRequestEventRouter::AuthRequiredResponse::
        AUTH_REQUIRED_RESPONSE_SET_AUTH:
      std::move(auth_required_callback_).Run(auth_credentials_);
      break;
    case WebRequestEventRouter::AuthRequiredResponse::
        AUTH_REQUIRED_RESPONSE_IO_PENDING:
      NOTREACHED_IN_MIGRATION();
      break;
  }
}

void WebRequestProxyingWebSocket::OnHeadersReceivedCompleteForAuth(
    const net::AuthChallengeInfo& auth_info,
    int rv) {
  if (rv != net::OK) {
    OnError(rv);
    return;
  }
  ResumeIncomingMethodCallProcessing();
  info_.AddResponseInfoFromResourceResponse(*response_);

  auto continuation =
      base::BindRepeating(&WebRequestProxyingWebSocket::OnAuthRequiredComplete,
                          weak_factory_.GetWeakPtr());
  auto auth_rv =
      WebRequestEventRouter::Get(browser_context_)
          ->OnAuthRequired(browser_context_, &info_, auth_info,
                           std::move(continuation), &auth_credentials_);
  PauseIncomingMethodCallProcessing();
  if (auth_rv == WebRequestEventRouter::AuthRequiredResponse::
                     AUTH_REQUIRED_RESPONSE_IO_PENDING) {
    return;
  }

  OnAuthRequiredComplete(auth_rv);
}

void WebRequestProxyingWebSocket::PauseIncomingMethodCallProcessing() {
  receiver_as_handshake_client_.Pause();
  receiver_as_auth_handler_.Pause();
  if (receiver_as_header_client_.is_bound()) {
    receiver_as_header_client_.Pause();
  }
}

void WebRequestProxyingWebSocket::ResumeIncomingMethodCallProcessing() {
  receiver_as_handshake_client_.Resume();
  receiver_as_auth_handler_.Resume();
  if (receiver_as_header_client_.is_bound()) {
    receiver_as_header_client_.Resume();
  }
}

void WebRequestProxyingWebSocket::OnError(int error_code) {
  if (!is_done_) {
    is_done_ = true;
    WebRequestEventRouter::Get(browser_context_)
        ->OnErrorOccurred(browser_context_, &info_, /*started=*/true,
                          error_code);
  }

  // Deletes |this|.
  proxies_->RemoveProxy(this);
}

void WebRequestProxyingWebSocket::OnMojoConnectionErrorWithCustomReason(
    uint32_t custom_reason,
    const std::string& description) {
  // Here we want to nofiy the custom reason to the client, which is why
  // we reset |forwarding_handshake_client_| manually.
  forwarding_handshake_client_.ResetWithReason(custom_reason, description);
  OnError(net::ERR_FAILED);
  // Deletes |this|.
}

void WebRequestProxyingWebSocket::OnMojoConnectionError() {
  OnError(net::ERR_FAILED);
  // Deletes |this|.
}

// static
void WebRequestProxyingWebSocket::EnsureAssociatedFactoryBuilt() {
  ShutdownNotifierFactory::GetInstance();
}

}  // namespace extensions

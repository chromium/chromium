// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_proxying_url_loader_factory.h"

#include <utility>

#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/common/url_utils.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"

namespace extensions {

WebRequestProxyingURLLoaderFactory::InProgressRequest::InProgressRequest(
    WebRequestProxyingURLLoaderFactory* factory,
    uint64_t request_id,
    int32_t network_service_request_id,
    int32_t routing_id,
    uint32_t options,
    const network::ResourceRequest& request,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    network::mojom::URLLoaderRequest loader_request,
    network::mojom::URLLoaderClientPtr client)
    : factory_(factory),
      request_(request),
      request_id_(request_id),
      network_service_request_id_(network_service_request_id),
      routing_id_(routing_id),
      options_(options),
      traffic_annotation_(traffic_annotation),
      proxied_loader_binding_(this, std::move(loader_request)),
      target_client_(std::move(client)),
      proxied_client_binding_(this),
      weak_factory_(this) {
  // If there is a client error, clean up the request.
  target_client_.set_connection_error_handler(base::BindOnce(
      &WebRequestProxyingURLLoaderFactory::InProgressRequest::OnRequestError,
      weak_factory_.GetWeakPtr(),
      network::URLLoaderCompletionStatus(net::ERR_ABORTED)));
}

WebRequestProxyingURLLoaderFactory::InProgressRequest::~InProgressRequest() {
  // This is important to ensure that no outstanding blocking requests continue
  // to reference state owned by this object.
  if (info_) {
    ExtensionWebRequestEventRouter::GetInstance()->OnRequestWillBeDestroyed(
        factory_->browser_context_, &info_.value());
  }
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::Restart() {
  request_completed_ = false;
  // Derive a new WebRequestInfo value any time |Restart()| is called, because
  // the details in |request_| may have changed e.g. if we've been redirected.
  info_.emplace(
      request_id_, factory_->render_process_id_, request_.render_frame_id,
      factory_->navigation_ui_data_ ? factory_->navigation_ui_data_->DeepCopy()
                                    : nullptr,
      routing_id_, factory_->resource_context_, request_,
      !(options_ & network::mojom::kURLLoadOptionSynchronous));

  auto continuation =
      base::BindRepeating(&InProgressRequest::ContinueToBeforeSendHeaders,
                          weak_factory_.GetWeakPtr());
  redirect_url_ = GURL();
  bool should_collapse_initiator = false;
  int result = ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRequest(
      factory_->browser_context_, factory_->info_map_, &info_.value(),
      continuation, &redirect_url_, &should_collapse_initiator);
  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    // The request was cancelled synchronously. Dispatch an error notification
    // and terminate the request.
    network::URLLoaderCompletionStatus status(result);
    if (should_collapse_initiator) {
      status.extended_error_code = static_cast<int>(
          blink::ResourceRequestBlockedReason::kCollapsedByClient);
    }
    OnRequestError(status);
    return;
  }

  if (result == net::ERR_IO_PENDING) {
    // One or more listeners is blocking, so the request must be paused until
    // they respond. |continuation| above will be invoked asynchronously to
    // continue or cancel the request.
    //
    // We pause the binding here to prevent further client message processing.
    if (proxied_client_binding_.is_bound())
      proxied_client_binding_.PauseIncomingMethodCallProcessing();
    return;
  }
  DCHECK_EQ(net::OK, result);

  ContinueToBeforeSendHeaders(net::OK);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::FollowRedirect(
    const base::Optional<std::vector<std::string>>&
        to_be_removed_request_headers,
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  if (to_be_removed_request_headers) {
    for (const std::string& header : *to_be_removed_request_headers)
      request_.headers.RemoveHeader(header);
  }

  if (modified_request_headers)
    request_.headers.MergeFrom(*modified_request_headers);

  if (target_loader_.is_bound()) {
    target_loader_->FollowRedirect(to_be_removed_request_headers,
                                   modified_request_headers);
  }

  Restart();
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    ProceedWithResponse() {
  if (target_loader_.is_bound())
    target_loader_->ProceedWithResponse();
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::SetPriority(
    net::RequestPriority priority,
    int32_t intra_priority_value) {
  if (target_loader_.is_bound())
    target_loader_->SetPriority(priority, intra_priority_value);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    PauseReadingBodyFromNet() {
  if (target_loader_.is_bound())
    target_loader_->PauseReadingBodyFromNet();
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    ResumeReadingBodyFromNet() {
  if (target_loader_.is_bound())
    target_loader_->ResumeReadingBodyFromNet();
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::OnReceiveResponse(
    const network::ResourceResponseHead& head) {
  current_response_ = head;
  HandleResponseOrRedirectHeaders(
      base::BindRepeating(&InProgressRequest::ContinueToResponseStarted,
                          weak_factory_.GetWeakPtr()));
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const network::ResourceResponseHead& head) {
  if (redirect_url_ != redirect_info.new_url &&
      !IsRedirectSafe(request_.url, redirect_info.new_url)) {
    OnRequestError(
        network::URLLoaderCompletionStatus(net::ERR_UNSAFE_REDIRECT));
    return;
  }

  current_response_ = head;
  HandleResponseOrRedirectHeaders(
      base::BindRepeating(&InProgressRequest::ContinueToBeforeRedirect,
                          weak_factory_.GetWeakPtr(), redirect_info));
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  target_client_->OnUploadProgress(current_position, total_size,
                                   std::move(callback));
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    OnReceiveCachedMetadata(const std::vector<uint8_t>& data) {
  target_client_->OnReceiveCachedMetadata(data);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    OnTransferSizeUpdated(int32_t transfer_size_diff) {
  target_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    OnStartLoadingResponseBody(mojo::ScopedDataPipeConsumerHandle body) {
  target_client_->OnStartLoadingResponseBody(std::move(body));
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::OnComplete(
    const network::URLLoaderCompletionStatus& status) {
  if (status.error_code != net::OK) {
    OnRequestError(status);
    return;
  }

  target_client_->OnComplete(status);
  ExtensionWebRequestEventRouter::GetInstance()->OnCompleted(
      factory_->browser_context_, factory_->info_map_, &info_.value(),
      status.error_code);

  // Deletes |this|.
  factory_->RemoveRequest(network_service_request_id_, request_id_);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::HandleAuthRequest(
    net::AuthChallengeInfo* auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    WebRequestAPI::AuthRequestCallback callback) {
  DCHECK(!auth_credentials_);

  // We first need to simulate |onHeadersReceived| for the response headers
  // which indicated a need to authenticate.
  network::ResourceResponseHead head;
  head.headers = response_headers;
  current_response_ = head;
  HandleResponseOrRedirectHeaders(base::BindRepeating(base::BindRepeating(
      &InProgressRequest::ContinueAuthRequest, weak_factory_.GetWeakPtr(),
      base::RetainedRef(auth_info), base::Passed(&callback))));
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToBeforeSendHeaders(int error_code) {
  if (error_code != net::OK) {
    OnRequestError(network::URLLoaderCompletionStatus(error_code));
    return;
  }

  if (!redirect_url_.is_empty()) {
    constexpr int kInternalRedirectStatusCode = 307;

    net::RedirectInfo redirect_info;
    redirect_info.status_code = kInternalRedirectStatusCode;
    redirect_info.new_method = request_.method;
    redirect_info.new_url = redirect_url_;
    redirect_info.new_site_for_cookies = redirect_url_;

    network::ResourceResponseHead head;
    std::string headers = base::StringPrintf(
        "HTTP/1.1 %i Internal Redirect\n"
        "Location: %s\n"
        "Non-Authoritative-Reason: WebRequest API\n\n",
        kInternalRedirectStatusCode, redirect_url_.spec().c_str());
    std::string http_origin;
    if (request_.headers.GetHeader("Origin", &http_origin)) {
      // If this redirect is used in a cross-origin request, add CORS headers to
      // make sure that the redirect gets through. Note that the destination URL
      // is still subject to the usual CORS policy, i.e. the resource will only
      // be available to web pages if the server serves the response with the
      // required CORS response headers.
      // Matches the behavior in url_request_redirect_job.cc.
      headers += base::StringPrintf(
          "\n"
          "Access-Control-Allow-Origin: %s\n"
          "Access-Control-Allow-Credentials: true",
          http_origin.c_str());
    }
    head.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.length()));
    head.encoded_data_length = 0;

    current_response_ = head;
    ContinueToBeforeRedirect(redirect_info, net::OK);
    return;
  }

  if (proxied_client_binding_.is_bound())
    proxied_client_binding_.ResumeIncomingMethodCallProcessing();

  if (request_.url.SchemeIsHTTPOrHTTPS()) {
    // NOTE: While it does not appear to be documented (and in fact it may be
    // intuitive), |onBeforeSendHeaders| is only dispatched for HTTP and HTTPS
    // requests.

    auto continuation = base::BindRepeating(
        &InProgressRequest::ContinueToSendHeaders, weak_factory_.GetWeakPtr());
    int result =
        ExtensionWebRequestEventRouter::GetInstance()->OnBeforeSendHeaders(
            factory_->browser_context_, factory_->info_map_, &info_.value(),
            continuation, &request_.headers);

    if (result == net::ERR_BLOCKED_BY_CLIENT) {
      // The request was cancelled synchronously. Dispatch an error notification
      // and terminate the request.
      OnRequestError(network::URLLoaderCompletionStatus(result));
      return;
    }

    if (result == net::ERR_IO_PENDING) {
      // One or more listeners is blocking, so the request must be paused until
      // they respond. |continuation| above will be invoked asynchronously to
      // continue or cancel the request.
      //
      // We pause the binding here to prevent further client message processing.
      if (proxied_client_binding_.is_bound())
        proxied_client_binding_.PauseIncomingMethodCallProcessing();
      return;
    }
    DCHECK_EQ(net::OK, result);
  }

  ContinueToSendHeaders(net::OK);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToSendHeaders(int error_code) {
  if (error_code != net::OK) {
    OnRequestError(network::URLLoaderCompletionStatus(error_code));
    return;
  }

  if (proxied_client_binding_.is_bound())
    proxied_client_binding_.ResumeIncomingMethodCallProcessing();

  if (request_.url.SchemeIsHTTPOrHTTPS()) {
    // NOTE: While it does not appear to be documented (and in fact it may be
    // intuitive), |onSendHeaders| is only dispatched for HTTP and HTTPS
    // requests.
    ExtensionWebRequestEventRouter::GetInstance()->OnSendHeaders(
        factory_->browser_context_, factory_->info_map_, &info_.value(),
        request_.headers);
  }

  if (!target_loader_.is_bound() && factory_->target_factory_.is_bound()) {
    // No extensions have cancelled us up to this point, so it's now OK to
    // initiate the real network request.
    network::mojom::URLLoaderClientPtr proxied_client;
    proxied_client_binding_.Bind(mojo::MakeRequest(&proxied_client));
    factory_->target_factory_->CreateLoaderAndStart(
        mojo::MakeRequest(&target_loader_), info_->routing_id,
        network_service_request_id_, options_, request_,
        std::move(proxied_client), traffic_annotation_);
  }

  // From here the lifecycle of this request is driven by subsequent events on
  // either |proxy_loader_binding_| or |proxy_client_binding_|.
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::ContinueAuthRequest(
    net::AuthChallengeInfo* auth_info,
    WebRequestAPI::AuthRequestCallback callback,
    int error_code) {
  if (error_code != net::OK) {
    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                             base::BindOnce(std::move(callback), base::nullopt,
                                            true /* should_cancel */));
    return;
  }

  info_->AddResponseInfoFromResourceResponse(current_response_);
  auto continuation =
      base::BindRepeating(&InProgressRequest::OnAuthRequestHandled,
                          weak_factory_.GetWeakPtr(), base::Passed(&callback));

  auth_credentials_.emplace();
  net::NetworkDelegate::AuthRequiredResponse response =
      ExtensionWebRequestEventRouter::GetInstance()->OnAuthRequired(
          factory_->browser_context_, factory_->info_map_, &info_.value(),
          *auth_info, continuation, &auth_credentials_.value());

  // At least one extension has a blocking handler for this request, so we'll
  // just wait for them to finish. |OnAuthRequestHandled()| will be invoked
  // eventually.
  if (response == net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_IO_PENDING)
    return;

  // We're not touching this auth request. Let the default browser behavior
  // proceed.
  DCHECK_EQ(response, net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION);
  continuation.Run(response);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    OnAuthRequestHandled(WebRequestAPI::AuthRequestCallback callback,
                         net::NetworkDelegate::AuthRequiredResponse response) {
  if (proxied_client_binding_.is_bound())
    proxied_client_binding_.ResumeIncomingMethodCallProcessing();

  base::OnceClosure completion;
  switch (response) {
    case net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_NO_ACTION:
      // We're not touching this auth request. Let the default browser behavior
      // proceed.
      completion = base::BindOnce(std::move(callback), base::nullopt,
                                  false /* should_cancel */);
      break;
    case net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_SET_AUTH:
      completion =
          base::BindOnce(std::move(callback), auth_credentials_.value(),
                         false /* should_cancel */);
      break;
    case net::NetworkDelegate::AUTH_REQUIRED_RESPONSE_CANCEL_AUTH:
      completion = base::BindOnce(std::move(callback), base::nullopt,
                                  true /* should_cancel */);
      break;
    default:
      NOTREACHED();
      return;
  }

  base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                           std::move(completion));
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToResponseStarted(int error_code) {
  if (error_code != net::OK) {
    OnRequestError(network::URLLoaderCompletionStatus(error_code));
    return;
  }

  if (override_headers_)
    current_response_.headers = override_headers_;

  std::string redirect_location;
  if (override_headers_ && override_headers_->IsRedirect(&redirect_location)) {
    // The response headers may have been overridden by an |onHeadersReceived|
    // handler and may have been changed to a redirect. We handle that here
    // instead of acting like regular request completion.
    //
    // Note that we can't actually change how the Network Service handles the
    // original request at this point, so our "redirect" is really just
    // generating an artificial |onBeforeRedirect| event and starting a new
    // request to the Network Service. Our client shouldn't know the difference.
    GURL new_url(redirect_location);

    net::RedirectInfo redirect_info;
    redirect_info.status_code = override_headers_->response_code();
    redirect_info.new_method = request_.method;
    redirect_info.new_url = new_url;
    redirect_info.new_site_for_cookies = new_url;

    // These will get re-bound if a new request is initiated by
    // |FollowRedirect()|.
    proxied_client_binding_.Close();
    target_loader_.reset();

    ContinueToBeforeRedirect(redirect_info, net::OK);
    return;
  }

  info_->AddResponseInfoFromResourceResponse(current_response_);

  proxied_client_binding_.ResumeIncomingMethodCallProcessing();

  ExtensionWebRequestEventRouter::GetInstance()->OnResponseStarted(
      factory_->browser_context_, factory_->info_map_, &info_.value(), net::OK);
  target_client_->OnReceiveResponse(current_response_);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToBeforeRedirect(const net::RedirectInfo& redirect_info,
                             int error_code) {
  if (error_code != net::OK) {
    OnRequestError(network::URLLoaderCompletionStatus(error_code));
    return;
  }

  info_->AddResponseInfoFromResourceResponse(current_response_);

  if (proxied_client_binding_.is_bound())
    proxied_client_binding_.ResumeIncomingMethodCallProcessing();

  ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRedirect(
      factory_->browser_context_, factory_->info_map_, &info_.value(),
      redirect_info.new_url);
  target_client_->OnReceiveRedirect(redirect_info, current_response_);
  request_.url = redirect_info.new_url;
  request_.method = redirect_info.new_method;
  request_.site_for_cookies = redirect_info.new_site_for_cookies;
  request_.referrer = GURL(redirect_info.new_referrer);
  request_.referrer_policy = redirect_info.new_referrer_policy;
  request_completed_ = true;
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    HandleResponseOrRedirectHeaders(
        const net::CompletionCallback& continuation) {
  override_headers_ = nullptr;
  redirect_url_ = GURL();
  if (request_.url.SchemeIsHTTPOrHTTPS()) {
    int result =
        ExtensionWebRequestEventRouter::GetInstance()->OnHeadersReceived(
            factory_->browser_context_, factory_->info_map_, &info_.value(),
            continuation, current_response_.headers.get(), &override_headers_,
            &redirect_url_);
    if (result == net::ERR_BLOCKED_BY_CLIENT) {
      OnRequestError(network::URLLoaderCompletionStatus(result));
      return;
    }

    if (result == net::ERR_IO_PENDING) {
      // One or more listeners is blocking, so the request must be paused until
      // they respond. |continuation| above will be invoked asynchronously to
      // continue or cancel the request.
      //
      // We pause the binding here to prevent further client message processing.
      proxied_client_binding_.PauseIncomingMethodCallProcessing();
      return;
    }

    DCHECK_EQ(net::OK, result);
  }

  continuation.Run(net::OK);
}
void WebRequestProxyingURLLoaderFactory::InProgressRequest::OnRequestError(
    const network::URLLoaderCompletionStatus& status) {
  if (!request_completed_) {
    target_client_->OnComplete(status);
    ExtensionWebRequestEventRouter::GetInstance()->OnErrorOccurred(
        factory_->browser_context_, factory_->info_map_, &info_.value(),
        true /* started */, status.error_code);
  }

  // Deletes |this|.
  factory_->RemoveRequest(network_service_request_id_, request_id_);
}

// Determines whether it is safe to redirect from |from_url| to |to_url|.
bool WebRequestProxyingURLLoaderFactory::InProgressRequest::IsRedirectSafe(
    const GURL& from_url,
    const GURL& to_url) {
  if (to_url.SchemeIs(extensions::kExtensionScheme)) {
    const Extension* extension =
        factory_->info_map_->extensions().GetByID(to_url.host());
    if (!extension)
      return false;
    return WebAccessibleResourcesInfo::IsResourceWebAccessible(extension,
                                                               to_url.path());
  }
  return content::IsSafeRedirectTarget(from_url, to_url);
}

WebRequestProxyingURLLoaderFactory::WebRequestProxyingURLLoaderFactory(
    void* browser_context,
    content::ResourceContext* resource_context,
    int render_process_id,
    scoped_refptr<WebRequestAPI::RequestIDGenerator> request_id_generator,
    std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data,
    InfoMap* info_map,
    network::mojom::URLLoaderFactoryRequest loader_request,
    network::mojom::URLLoaderFactoryPtrInfo target_factory_info,
    WebRequestAPI::ProxySet* proxies)
    : browser_context_(browser_context),
      resource_context_(resource_context),
      render_process_id_(render_process_id),
      request_id_generator_(std::move(request_id_generator)),
      navigation_ui_data_(std::move(navigation_ui_data)),
      info_map_(info_map),
      proxies_(proxies),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  target_factory_.Bind(std::move(target_factory_info));
  target_factory_.set_connection_error_handler(
      base::BindOnce(&WebRequestProxyingURLLoaderFactory::OnTargetFactoryError,
                     base::Unretained(this)));
  proxy_bindings_.AddBinding(this, std::move(loader_request));
  proxy_bindings_.set_connection_error_handler(base::BindRepeating(
      &WebRequestProxyingURLLoaderFactory::OnProxyBindingError,
      base::Unretained(this)));
}

void WebRequestProxyingURLLoaderFactory::StartProxying(
    void* browser_context,
    content::ResourceContext* resource_context,
    int render_process_id,
    scoped_refptr<WebRequestAPI::RequestIDGenerator> request_id_generator,
    std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data,
    InfoMap* info_map,
    network::mojom::URLLoaderFactoryRequest loader_request,
    network::mojom::URLLoaderFactoryPtrInfo target_factory_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto* proxies =
      WebRequestAPI::ProxySet::GetFromResourceContext(resource_context);

  auto proxy = std::make_unique<WebRequestProxyingURLLoaderFactory>(
      browser_context, resource_context, render_process_id,
      std::move(request_id_generator), std::move(navigation_ui_data), info_map,
      std::move(loader_request), std::move(target_factory_info), proxies);

  proxies->AddProxy(std::move(proxy));
}

void WebRequestProxyingURLLoaderFactory::CreateLoaderAndStart(
    network::mojom::URLLoaderRequest loader_request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    network::mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // Make sure we are not proxying a browser initiated non-navigation request.
  DCHECK(render_process_id_ != -1 || navigation_ui_data_);

  // The request ID doesn't really matter in the Network Service path. It just
  // needs to be unique per-BrowserContext so extensions can make sense of it.
  // Note that |network_service_request_id_| by contrast is not necessarily
  // unique, so we don't use it for identity here.
  const uint64_t web_request_id = request_id_generator_->Generate();

  if (request_id) {
    // Only requests with a non-zero request ID can have their proxy associated
    // with said ID. This is necessary to support correlation against any auth
    // events received by the browser. Requests with a request ID of 0 therefore
    // do not support dispatching |WebRequest.onAuthRequired| events.
    proxies_->AssociateProxyWithRequestId(
        this, content::GlobalRequestID(render_process_id_, request_id));
    network_request_id_to_web_request_id_.emplace(request_id, web_request_id);
  }

  auto result = requests_.emplace(
      web_request_id,
      std::make_unique<InProgressRequest>(
          this, web_request_id, request_id, routing_id, options, request,
          traffic_annotation, std::move(loader_request), std::move(client)));
  result.first->second->Restart();
}

void WebRequestProxyingURLLoaderFactory::Clone(
    network::mojom::URLLoaderFactoryRequest loader_request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  proxy_bindings_.AddBinding(this, std::move(loader_request));
}

void WebRequestProxyingURLLoaderFactory::HandleAuthRequest(
    net::AuthChallengeInfo* auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    int32_t request_id,
    WebRequestAPI::AuthRequestCallback callback) {
  auto it = network_request_id_to_web_request_id_.find(request_id);
  if (it == network_request_id_to_web_request_id_.end()) {
    base::PostTaskWithTraits(FROM_HERE, {content::BrowserThread::UI},
                             base::BindOnce(std::move(callback), base::nullopt,
                                            true /* should_cancel */));
    return;
  }

  auto request_it = requests_.find(it->second);
  DCHECK(request_it != requests_.end());
  request_it->second->HandleAuthRequest(auth_info, std::move(response_headers),
                                        std::move(callback));
}

WebRequestProxyingURLLoaderFactory::~WebRequestProxyingURLLoaderFactory() =
    default;

void WebRequestProxyingURLLoaderFactory::OnTargetFactoryError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  target_factory_.reset();
  proxy_bindings_.CloseAllBindings();

  MaybeRemoveProxy();
}

void WebRequestProxyingURLLoaderFactory::OnProxyBindingError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (proxy_bindings_.empty())
    target_factory_.reset();

  MaybeRemoveProxy();
}

void WebRequestProxyingURLLoaderFactory::RemoveRequest(
    int32_t network_service_request_id,
    uint64_t request_id) {
  network_request_id_to_web_request_id_.erase(network_service_request_id);
  requests_.erase(request_id);
  if (network_service_request_id) {
    proxies_->DisassociateProxyWithRequestId(
        this, content::GlobalRequestID(render_process_id_,
                                       network_service_request_id));
  }

  MaybeRemoveProxy();
}

void WebRequestProxyingURLLoaderFactory::MaybeRemoveProxy() {
  // Even if all URLLoaderFactory pipes connected to this object have been
  // closed it has to stay alive until all active requests have completed.
  if (target_factory_.is_bound() || !requests_.empty())
    return;

  // Deletes |this|.
  proxies_->RemoveProxy(this);
}

}  // namespace extensions

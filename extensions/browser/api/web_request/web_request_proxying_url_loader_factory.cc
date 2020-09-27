// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_proxying_url_loader_factory.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "components/keyed_service/content/browser_context_keyed_service_shutdown_notifier_factory.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/url_utils.h"
#include "extensions/browser/api/web_request/permission_helper.h"
#include "extensions/browser/extension_navigation_ui_data.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_features.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "net/base/completion_repeating_callback.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "net/http/http_util.h"
#include "net/url_request/redirect_info.h"
#include "net/url_request/redirect_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/loader/throttling_url_loader.h"
#include "third_party/blink/public/platform/resource_request_blocked_reason.h"
#include "url/origin.h"

namespace extensions {
namespace {

// This shutdown notifier makes sure the proxy is destroyed if an incognito
// browser context is destroyed. This is needed because WebRequestAPI only
// clears the proxies when the original browser context is destroyed.
class ShutdownNotifierFactory
    : public BrowserContextKeyedServiceShutdownNotifierFactory {
 public:
  static ShutdownNotifierFactory* GetInstance() {
    static base::NoDestructor<ShutdownNotifierFactory> factory;
    return factory.get();
  }

 private:
  friend class base::NoDestructor<ShutdownNotifierFactory>;

  ShutdownNotifierFactory()
      : BrowserContextKeyedServiceShutdownNotifierFactory(
            "WebRequestProxyingURLLoaderFactory") {
    DependsOn(PermissionHelper::GetFactoryInstance());
  }
  ~ShutdownNotifierFactory() override {}

  DISALLOW_COPY_AND_ASSIGN(ShutdownNotifierFactory);
};

// Creates simulated net::RedirectInfo when an extension redirects a request,
// behaving like a redirect response was actually returned by the remote server.
net::RedirectInfo CreateRedirectInfo(
    const network::ResourceRequest& original_request,
    const GURL& new_url,
    int response_code,
    const base::Optional<std::string>& referrer_policy_header) {
  return net::RedirectInfo::ComputeRedirectInfo(
      original_request.method, original_request.url,
      original_request.site_for_cookies,
      original_request.update_first_party_url_on_redirect
          ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
          : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL,
      original_request.referrer_policy, original_request.referrer.spec(),
      response_code, new_url, referrer_policy_header,
      false /* insecure_scheme_was_upgraded */, false /* copy_fragment */,
      false /* is_signed_exchange_fallback_redirect */);
}

}  // namespace

WebRequestProxyingURLLoaderFactory::InProgressRequest::FollowRedirectParams::
    FollowRedirectParams() = default;
WebRequestProxyingURLLoaderFactory::InProgressRequest::FollowRedirectParams::
    ~FollowRedirectParams() = default;

WebRequestProxyingURLLoaderFactory::InProgressRequest::InProgressRequest(
    WebRequestProxyingURLLoaderFactory* factory,
    uint64_t request_id,
    int32_t network_service_request_id,
    int32_t routing_id,
    uint32_t options,
    base::UkmSourceId ukm_source_id,
    const network::ResourceRequest& request,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client)
    : factory_(factory),
      request_(request),
      original_initiator_(request.request_initiator),
      request_id_(request_id),
      network_service_request_id_(network_service_request_id),
      routing_id_(routing_id),
      options_(options),
      ukm_source_id_(ukm_source_id),
      traffic_annotation_(traffic_annotation),
      proxied_loader_receiver_(this, std::move(loader_receiver)),
      target_client_(std::move(client)),
      current_response_(network::mojom::URLResponseHead::New()),
      has_any_extra_headers_listeners_(
          network_service_request_id_ != 0 &&
          ExtensionWebRequestEventRouter::GetInstance()
              ->HasAnyExtraHeadersListener(factory_->browser_context_)) {
  // If there is a client error, clean up the request.
  target_client_.set_disconnect_handler(
      base::BindOnce(&WebRequestProxyingURLLoaderFactory::InProgressRequest::
                         OnClientDisconnected,
                     weak_factory_.GetWeakPtr()));
  proxied_loader_receiver_.set_disconnect_with_reason_handler(
      base::BindOnce(&WebRequestProxyingURLLoaderFactory::InProgressRequest::
                         OnLoaderDisconnected,
                     weak_factory_.GetWeakPtr()));
}

WebRequestProxyingURLLoaderFactory::InProgressRequest::InProgressRequest(
    WebRequestProxyingURLLoaderFactory* factory,
    uint64_t request_id,
    const network::ResourceRequest& request)
    : factory_(factory),
      request_(request),
      original_initiator_(request.request_initiator),
      request_id_(request_id),
      ukm_source_id_(base::kInvalidUkmSourceId),
      proxied_loader_receiver_(this),
      for_cors_preflight_(true),
      has_any_extra_headers_listeners_(
          ExtensionWebRequestEventRouter::GetInstance()
              ->HasAnyExtraHeadersListener(factory_->browser_context_)) {}

WebRequestProxyingURLLoaderFactory::InProgressRequest::~InProgressRequest() {
  DCHECK_NE(state_, State::kInvalid);
  if (request_.keepalive && !for_cors_preflight_) {
    UMA_HISTOGRAM_ENUMERATION("Extensions.WebRequest.KeepaliveRequestState",
                              state_);
    if (base::FeatureList::IsEnabled(
            extensions_features::kReportKeepaliveUkm)) {
      ukm::builders::Extensions_WebRequest_KeepaliveRequestFinished(
          ukm_source_id_)
          .SetState(state_)
          .Record(ukm::UkmRecorder::Get());
    }
  }
  // This is important to ensure that no outstanding blocking requests continue
  // to reference state owned by this object.
  if (info_) {
    ExtensionWebRequestEventRouter::GetInstance()->OnRequestWillBeDestroyed(
        factory_->browser_context_, &info_.value());
  }
  if (on_before_send_headers_callback_) {
    std::move(on_before_send_headers_callback_)
        .Run(net::ERR_ABORTED, base::nullopt);
  }
  if (on_headers_received_callback_) {
    std::move(on_headers_received_callback_)
        .Run(net::ERR_ABORTED, base::nullopt, base::nullopt);
  }
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::Restart() {
  UpdateRequestInfo();
  RestartInternal();
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    UpdateRequestInfo() {
  // Derive a new WebRequestInfo value any time |Restart()| is called, because
  // the details in |request_| may have changed e.g. if we've been redirected.
  // |request_initiator| can be modified on redirects, but we keep the original
  // for |initiator| in the event. See also
  // https://developer.chrome.com/extensions/webRequest#event-onBeforeRequest.
  network::ResourceRequest request_for_info = request_;
  request_for_info.request_initiator = original_initiator_;
  info_.emplace(WebRequestInfoInitParams(
      request_id_, factory_->render_process_id_, request_.render_frame_id,
      factory_->navigation_ui_data_ ? factory_->navigation_ui_data_->DeepCopy()
                                    : nullptr,
      routing_id_, request_for_info, factory_->IsForDownload(),
      !(options_ & network::mojom::kURLLoadOptionSynchronous),
      factory_->IsForServiceWorkerScript(), factory_->navigation_id_,
      ukm_source_id_));

  current_request_uses_header_client_ =
      factory_->url_loader_header_client_receiver_.is_bound() &&
      request_.url.SchemeIsHTTPOrHTTPS() &&
      (for_cors_preflight_ || network_service_request_id_ != 0) &&
      ExtensionWebRequestEventRouter::GetInstance()
          ->HasExtraHeadersListenerForRequest(factory_->browser_context_,
                                              &info_.value());
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::RestartInternal() {
  DCHECK_EQ(info_->url, request_.url)
      << "UpdateRequestInfo must have been called first";
  is_header_client_receiver_paused_ = false;
  // If the header client will be used, we start the request immediately, and
  // OnBeforeSendHeaders and OnSendHeaders will be handled there. Otherwise,
  // send these events before the request starts.
  base::RepeatingCallback<void(int)> continuation;
  const auto state_on_error = State::kRejectedByOnBeforeRequest;
  if (current_request_uses_header_client_) {
    continuation =
        base::BindRepeating(&InProgressRequest::ContinueToStartRequest,
                            weak_factory_.GetWeakPtr(), state_on_error);
  } else if (for_cors_preflight_) {
    // In this case we do nothing because extensions should see nothing.
    return;
  } else {
    continuation =
        base::BindRepeating(&InProgressRequest::ContinueToBeforeSendHeaders,
                            weak_factory_.GetWeakPtr(), state_on_error);
  }
  redirect_url_ = GURL();
  bool should_collapse_initiator = false;
  int result = ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRequest(
      factory_->browser_context_, &info_.value(), continuation, &redirect_url_,
      &should_collapse_initiator);
  if (result == net::ERR_BLOCKED_BY_CLIENT) {
    // The request was cancelled synchronously. Dispatch an error notification
    // and terminate the request.
    network::URLLoaderCompletionStatus status(result);
    if (should_collapse_initiator) {
      status.extended_error_code = static_cast<int>(
          blink::ResourceRequestBlockedReason::kCollapsedByClient);
    }
    OnRequestError(status, state_on_error);
    return;
  }

  if (result == net::ERR_IO_PENDING) {
    // One or more listeners is blocking, so the request must be paused until
    // they respond. |continuation| above will be invoked asynchronously to
    // continue or cancel the request.
    //
    // We pause the receiver here to prevent further client message processing.
    if (proxied_client_receiver_.is_bound())
      proxied_client_receiver_.Pause();

    // Pause the header client, since we want to wait until OnBeforeRequest has
    // finished before processing any future events.
    if (header_client_receiver_.is_bound()) {
      header_client_receiver_.Pause();
      is_header_client_receiver_paused_ = true;
    }
    return;
  }
  DCHECK_EQ(net::OK, result);

  continuation.Run(net::OK);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const base::Optional<GURL>& new_url) {
  if (new_url)
    request_.url = new_url.value();

  for (const std::string& header : removed_headers)
    request_.headers.RemoveHeader(header);
  request_.headers.MergeFrom(modified_headers);

  // Call this before checking |current_request_uses_header_client_| as it
  // calculates it.
  UpdateRequestInfo();

  if (target_loader_.is_bound()) {
    // If header_client_ is used, then we have to call FollowRedirect now as
    // that's what triggers the network service calling back to
    // OnBeforeSendHeaders(). Otherwise, don't call FollowRedirect now. Wait for
    // the onBeforeSendHeaders callback(s) to run as these may modify request
    // headers and if so we'll pass these modifications to FollowRedirect.
    if (current_request_uses_header_client_) {
      target_loader_->FollowRedirect(removed_headers, modified_headers,
                                     modified_cors_exempt_headers, new_url);
    } else {
      auto params = std::make_unique<FollowRedirectParams>();
      params->removed_headers = removed_headers;
      params->modified_headers = modified_headers;
      params->modified_cors_exempt_headers = modified_cors_exempt_headers;
      params->new_url = new_url;
      pending_follow_redirect_params_ = std::move(params);
    }
  }

  RestartInternal();
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
    network::mojom::URLResponseHeadPtr head) {
  if (current_request_uses_header_client_) {
    // Use the headers we got from OnHeadersReceived as that'll contain
    // Set-Cookie if it existed.
    auto saved_headers = current_response_->headers;
    current_response_ = std::move(head);
    current_response_->headers = saved_headers;
    ContinueToResponseStarted(net::OK);
  } else {
    current_response_ = std::move(head);
    HandleResponseOrRedirectHeaders(
        base::BindOnce(&InProgressRequest::ContinueToResponseStarted,
                       weak_factory_.GetWeakPtr()));
  }
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    network::mojom::URLResponseHeadPtr head) {
  if (redirect_url_ != redirect_info.new_url &&
      !IsRedirectSafe(request_.url, redirect_info.new_url,
                      info_->is_navigation_request)) {
    OnNetworkError(
        network::URLLoaderCompletionStatus(net::ERR_UNSAFE_REDIRECT));
    return;
  }

  if (current_request_uses_header_client_) {
    // Use the headers we got from OnHeadersReceived as that'll contain
    // Set-Cookie if it existed.
    auto saved_headers = current_response_->headers;
    current_response_ = std::move(head);
    // If this redirect is from an HSTS upgrade, OnHeadersReceived will not be
    // called before OnReceiveRedirect, so make sure the saved headers exist
    // before setting them.
    if (saved_headers)
      current_response_->headers = saved_headers;
    ContinueToBeforeRedirect(redirect_info, net::OK);
  } else {
    current_response_ = std::move(head);
    HandleResponseOrRedirectHeaders(
        base::BindOnce(&InProgressRequest::ContinueToBeforeRedirect,
                       weak_factory_.GetWeakPtr(), redirect_info));
  }
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::OnUploadProgress(
    int64_t current_position,
    int64_t total_size,
    OnUploadProgressCallback callback) {
  target_client_->OnUploadProgress(current_position, total_size,
                                   std::move(callback));
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  target_client_->OnReceiveCachedMetadata(std::move(data));
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
    OnNetworkError(status);
    return;
  }

  state_ = kCompleted;
  target_client_->OnComplete(status);
  ExtensionWebRequestEventRouter::GetInstance()->OnCompleted(
      factory_->browser_context_, &info_.value(), status.error_code);

  // Deletes |this|.
  factory_->RemoveRequest(network_service_request_id_, request_id_);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::HandleAuthRequest(
    const net::AuthChallengeInfo& auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    WebRequestAPI::AuthRequestCallback callback) {
  DCHECK(!auth_credentials_);

  // If |current_request_uses_header_client_| is true, |current_response_|
  // should already hold the correct set of response headers (including
  // Set-Cookie). So we don't use |response_headers| since it won't have the
  // Set-Cookie headers.
  if (!current_request_uses_header_client_) {
    current_response_ = network::mojom::URLResponseHead::New();
    current_response_->headers = response_headers;
  }
  // We first need to simulate |onHeadersReceived| for the response headers
  // which indicated a need to authenticate.
  HandleResponseOrRedirectHeaders(base::BindOnce(
      &InProgressRequest::ContinueAuthRequest, weak_factory_.GetWeakPtr(),
      auth_info, std::move(callback)));
}

bool WebRequestProxyingURLLoaderFactory::IsForServiceWorkerScript() const {
  return loader_factory_type_ == content::ContentBrowserClient::
                                     URLLoaderFactoryType::kServiceWorkerScript;
}

bool WebRequestProxyingURLLoaderFactory::IsForDownload() const {
  return loader_factory_type_ ==
         content::ContentBrowserClient::URLLoaderFactoryType::kDownload;
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::OnLoaderCreated(
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  // When CORS is involved there may be multiple network::URLLoader associated
  // with this InProgressRequest, because CorsURLLoader may create a new
  // network::URLLoader for the same request id in redirect handling - see
  // CorsURLLoader::FollowRedirect. In such a case the old network::URLLoader
  // is going to be detached fairly soon, so we don't need to take care of it.
  // We need this explicit reset to avoid a DCHECK failure in mojo::Receiver.
  header_client_receiver_.reset();

  header_client_receiver_.Bind(std::move(receiver));
  if (is_header_client_receiver_paused_)
    header_client_receiver_.Pause();
  if (for_cors_preflight_) {
    // In this case we don't have |target_loader_| and
    // |proxied_client_receiver_|, and |receiver| is the only connection to the
    // network service, so we observe mojo connection errors.
    header_client_receiver_.set_disconnect_handler(base::BindOnce(
        &WebRequestProxyingURLLoaderFactory::InProgressRequest::OnNetworkError,
        weak_factory_.GetWeakPtr(),
        network::URLLoaderCompletionStatus(net::ERR_FAILED)));
  }
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::OnBeforeSendHeaders(
    const net::HttpRequestHeaders& headers,
    OnBeforeSendHeadersCallback callback) {
  if (!current_request_uses_header_client_) {
    std::move(callback).Run(net::OK, base::nullopt);
    return;
  }

  request_.headers = headers;
  on_before_send_headers_callback_ = std::move(callback);
  ContinueToBeforeSendHeadersWithOk();
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::OnHeadersReceived(
    const std::string& headers,
    const net::IPEndPoint& remote_endpoint,
    OnHeadersReceivedCallback callback) {
  if (!current_request_uses_header_client_) {
    std::move(callback).Run(net::OK, base::nullopt, base::nullopt);

    if (for_cors_preflight_) {
      // CORS preflight is supported only when "extraHeaders" is specified.
      // Deletes |this|.
      factory_->RemoveRequest(network_service_request_id_, request_id_);
    }
    return;
  }

  on_headers_received_callback_ = std::move(callback);
  current_response_ = network::mojom::URLResponseHead::New();
  current_response_->headers =
      base::MakeRefCounted<net::HttpResponseHeaders>(headers);
  current_response_->remote_endpoint = remote_endpoint;
  HandleResponseOrRedirectHeaders(
      base::BindOnce(&InProgressRequest::ContinueToHandleOverrideHeaders,
                     weak_factory_.GetWeakPtr()));
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    HandleBeforeRequestRedirect() {
  // The extension requested a redirect. Close the connection with the current
  // URLLoader and inform the URLLoaderClient the WebRequest API generated a
  // redirect. To load |redirect_url_|, a new URLLoader will be recreated
  // after receiving FollowRedirect().

  // Forgetting to close the connection with the current URLLoader caused
  // bugs. The latter doesn't know anything about the redirect. Continuing
  // the load with it gives unexpected results. See
  // https://crbug.com/882661#c72.
  proxied_client_receiver_.reset();
  header_client_receiver_.reset();
  target_loader_.reset();

  constexpr int kInternalRedirectStatusCode = 307;

  net::RedirectInfo redirect_info =
      CreateRedirectInfo(request_, redirect_url_, kInternalRedirectStatusCode,
                         base::nullopt /* referrer_policy_header */);

  auto head = network::mojom::URLResponseHead::New();
  std::string headers = base::StringPrintf(
      "HTTP/1.1 %i Internal Redirect\n"
      "Location: %s\n"
      "Non-Authoritative-Reason: WebRequest API\n\n",
      kInternalRedirectStatusCode, redirect_url_.spec().c_str());

  // Cross-origin requests need to modify the Origin header to 'null'. Since
  // CorsURLLoader sets |request_initiator| to the Origin request header in
  // NetworkService, we need to modify |request_initiator| here to craft the
  // Origin header indirectly.
  // Following checks implement the step 10 of "4.4. HTTP-redirect fetch",
  // https://fetch.spec.whatwg.org/#http-redirect-fetch
  if (request_.request_initiator &&
      (!url::Origin::Create(redirect_url_)
            .IsSameOriginWith(url::Origin::Create(request_.url)) &&
       !request_.request_initiator->IsSameOriginWith(
           url::Origin::Create(request_.url)))) {
    // Reset the initiator to pretend tainted origin flag of the spec is set.
    request_.request_initiator = url::Origin();
  }
  head->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
      net::HttpUtil::AssembleRawHeaders(headers));
  head->encoded_data_length = 0;

  current_response_ = std::move(head);
  ContinueToBeforeRedirect(redirect_info, net::OK);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToBeforeSendHeaders(State state_on_error, int error_code) {
  if (error_code != net::OK) {
    OnRequestError(network::URLLoaderCompletionStatus(error_code),
                   state_on_error);
    return;
  }

  if (!current_request_uses_header_client_ && !redirect_url_.is_empty()) {
    HandleBeforeRequestRedirect();
    return;
  }

  if (proxied_client_receiver_.is_bound())
    proxied_client_receiver_.Resume();

  if (request_.url.SchemeIsHTTPOrHTTPS()) {
    // NOTE: While it does not appear to be documented (and in fact it may be
    // intuitive), |onBeforeSendHeaders| is only dispatched for HTTP and HTTPS
    // requests.

    const auto state_on_error = State::kRejectedByOnBeforeSendHeaders;
    auto continuation =
        base::BindRepeating(&InProgressRequest::ContinueToSendHeaders,
                            weak_factory_.GetWeakPtr(), state_on_error);
    int result =
        ExtensionWebRequestEventRouter::GetInstance()->OnBeforeSendHeaders(
            factory_->browser_context_, &info_.value(), continuation,
            &request_.headers);

    if (result == net::ERR_BLOCKED_BY_CLIENT) {
      // The request was cancelled synchronously. Dispatch an error notification
      // and terminate the request.
      OnRequestError(network::URLLoaderCompletionStatus(result),
                     state_on_error);
      return;
    }

    if (result == net::ERR_IO_PENDING) {
      // One or more listeners is blocking, so the request must be paused until
      // they respond. |continuation| above will be invoked asynchronously to
      // continue or cancel the request.
      //
      // We pause the binding here to prevent further client message processing.
      if (proxied_client_receiver_.is_bound())
        proxied_client_receiver_.Pause();
      return;
    }
    DCHECK_EQ(net::OK, result);
  }

  ContinueToSendHeadersWithOk(std::set<std::string>(), std::set<std::string>());
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToBeforeSendHeadersWithOk() {
  ContinueToBeforeSendHeaders(State::kInvalid, net::OK);
}
void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToStartRequest(State state_on_error, int error_code) {
  if (error_code != net::OK) {
    OnRequestError(network::URLLoaderCompletionStatus(error_code),
                   state_on_error);
    return;
  }

  if (current_request_uses_header_client_ && !redirect_url_.is_empty()) {
    if (for_cors_preflight_) {
      // CORS preflight doesn't support redirect.
      OnRequestError(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                     state_on_error);
      return;
    }
    HandleBeforeRequestRedirect();
    return;
  }

  if (proxied_client_receiver_.is_bound())
    proxied_client_receiver_.Resume();

  if (header_client_receiver_.is_bound()) {
    header_client_receiver_.Resume();
    is_header_client_receiver_paused_ = false;
  }

  if (for_cors_preflight_) {
    // For CORS preflight requests, we have already started the request in
    // the network service. We did block the request by blocking
    // |header_client_receiver_|, which we unblocked right above.
    return;
  }

  if (!target_loader_.is_bound() && factory_->target_factory_.is_bound()) {
    // No extensions have cancelled us up to this point, so it's now OK to
    // initiate the real network request.
    uint32_t options = options_;
    // Even if this request does not use the header client, future redirects
    // might, so we need to set the option on the loader.
    if (has_any_extra_headers_listeners_)
      options |= network::mojom::kURLLoadOptionUseHeaderClient;
    factory_->target_factory_->CreateLoaderAndStart(
        target_loader_.BindNewPipeAndPassReceiver(), info_->routing_id,
        network_service_request_id_, options, request_,
        proxied_client_receiver_.BindNewPipeAndPassRemote(),
        traffic_annotation_);
  }

  // From here the lifecycle of this request is driven by subsequent events on
  // either |proxy_loader_binding_|, |proxy_client_binding_|, or
  // |header_client_receiver_|.
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToStartRequestWithOk() {
  ContinueToStartRequest(State::kInvalid, net::OK);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToSendHeaders(State state_on_error,
                          const std::set<std::string>& removed_headers,
                          const std::set<std::string>& set_headers,
                          int error_code) {
  if (error_code != net::OK) {
    OnRequestError(network::URLLoaderCompletionStatus(error_code),
                   state_on_error);
    return;
  }

  if (current_request_uses_header_client_) {
    DCHECK(on_before_send_headers_callback_);
    std::move(on_before_send_headers_callback_)
        .Run(error_code, request_.headers);
  } else if (pending_follow_redirect_params_) {
    pending_follow_redirect_params_->removed_headers.insert(
        pending_follow_redirect_params_->removed_headers.end(),
        removed_headers.begin(), removed_headers.end());

    for (auto& set_header : set_headers) {
      std::string header_value;
      if (request_.headers.GetHeader(set_header, &header_value)) {
        pending_follow_redirect_params_->modified_headers.SetHeader(
            set_header, header_value);
      } else {
        NOTREACHED();
      }
    }

    if (target_loader_.is_bound()) {
      target_loader_->FollowRedirect(
          pending_follow_redirect_params_->removed_headers,
          pending_follow_redirect_params_->modified_headers,
          pending_follow_redirect_params_->modified_cors_exempt_headers,
          pending_follow_redirect_params_->new_url);
    }

    pending_follow_redirect_params_.reset();
  }

  if (proxied_client_receiver_.is_bound())
    proxied_client_receiver_.Resume();

  if (request_.url.SchemeIsHTTPOrHTTPS()) {
    // NOTE: While it does not appear to be documented (and in fact it may be
    // intuitive), |onSendHeaders| is only dispatched for HTTP and HTTPS
    // requests.
    ExtensionWebRequestEventRouter::GetInstance()->OnSendHeaders(
        factory_->browser_context_, &info_.value(), request_.headers);
  }

  if (!current_request_uses_header_client_)
    ContinueToStartRequestWithOk();
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToSendHeadersWithOk(const std::set<std::string>& removed_headers,
                                const std::set<std::string>& set_headers) {
  ContinueToSendHeaders(State::kInvalid, removed_headers, set_headers, net::OK);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::ContinueAuthRequest(
    const net::AuthChallengeInfo& auth_info,
    WebRequestAPI::AuthRequestCallback callback,
    int error_code) {
  if (error_code != net::OK) {
    // Here we come from an onHeaderReceived failure.
    state_ = State::kRejectedByOnHeadersReceivedForAuth;
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt,
                                  true /* should_cancel */));
    return;
  }

  info_->AddResponseInfoFromResourceResponse(*current_response_);
  auto continuation =
      base::BindRepeating(&InProgressRequest::OnAuthRequestHandled,
                          weak_factory_.GetWeakPtr(), base::Passed(&callback));

  auth_credentials_.emplace();
  ExtensionWebRequestEventRouter::AuthRequiredResponse response =
      ExtensionWebRequestEventRouter::GetInstance()->OnAuthRequired(
          factory_->browser_context_, &info_.value(), auth_info, continuation,
          &auth_credentials_.value());

  // At least one extension has a blocking handler for this request, so we'll
  // just wait for them to finish. |OnAuthRequestHandled()| will be invoked
  // eventually.
  if (response == ExtensionWebRequestEventRouter::AuthRequiredResponse::
                      AUTH_REQUIRED_RESPONSE_IO_PENDING) {
    return;
  }

  // We're not touching this auth request. Let the default browser behavior
  // proceed.
  DCHECK_EQ(response, ExtensionWebRequestEventRouter::AuthRequiredResponse::
                          AUTH_REQUIRED_RESPONSE_NO_ACTION);
  continuation.Run(response);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    OnAuthRequestHandled(
        WebRequestAPI::AuthRequestCallback callback,
        ExtensionWebRequestEventRouter::AuthRequiredResponse response) {
  if (proxied_client_receiver_.is_bound())
    proxied_client_receiver_.Resume();

  base::OnceClosure completion;
  switch (response) {
    case ExtensionWebRequestEventRouter::AuthRequiredResponse::
        AUTH_REQUIRED_RESPONSE_NO_ACTION:
      // We're not touching this auth request. Let the default browser behavior
      // proceed.
      completion = base::BindOnce(std::move(callback), base::nullopt,
                                  false /* should_cancel */);
      break;
    case ExtensionWebRequestEventRouter::AuthRequiredResponse::
        AUTH_REQUIRED_RESPONSE_SET_AUTH:
      completion =
          base::BindOnce(std::move(callback), auth_credentials_.value(),
                         false /* should_cancel */);
      break;
    case ExtensionWebRequestEventRouter::AuthRequiredResponse::
        AUTH_REQUIRED_RESPONSE_CANCEL_AUTH:
      completion = base::BindOnce(std::move(callback), base::nullopt,
                                  true /* should_cancel */);
      state_ = State::kRejectedByOnAuthRequired;
      break;
    default:
      NOTREACHED();
      return;
  }

  auth_credentials_ = base::nullopt;
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   std::move(completion));
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToHandleOverrideHeaders(int error_code) {
  if (error_code != net::OK) {
    const int status_code = current_response_->headers
                                ? current_response_->headers->response_code()
                                : 0;
    State state;
    if (status_code == net::HTTP_UNAUTHORIZED) {
      state = State::kRejectedByOnHeadersReceivedForAuth;
    } else if (net::HttpResponseHeaders::IsRedirectResponseCode(status_code)) {
      state = State::kRejectedByOnHeadersReceivedForRedirect;
    } else {
      state = State::kRejectedByOnHeadersReceivedForFinalResponse;
    }
    OnRequestError(network::URLLoaderCompletionStatus(error_code), state);
    return;
  }

  DCHECK(on_headers_received_callback_);
  base::Optional<std::string> headers;
  if (override_headers_) {
    headers = override_headers_->raw_headers();
    if (current_request_uses_header_client_) {
      // Make sure to update current_response_,  since when OnReceiveResponse
      // is called we will not use its headers as it might be missing the
      // Set-Cookie line (as that gets stripped over IPC).
      current_response_->headers = override_headers_;
    }
  }

  if (for_cors_preflight_ && !redirect_url_.is_empty()) {
    OnRequestError(network::URLLoaderCompletionStatus(net::ERR_FAILED),
                   State::kRejectedByOnHeadersReceivedForRedirect);
    return;
  }

  std::move(on_headers_received_callback_).Run(net::OK, headers, redirect_url_);
  override_headers_ = nullptr;

  if (for_cors_preflight_) {
    // If this is for CORS preflight, there is no associated client. We notify
    // the completion here, and deletes |this|.
    info_->AddResponseInfoFromResourceResponse(*current_response_);
    ExtensionWebRequestEventRouter::GetInstance()->OnResponseStarted(
        factory_->browser_context_, &info_.value(), net::OK);
    ExtensionWebRequestEventRouter::GetInstance()->OnCompleted(
        factory_->browser_context_, &info_.value(), net::OK);

    // Deletes |this|.
    factory_->RemoveRequest(network_service_request_id_, request_id_);
    return;
  }

  if (proxied_client_receiver_.is_bound())
    proxied_client_receiver_.Resume();
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToResponseStarted(int error_code) {
  DCHECK(!for_cors_preflight_);
  if (error_code != net::OK) {
    OnRequestError(network::URLLoaderCompletionStatus(error_code),
                   State::kRejectedByOnHeadersReceivedForFinalResponse);
    return;
  }

  if (state_ == State::kInProgress) {
    state_ = State::kInProgressWithFinalResponseReceived;
  }

  DCHECK(!current_request_uses_header_client_ || !override_headers_);
  if (override_headers_)
    current_response_->headers = override_headers_;

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

    net::RedirectInfo redirect_info = CreateRedirectInfo(
        request_, new_url, override_headers_->response_code(),
        net::RedirectUtil::GetReferrerPolicyHeader(override_headers_.get()));

    // These will get re-bound if a new request is initiated by
    // |FollowRedirect()|.
    proxied_client_receiver_.reset();
    header_client_receiver_.reset();
    target_loader_.reset();

    ContinueToBeforeRedirect(redirect_info, net::OK);
    return;
  }

  info_->AddResponseInfoFromResourceResponse(*current_response_);

  proxied_client_receiver_.Resume();

  ExtensionWebRequestEventRouter::GetInstance()->OnResponseStarted(
      factory_->browser_context_, &info_.value(), net::OK);
  target_client_->OnReceiveResponse(current_response_.Clone());
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    ContinueToBeforeRedirect(const net::RedirectInfo& redirect_info,
                             int error_code) {
  if (error_code != net::OK) {
    OnRequestError(network::URLLoaderCompletionStatus(error_code),
                   kRejectedByOnHeadersReceivedForRedirect);
    return;
  }

  info_->AddResponseInfoFromResourceResponse(*current_response_);

  if (proxied_client_receiver_.is_bound())
    proxied_client_receiver_.Resume();

  ExtensionWebRequestEventRouter::GetInstance()->OnBeforeRedirect(
      factory_->browser_context_, &info_.value(), redirect_info.new_url);
  target_client_->OnReceiveRedirect(redirect_info, current_response_.Clone());
  request_.url = redirect_info.new_url;
  request_.method = redirect_info.new_method;
  request_.site_for_cookies = redirect_info.new_site_for_cookies;
  request_.referrer = GURL(redirect_info.new_referrer);
  request_.referrer_policy = redirect_info.new_referrer_policy;
  if (request_.trusted_params) {
    request_.trusted_params->isolation_info =
        request_.trusted_params->isolation_info.CreateForRedirect(
            url::Origin::Create(redirect_info.new_url));
  }

  // The request method can be changed to "GET". In this case we need to
  // reset the request body manually.
  if (request_.method == net::HttpRequestHeaders::kGetMethod)
    request_.request_body = nullptr;
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    HandleResponseOrRedirectHeaders(net::CompletionOnceCallback continuation) {
  override_headers_ = nullptr;
  redirect_url_ = GURL();

  net::CompletionRepeatingCallback copyable_callback =
      base::AdaptCallbackForRepeating(std::move(continuation));
  if (request_.url.SchemeIsHTTPOrHTTPS()) {
    DCHECK(info_.has_value());
    int result =
        ExtensionWebRequestEventRouter::GetInstance()->OnHeadersReceived(
            factory_->browser_context_, &info_.value(), copyable_callback,
            current_response_->headers.get(), &override_headers_,
            &redirect_url_);
    if (result == net::ERR_BLOCKED_BY_CLIENT) {
      const int status_code = current_response_->headers
                                  ? current_response_->headers->response_code()
                                  : 0;
      State state;
      if (status_code == net::HTTP_UNAUTHORIZED) {
        state = State::kRejectedByOnHeadersReceivedForAuth;
      } else if (net::HttpResponseHeaders::IsRedirectResponseCode(
                     status_code)) {
        state = State::kRejectedByOnHeadersReceivedForRedirect;
      } else {
        state = State::kRejectedByOnHeadersReceivedForFinalResponse;
      }
      OnRequestError(network::URLLoaderCompletionStatus(result), state);
      return;
    }

    if (result == net::ERR_IO_PENDING) {
      if (proxied_client_receiver_.is_bound()) {
        // One or more listeners is blocking, so the request must be paused
        // until they respond. |continuation| above will be invoked
        // asynchronously to continue or cancel the request.
        //
        // We pause the receiver here to prevent further client message
        // processing.
        proxied_client_receiver_.Pause();
      }
      return;
    }

    DCHECK_EQ(net::OK, result);
  }

  copyable_callback.Run(net::OK);
}
void WebRequestProxyingURLLoaderFactory::InProgressRequest::OnRequestError(
    const network::URLLoaderCompletionStatus& status,
    State state) {
  if (target_client_)
    target_client_->OnComplete(status);
  ExtensionWebRequestEventRouter::GetInstance()->OnErrorOccurred(
      factory_->browser_context_, &info_.value(), true /* started */,
      status.error_code);
  state_ = state;

  // Deletes |this|.
  factory_->RemoveRequest(network_service_request_id_, request_id_);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::OnNetworkError(
    const network::URLLoaderCompletionStatus& status) {
  State state = state_;
  if (state_ == State::kInProgress) {
    state = State::kRejectedByNetworkError;
  } else if (state_ == State::kInProgressWithFinalResponseReceived) {
    state = State::kRejectedByNetworkErrorAfterReceivingFinalResponse;
  }
  OnRequestError(status, state);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    OnClientDisconnected() {
  State state = state_;
  if (state_ == State::kInProgress) {
    state = State::kDetachedFromClient;
  } else if (state_ == State::kInProgressWithFinalResponseReceived) {
    state = State::kDetachedFromClientAfterReceivingResponse;
  }
  OnRequestError(network::URLLoaderCompletionStatus(net::ERR_ABORTED), state);
}

void WebRequestProxyingURLLoaderFactory::InProgressRequest::
    OnLoaderDisconnected(uint32_t custom_reason,
                         const std::string& description) {
  if (custom_reason == network::mojom::URLLoader::kClientDisconnectReason &&
      description == blink::ThrottlingURLLoader::kFollowRedirectReason) {
    // Save the ID here because this request will be restarted with a new
    // URLLoader instead of continuing with FollowRedirect(). The saved ID will
    // be retrieved in the restarted request, which will call
    // RequestIDGenerator::Generate() with the same ID pair.
    factory_->request_id_generator_->SaveID(
        routing_id_, network_service_request_id_, request_id_);

    state_ = State::kRedirectFollowedByAnotherInProgressRequest;
    // Deletes |this|.
    factory_->RemoveRequest(network_service_request_id_, request_id_);
  } else {
    OnNetworkError(network::URLLoaderCompletionStatus(net::ERR_ABORTED));
  }
}

// Determines whether it is safe to redirect from |from_url| to |to_url|.
bool WebRequestProxyingURLLoaderFactory::InProgressRequest::IsRedirectSafe(
    const GURL& from_url,
    const GURL& to_url,
    bool is_navigation_request) {
  // For navigations, non-web accessible resources will be blocked by
  // ExtensionNavigationThrottle.
  if (!is_navigation_request && to_url.SchemeIs(extensions::kExtensionScheme)) {
    const Extension* extension =
        ExtensionRegistry::Get(factory_->browser_context_)
            ->enabled_extensions()
            .GetByID(to_url.host());
    if (!extension)
      return false;
    return WebAccessibleResourcesInfo::IsResourceWebAccessible(extension,
                                                               to_url.path());
  }
  return content::IsSafeRedirectTarget(from_url, to_url);
}

WebRequestProxyingURLLoaderFactory::WebRequestProxyingURLLoaderFactory(
    content::BrowserContext* browser_context,
    int render_process_id,
    WebRequestAPI::RequestIDGenerator* request_id_generator,
    std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data,
    base::Optional<int64_t> navigation_id,
    base::UkmSourceId ukm_source_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote,
    mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
        header_client_receiver,
    WebRequestAPI::ProxySet* proxies,
    content::ContentBrowserClient::URLLoaderFactoryType loader_factory_type)
    : browser_context_(browser_context),
      render_process_id_(render_process_id),
      request_id_generator_(request_id_generator),
      navigation_ui_data_(std::move(navigation_ui_data)),
      navigation_id_(std::move(navigation_id)),
      proxies_(proxies),
      loader_factory_type_(loader_factory_type),
      ukm_source_id_(ukm_source_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // base::Unretained is safe here because the callback will be
  // canceled when |shutdown_notifier_| is destroyed, and |proxies_|
  // owns this.
  shutdown_notifier_ =
      ShutdownNotifierFactory::GetInstance()
          ->Get(browser_context)
          ->Subscribe(base::BindRepeating(&WebRequestAPI::ProxySet::RemoveProxy,
                                          base::Unretained(proxies_), this));

  target_factory_.Bind(std::move(target_factory_remote));
  target_factory_.set_disconnect_handler(
      base::BindOnce(&WebRequestProxyingURLLoaderFactory::OnTargetFactoryError,
                     base::Unretained(this)));
  proxy_receivers_.Add(this, std::move(loader_receiver));
  proxy_receivers_.set_disconnect_handler(base::BindRepeating(
      &WebRequestProxyingURLLoaderFactory::OnProxyBindingError,
      base::Unretained(this)));

  if (header_client_receiver)
    url_loader_header_client_receiver_.Bind(std::move(header_client_receiver));
}

void WebRequestProxyingURLLoaderFactory::StartProxying(
    content::BrowserContext* browser_context,
    int render_process_id,
    WebRequestAPI::RequestIDGenerator* request_id_generator,
    std::unique_ptr<ExtensionNavigationUIData> navigation_ui_data,
    base::Optional<int64_t> navigation_id,
    base::UkmSourceId ukm_source_id,
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver,
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote,
    mojo::PendingReceiver<network::mojom::TrustedURLLoaderHeaderClient>
        header_client_receiver,
    WebRequestAPI::ProxySet* proxies,
    content::ContentBrowserClient::URLLoaderFactoryType loader_factory_type) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto proxy = std::make_unique<WebRequestProxyingURLLoaderFactory>(
      browser_context, render_process_id, request_id_generator,
      std::move(navigation_ui_data), std::move(navigation_id), ukm_source_id,
      std::move(loader_receiver), std::move(target_factory_remote),
      std::move(header_client_receiver), proxies, loader_factory_type);

  proxies->AddProxy(std::move(proxy));
}

void WebRequestProxyingURLLoaderFactory::CreateLoaderAndStart(
    mojo::PendingReceiver<network::mojom::URLLoader> loader_receiver,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    const network::ResourceRequest& request,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Make sure we are not proxying a browser initiated non-navigation
  // request except for loading service worker scripts.
  DCHECK(render_process_id_ != -1 || navigation_ui_data_ ||
         IsForServiceWorkerScript());

  // The |web_request_id| doesn't really matter. It just needs to be
  // unique per-BrowserContext so extensions can make sense of it.
  // Note that |network_service_request_id_| by contrast is not
  // necessarily unique, so we don't use it for identity here. This
  // request ID may be the same as a previous request if the previous
  // request was redirected to a URL that required a different loader.
  const uint64_t web_request_id =
      request_id_generator_->Generate(routing_id, request_id);

  if (request_id) {
    // Only requests with a non-zero request ID can have their proxy
    // associated with said ID. This is necessary to support
    // correlation against any auth events received by the browser.
    // Requests with a request ID of 0 therefore do not support
    // dispatching |WebRequest.onAuthRequired| events.
    proxies_->AssociateProxyWithRequestId(
        this, content::GlobalRequestID(render_process_id_, request_id));
    network_request_id_to_web_request_id_.emplace(request_id, web_request_id);
  }

  auto result = requests_.emplace(
      web_request_id, std::make_unique<InProgressRequest>(
                          this, web_request_id, request_id, routing_id, options,
                          ukm_source_id_, request, traffic_annotation,
                          std::move(loader_receiver), std::move(client)));
  result.first->second->Restart();
}

void WebRequestProxyingURLLoaderFactory::Clone(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> loader_receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  proxy_receivers_.Add(this, std::move(loader_receiver));
}

void WebRequestProxyingURLLoaderFactory::OnLoaderCreated(
    int32_t request_id,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  auto it = network_request_id_to_web_request_id_.find(request_id);
  if (it == network_request_id_to_web_request_id_.end())
    return;

  auto request_it = requests_.find(it->second);
  DCHECK(request_it != requests_.end());
  request_it->second->OnLoaderCreated(std::move(receiver));
}

void WebRequestProxyingURLLoaderFactory::OnLoaderForCorsPreflightCreated(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::TrustedHeaderClient> receiver) {
  // Please note that the URLLoader is now starting, without waiting for
  // additional signals from here. The URLLoader will be blocked before
  // sending HTTP request headers (TrustedHeaderClient.OnBeforeSendHeaders),
  // but the connection set up will be done before that. This is acceptable from
  // Web Request API because the extension has already allowed to set up
  // a connection to the same URL (i.e., the actual request), and distinguishing
  // two connections for the actual request and the preflight request before
  // sending request headers is very difficult.
  const uint64_t web_request_id =
      request_id_generator_->Generate(MSG_ROUTING_NONE, 0);

  auto result = requests_.insert(std::make_pair(
      web_request_id,
      std::make_unique<InProgressRequest>(this, web_request_id, request)));

  result.first->second->OnLoaderCreated(std::move(receiver));
  result.first->second->Restart();
}

void WebRequestProxyingURLLoaderFactory::HandleAuthRequest(
    const net::AuthChallengeInfo& auth_info,
    scoped_refptr<net::HttpResponseHeaders> response_headers,
    int32_t request_id,
    WebRequestAPI::AuthRequestCallback callback) {
  auto it = network_request_id_to_web_request_id_.find(request_id);
  if (it == network_request_id_to_web_request_id_.end()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), base::nullopt,
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  target_factory_.reset();
  proxy_receivers_.Clear();

  MaybeRemoveProxy();
}

void WebRequestProxyingURLLoaderFactory::OnProxyBindingError() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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
  // We can delete this factory only when
  //  - there are no existing requests, and
  //  - it is impossible for a new request to arrive in the future.
  if (!requests_.empty() || !proxy_receivers_.empty()) {
    return;
  }

  // Deletes |this|.
  proxies_->RemoveProxy(this);
}

}  // namespace extensions

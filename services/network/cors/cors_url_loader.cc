// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader.h"

#include "base/stl_util.h"
#include "net/base/load_flags.h"
#include "services/network/cors/preflight_controller.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "url/url_util.h"

namespace network {

namespace cors {

namespace {

bool NeedsPreflight(const ResourceRequest& request) {
  if (!IsCORSEnabledRequestMode(request.fetch_request_mode))
    return false;

  if (request.is_external_request)
    return true;

  if (request.fetch_request_mode ==
      mojom::FetchRequestMode::kCORSWithForcedPreflight) {
    return true;
  }

  if (request.cors_preflight_policy ==
      mojom::CORSPreflightPolicy::kPreventPreflight) {
    return false;
  }

  if (!IsCORSSafelistedMethod(request.method))
    return true;

  return !CORSUnsafeNotForbiddenRequestHeaderNames(
              request.headers.GetHeaderVector(), request.is_revalidating)
              .empty();
}

}  // namespace

CORSURLLoader::CORSURLLoader(
    mojom::URLLoaderRequest loader_request,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    DeleteCallback delete_callback,
    const ResourceRequest& resource_request,
    mojom::URLLoaderClientPtr client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojom::URLLoaderFactory* network_loader_factory,
    const base::RepeatingCallback<void(int)>& request_finalizer,
    const OriginAccessList* origin_access_list)
    : binding_(this, std::move(loader_request)),
      routing_id_(routing_id),
      request_id_(request_id),
      options_(options),
      delete_callback_(std::move(delete_callback)),
      network_loader_factory_(network_loader_factory),
      network_client_binding_(this),
      request_(resource_request),
      forwarding_client_(std::move(client)),
      request_finalizer_(request_finalizer),
      traffic_annotation_(traffic_annotation),
      origin_access_list_(origin_access_list),
      weak_factory_(this) {
  binding_.set_connection_error_handler(base::BindOnce(
      &CORSURLLoader::OnConnectionError, base::Unretained(this)));
  DCHECK(network_loader_factory_);
  DCHECK(origin_access_list_);
  SetCORSFlagIfNeeded();
}

CORSURLLoader::~CORSURLLoader() {
  // Close pipes first to ignore possible subsequent callback invocations
  // cased by |network_loader_|
  network_client_binding_.Close();
}

void CORSURLLoader::Start() {
  if (fetch_cors_flag_ &&
      IsCORSEnabledRequestMode(request_.fetch_request_mode)) {
    // Username and password should be stripped in a CORS-enabled request.
    if (request_.url.has_username() || request_.url.has_password()) {
      GURL::Replacements replacements;
      replacements.SetUsernameStr("");
      replacements.SetPasswordStr("");
      request_.url = request_.url.ReplaceComponents(replacements);
    }
  }
  StartRequest();
}

void CORSURLLoader::FollowRedirect(
    const base::Optional<std::vector<std::string>>&
        to_be_removed_request_headers,
    const base::Optional<net::HttpRequestHeaders>& modified_request_headers) {
  if (!network_loader_ || !is_waiting_follow_redirect_call_) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }
  is_waiting_follow_redirect_call_ = false;

  // When the redirect mode is "error", the client is not expected to
  // call this function. Let's abort the request.
  if (request_.fetch_redirect_mode == mojom::FetchRedirectMode::kError) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  if (to_be_removed_request_headers) {
    for (const auto& name : *to_be_removed_request_headers)
      request_.headers.RemoveHeader(name);
  }
  if (modified_request_headers)
    request_.headers.MergeFrom(*modified_request_headers);

  request_.url = redirect_info_.new_url;
  request_.method = redirect_info_.new_method;
  request_.referrer = GURL(redirect_info_.new_referrer);
  request_.referrer_policy = redirect_info_.new_referrer_policy;

  // The request method can be changed to "GET". In this case we need to
  // reset the request body manually.
  if (request_.method == net::HttpRequestHeaders::kGetMethod)
    request_.request_body = nullptr;

  const bool original_fetch_cors_flag = fetch_cors_flag_;
  SetCORSFlagIfNeeded();

  // We cannot use FollowRedirect for a request with preflight (i.e., when both
  // |fetch_cors_flag_| and |NeedsPreflight(request_)| are true).
  //
  // Additionally, when |original_fetch_cors_flag| is false,
  // |fetch_cors_flag_| is true and |NeedsPreflight(request)| is false, the net/
  // implementation won't attach an "origin" header on redirect, as the original
  // request didn't have one. In such a case we need to re-issue a request
  // manually in order to attach the correct origin header.
  // For "no-cors" requests we rely on redirect logic in net/ (specifically
  // in net/url_request/redirect_util.cc).
  if ((original_fetch_cors_flag && !NeedsPreflight(request_)) ||
      !fetch_cors_flag_) {
    response_tainting_ =
        CalculateResponseTainting(request_.url, request_.fetch_request_mode,
                                  request_.request_initiator, fetch_cors_flag_);
    network_loader_->FollowRedirect(to_be_removed_request_headers,
                                    modified_request_headers);
    return;
  }
  DCHECK_NE(request_.fetch_request_mode, mojom::FetchRequestMode::kNoCORS);

  if (request_finalizer_)
    request_finalizer_.Run(request_id_);
  network_client_binding_.Unbind();

  StartRequest();
}

void CORSURLLoader::ProceedWithResponse() {
  NOTREACHED();
}

void CORSURLLoader::SetPriority(net::RequestPriority priority,
                                int32_t intra_priority_value) {
  if (network_loader_)
    network_loader_->SetPriority(priority, intra_priority_value);
}

void CORSURLLoader::PauseReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->PauseReadingBodyFromNet();
}

void CORSURLLoader::ResumeReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->ResumeReadingBodyFromNet();
}

void CORSURLLoader::OnReceiveResponse(
    const ResourceResponseHead& response_head) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!is_waiting_follow_redirect_call_);

  const bool is_304_for_revalidation =
      request_.is_revalidating && response_head.headers->response_code() == 304;
  if (fetch_cors_flag_ && !is_304_for_revalidation) {
    const auto error_status = CheckAccess(
        request_.url, response_head.headers->response_code(),
        GetHeaderString(response_head, header_names::kAccessControlAllowOrigin),
        GetHeaderString(response_head,
                        header_names::kAccessControlAllowCredentials),
        request_.fetch_credentials_mode,
        tainted_ ? url::Origin() : *request_.request_initiator);
    if (error_status) {
      HandleComplete(URLLoaderCompletionStatus(*error_status));
      return;
    }
  }

  ResourceResponseHead response_head_to_pass = response_head;
  response_head_to_pass.response_type = response_tainting_;
  forwarding_client_->OnReceiveResponse(response_head_to_pass);
}

void CORSURLLoader::OnReceiveRedirect(
    const net::RedirectInfo& redirect_info,
    const ResourceResponseHead& response_head) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!is_waiting_follow_redirect_call_);

  if (request_.fetch_redirect_mode == mojom::FetchRedirectMode::kManual) {
    is_waiting_follow_redirect_call_ = true;
    forwarding_client_->OnReceiveRedirect(redirect_info, response_head);
    return;
  }

  // If |CORS flag| is set and a CORS check for |request| and |response| returns
  // failure, then return a network error.
  if (fetch_cors_flag_ &&
      IsCORSEnabledRequestMode(request_.fetch_request_mode)) {
    const auto error_status = CheckAccess(
        request_.url, response_head.headers->response_code(),
        GetHeaderString(response_head, header_names::kAccessControlAllowOrigin),
        GetHeaderString(response_head,
                        header_names::kAccessControlAllowCredentials),
        request_.fetch_credentials_mode,
        tainted_ ? url::Origin() : *request_.request_initiator);
    if (error_status) {
      HandleComplete(URLLoaderCompletionStatus(*error_status));
      return;
    }
  }

  // Because we initiate a new request on redirect in some cases, we cannot
  // rely on the redirect logic in the network stack. Hence we need to
  // implement some logic in
  // https://fetch.spec.whatwg.org/#http-redirect-fetch here.

  // If |request|’s redirect count is twenty, return a network error.
  // Increase |request|’s redirect count by one.
  if (redirect_count_++ == 20) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_TOO_MANY_REDIRECTS));
    return;
  }

  const auto error_status = CheckRedirectLocation(
      redirect_info.new_url, request_.fetch_request_mode,
      request_.request_initiator, fetch_cors_flag_, tainted_);
  if (error_status) {
    HandleComplete(URLLoaderCompletionStatus(*error_status));
    return;
  }

  // TODO(yhirano): Implement the following (Note: this is needed when upload
  // streaming is implemented):
  // If |actualResponse|’s status is not 303, |request|’s body is non-null, and
  // |request|’s body’s source is null, then return a network error.

  // If |actualResponse|’s location URL’s origin is not same origin with
  // |request|’s current url’s origin and |request|’s origin is not same origin
  // with |request|’s current url’s origin, then set |request|’s tainted origin
  // flag.
  if (request_.request_initiator &&
      (!url::Origin::Create(redirect_info.new_url)
            .IsSameOriginWith(url::Origin::Create(request_.url)) &&
       !request_.request_initiator->IsSameOriginWith(
           url::Origin::Create(request_.url)))) {
    tainted_ = true;
  }

  // TODO(yhirano): Implement the following:
  // If either |actualResponse|’s status is 301 or 302 and |request|’s method is
  // `POST`, or |actualResponse|’s status is 303, set |request|’s method to
  // `GET` and request’s body to null.

  // TODO(yhirano): Implement the following:
  // Invoke |set request’s referrer policy on redirect| on |request| and
  // |actualResponse|.

  redirect_info_ = redirect_info;

  is_waiting_follow_redirect_call_ = true;

  auto response_head_to_pass = response_head;
  if (request_.fetch_redirect_mode == mojom::FetchRedirectMode::kManual) {
    response_head_to_pass.response_type =
        mojom::FetchResponseType::kOpaqueRedirect;
  } else {
    response_head_to_pass.response_type = response_tainting_;
  }
  forwarding_client_->OnReceiveRedirect(redirect_info, response_head_to_pass);
}

void CORSURLLoader::OnUploadProgress(int64_t current_position,
                                     int64_t total_size,
                                     OnUploadProgressCallback ack_callback) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!is_waiting_follow_redirect_call_);
  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(ack_callback));
}

void CORSURLLoader::OnReceiveCachedMetadata(const std::vector<uint8_t>& data) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!is_waiting_follow_redirect_call_);
  forwarding_client_->OnReceiveCachedMetadata(data);
}

void CORSURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!is_waiting_follow_redirect_call_);
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void CORSURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!is_waiting_follow_redirect_call_);
  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
}

void CORSURLLoader::OnComplete(const URLLoaderCompletionStatus& status) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!is_waiting_follow_redirect_call_);

  URLLoaderCompletionStatus modified_status(status);
  if (status.error_code == net::OK)
    modified_status.cors_preflight_timing_info.swap(preflight_timing_info_);
  HandleComplete(modified_status);
}

void CORSURLLoader::StartRequest() {
  if (fetch_cors_flag_ && !base::ContainsValue(url::GetCORSEnabledSchemes(),
                                               request_.url.scheme())) {
    HandleComplete(URLLoaderCompletionStatus(
        CORSErrorStatus(mojom::CORSError::kCORSDisabledScheme)));
    return;
  }

  // If the CORS flag is set, |httpRequest|’s method is neither `GET` nor
  // `HEAD`, or |httpRequest|’s mode is "websocket", then append
  // `Origin`/the result of serializing a request origin with |httpRequest|, to
  // |httpRequest|’s header list.
  //
  // We exclude navigation requests to keep the existing behavior.
  // TODO(yhirano): Reconsider this.
  if (request_.fetch_request_mode != mojom::FetchRequestMode::kNavigate &&
      request_.request_initiator &&
      (fetch_cors_flag_ ||
       (request_.method != "GET" && request_.method != "HEAD"))) {
    request_.headers.SetHeader(
        net::HttpRequestHeaders::kOrigin,
        (tainted_ ? url::Origin() : *request_.request_initiator).Serialize());
  }

  if (fetch_cors_flag_ &&
      request_.fetch_request_mode == mojom::FetchRequestMode::kSameOrigin) {
    DCHECK(request_.request_initiator);
    HandleComplete(URLLoaderCompletionStatus(
        CORSErrorStatus(mojom::CORSError::kDisallowedByMode)));
    return;
  }

  response_tainting_ =
      CalculateResponseTainting(request_.url, request_.fetch_request_mode,
                                request_.request_initiator, fetch_cors_flag_);

  if (!CalculateCredentialsFlag(request_.fetch_credentials_mode,
                                response_tainting_)) {
    request_.load_flags |= net::LOAD_DO_NOT_SAVE_COOKIES;
    request_.load_flags |= net::LOAD_DO_NOT_SEND_COOKIES;
    request_.load_flags |= net::LOAD_DO_NOT_SEND_AUTH_DATA;
  }

  // Note that even when |NeedsPreflight(request_)| holds we don't make a
  // preflight request when |fetch_cors_flag_| is false (e.g., when the origin
  // of the url is equal to the origin of the request.
  if (!fetch_cors_flag_ || !NeedsPreflight(request_)) {
    StartNetworkRequest(net::OK, base::nullopt, base::nullopt);
    return;
  }

  base::OnceCallback<void()> preflight_finalizer;
  if (request_finalizer_)
    preflight_finalizer = base::BindOnce(request_finalizer_, request_id_);

  PreflightController::GetDefaultController()->PerformPreflightCheck(
      base::BindOnce(&CORSURLLoader::StartNetworkRequest,
                     weak_factory_.GetWeakPtr()),
      request_id_, request_, tainted_,
      net::NetworkTrafficAnnotationTag(traffic_annotation_),
      network_loader_factory_, std::move(preflight_finalizer));
}

void CORSURLLoader::StartNetworkRequest(
    int error_code,
    base::Optional<CORSErrorStatus> status,
    base::Optional<PreflightTimingInfo> preflight_timing_info) {
  if (error_code != net::OK) {
    HandleComplete(status ? URLLoaderCompletionStatus(*status)
                          : URLLoaderCompletionStatus(error_code));
    return;
  }
  DCHECK(!status);

  if (preflight_timing_info)
    preflight_timing_info_.push_back(*preflight_timing_info);

  mojom::URLLoaderClientPtr network_client;
  network_client_binding_.Bind(mojo::MakeRequest(&network_client));
  // Binding |this| as an unretained pointer is safe because
  // |network_client_binding_| shares this object's lifetime.
  network_client_binding_.set_connection_error_handler(base::BindOnce(
      &CORSURLLoader::OnConnectionError, base::Unretained(this)));
  network_loader_factory_->CreateLoaderAndStart(
      mojo::MakeRequest(&network_loader_), routing_id_, request_id_, options_,
      request_, std::move(network_client), traffic_annotation_);
}

void CORSURLLoader::HandleComplete(const URLLoaderCompletionStatus& status) {
  forwarding_client_->OnComplete(status);
  std::move(delete_callback_).Run(this);
  // |this| is deleted here.
}

void CORSURLLoader::OnConnectionError() {
  HandleComplete(URLLoaderCompletionStatus(net::ERR_ABORTED));
}

// This should be identical to CalculateCORSFlag defined in
// //third_party/blink/renderer/platform/loader/cors/cors.cc.
void CORSURLLoader::SetCORSFlagIfNeeded() {
  if (fetch_cors_flag_)
    return;

  if (request_.fetch_request_mode == mojom::FetchRequestMode::kNavigate ||
      request_.fetch_request_mode == mojom::FetchRequestMode::kNoCORS) {
    return;
  }

  if (request_.url.SchemeIs(url::kDataScheme))
    return;

  // CORS needs a proper origin (including a unique opaque origin). If the
  // request doesn't have one, CORS should not work.
  DCHECK(request_.request_initiator);

  // The source origin and destination URL pair may be in the allow list.
  if (origin_access_list_->IsAllowed(*request_.request_initiator,
                                     request_.url)) {
    return;
  }

  // When a request is initiated in a unique opaque origin (e.g., in a sandboxed
  // iframe) and the blob is also created in the context, |request_initiator|
  // is a unique opaque origin and url::Origin::Create(request_.url) is another
  // unique opaque origin. url::Origin::IsSameOriginWith(p, q) returns false
  // when both |p| and |q| are opaque, but in this case we want to say that the
  // request is a same-origin request. Hence we don't set |fetch_cors_flag_|,
  // assuming the request comes from a renderer and the origin is checked there
  // (in BaseFetchContext::CanRequest).
  // In the future blob URLs will not come here because there will be a
  // separate URLLoaderFactory for blobs.
  // TODO(yhirano): Remove this logic at the time.
  if (request_.url.SchemeIsBlob() && request_.request_initiator->opaque() &&
      url::Origin::Create(request_.url).opaque()) {
    return;
  }

  if (request_.request_initiator->IsSameOriginWith(
          url::Origin::Create(request_.url))) {
    return;
  }

  fetch_cors_flag_ = true;
}

base::Optional<std::string> CORSURLLoader::GetHeaderString(
    const ResourceResponseHead& response,
    const std::string& header_name) {
  std::string header_value;
  if (!response.headers->GetNormalizedHeader(header_name, &header_value))
    return base::nullopt;
  return header_value;
}

}  // namespace cors

}  // namespace network

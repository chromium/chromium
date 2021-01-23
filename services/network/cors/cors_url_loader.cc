// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader.h"

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/cors/preflight_controller.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/request_mode.h"
#include "services/network/trust_tokens/trust_token_operation_metrics_recorder.h"
#include "services/network/url_loader.h"
#include "url/url_util.h"

namespace network {

namespace cors {

namespace {

bool NeedsPreflight(const ResourceRequest& request) {
  if (!IsCorsEnabledRequestMode(request.mode))
    return false;

  if (request.is_external_request)
    return true;

  if (request.mode == mojom::RequestMode::kCorsWithForcedPreflight) {
    return true;
  }

  if (request.cors_preflight_policy ==
      mojom::CorsPreflightPolicy::kPreventPreflight) {
    return false;
  }

  if (!IsCorsSafelistedMethod(request.method))
    return true;

  return !CorsUnsafeNotForbiddenRequestHeaderNames(
              request.headers.GetHeaderVector(), request.is_revalidating)
              .empty();
}

constexpr const char kTimingAllowOrigin[] = "Timing-Allow-Origin";

}  // namespace

CorsURLLoader::CorsURLLoader(
    mojo::PendingReceiver<mojom::URLLoader> loader_receiver,
    int32_t process_id,
    int32_t routing_id,
    int32_t request_id,
    uint32_t options,
    DeleteCallback delete_callback,
    const ResourceRequest& resource_request,
    bool ignore_isolated_world_origin,
    bool skip_cors_enabled_scheme_check,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojom::URLLoaderFactory* network_loader_factory,
    const OriginAccessList* origin_access_list,
    PreflightController* preflight_controller,
    const base::flat_set<std::string>* allowed_exempt_headers,
    bool allow_any_cors_exempt_header,
    const net::IsolationInfo& isolation_info)
    : receiver_(this, std::move(loader_receiver)),
      process_id_(process_id),
      routing_id_(routing_id),
      request_id_(request_id),
      options_(options),
      delete_callback_(std::move(delete_callback)),
      network_loader_factory_(network_loader_factory),
      request_(resource_request),
      forwarding_client_(std::move(client)),
      traffic_annotation_(traffic_annotation),
      origin_access_list_(origin_access_list),
      preflight_controller_(preflight_controller),
      allowed_exempt_headers_(allowed_exempt_headers),
      skip_cors_enabled_scheme_check_(skip_cors_enabled_scheme_check),
      allow_any_cors_exempt_header_(allow_any_cors_exempt_header),
      isolation_info_(isolation_info) {
  if (ignore_isolated_world_origin)
    request_.isolated_world_origin = base::nullopt;

  receiver_.set_disconnect_handler(
      base::BindOnce(&CorsURLLoader::OnMojoDisconnect, base::Unretained(this)));
  DCHECK(network_loader_factory_);
  DCHECK(origin_access_list_);
  DCHECK(preflight_controller_);
  DCHECK(allowed_exempt_headers_);
  SetCorsFlagIfNeeded();
}

CorsURLLoader::~CorsURLLoader() {
  // Reset pipes first to ignore possible subsequent callback invocations
  // caused by |network_loader_|
  network_client_receiver_.reset();
}

void CorsURLLoader::Start() {
  if (fetch_cors_flag_ && IsCorsEnabledRequestMode(request_.mode)) {
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

void CorsURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const base::Optional<GURL>& new_url) {
  if (!network_loader_ || !deferred_redirect_url_) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  if (new_url &&
      (new_url->GetOrigin() != deferred_redirect_url_->GetOrigin())) {
    NOTREACHED() << "Can only change the URL within the same origin.";
    HandleComplete(URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  deferred_redirect_url_.reset();

  // When the redirect mode is "error", the client is not expected to
  // call this function. Let's abort the request.
  if (request_.redirect_mode == mojom::RedirectMode::kError) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  // Does not allow modifying headers that are stored in |cors_exempt_headers|.
  for (const auto& header : modified_headers.GetHeaderVector()) {
    if (request_.cors_exempt_headers.HasHeader(header.key)) {
      LOG(WARNING) << "A client is trying to modify header value for '"
                   << header.key << "', but it is not permitted.";
      HandleComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
      return;
    }
  }

  network::URLLoader::LogConcerningRequestHeaders(
      modified_headers, true /* added_during_redirect */);
  network::URLLoader::LogConcerningRequestHeaders(
      modified_cors_exempt_headers, true /* added_during_redirect */);

  for (const auto& name : removed_headers) {
    request_.headers.RemoveHeader(name);
    request_.cors_exempt_headers.RemoveHeader(name);
  }
  request_.headers.MergeFrom(modified_headers);

  if (!allow_any_cors_exempt_header_ &&
      !CorsURLLoaderFactory::IsValidCorsExemptHeaders(
          *allowed_exempt_headers_, modified_cors_exempt_headers)) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }
  request_.cors_exempt_headers.MergeFrom(modified_cors_exempt_headers);

  if (!AreRequestHeadersSafe(request_.headers)) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }

  const std::string original_method = std::move(request_.method);
  request_.url = redirect_info_.new_url;
  request_.method = redirect_info_.new_method;
  request_.referrer = GURL(redirect_info_.new_referrer);
  request_.referrer_policy = redirect_info_.new_referrer_policy;
  request_.site_for_cookies = redirect_info_.new_site_for_cookies;

  if (request_.trusted_params) {
    request_.trusted_params->isolation_info =
        request_.trusted_params->isolation_info.CreateForRedirect(
            url::Origin::Create(request_.url));
  }

  // The request method can be changed to "GET". In this case we need to
  // reset the request body manually.
  if (request_.method == net::HttpRequestHeaders::kGetMethod)
    request_.request_body = nullptr;

  const bool original_fetch_cors_flag = fetch_cors_flag_;
  SetCorsFlagIfNeeded();

  // We cannot use FollowRedirect for a request with preflight (i.e., when both
  // |fetch_cors_flag_| and |NeedsPreflight(request_)| are true).
  //
  // When |original_fetch_cors_flag| is false, |fetch_cors_flag_| is true and
  // |NeedsPreflight(request)| is false, the net/ implementation won't attach an
  // "origin" header on redirect, as the original request didn't have one.
  //
  // When the request method is changed (due to 302 status code, for example),
  // the net/ implementation removes the origin header.
  //
  // In such cases we need to re-issue a request manually in order to attach the
  // correct origin header. For "no-cors" requests we rely on redirect logic in
  // net/ (specifically in net/url_request/redirect_util.cc).
  //
  // After both OOR-CORS and network service are fully shipped, we may be able
  // to remove the logic in net/.
  if ((fetch_cors_flag_ && NeedsPreflight(request_)) ||
      (!original_fetch_cors_flag && fetch_cors_flag_) ||
      (fetch_cors_flag_ && original_method != request_.method)) {
    DCHECK_NE(request_.mode, mojom::RequestMode::kNoCors);
    network_client_receiver_.reset();
    StartRequest();
    return;
  }

  response_tainting_ = CalculateResponseTainting(
      request_.url, request_.mode, request_.request_initiator,
      request_.isolated_world_origin, fetch_cors_flag_, tainted_,
      origin_access_list_);
  network_loader_->FollowRedirect(removed_headers, modified_headers,
                                  modified_cors_exempt_headers, new_url);
}

void CorsURLLoader::SetPriority(net::RequestPriority priority,
                                int32_t intra_priority_value) {
  if (network_loader_)
    network_loader_->SetPriority(priority, intra_priority_value);
}

void CorsURLLoader::PauseReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->PauseReadingBodyFromNet();
}

void CorsURLLoader::ResumeReadingBodyFromNet() {
  if (network_loader_)
    network_loader_->ResumeReadingBodyFromNet();
}

void CorsURLLoader::OnReceiveResponse(mojom::URLResponseHeadPtr response_head) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);

  // See 10.7.4 of https://fetch.spec.whatwg.org/#http-network-or-cache-fetch
  const bool is_304_for_revalidation =
      request_.is_revalidating && response_head->headers &&
      response_head->headers->response_code() == 304;
  if (fetch_cors_flag_ && !is_304_for_revalidation) {
    const auto error_status = CheckAccess(
        request_.url,
        GetHeaderString(*response_head,
                        header_names::kAccessControlAllowOrigin),
        GetHeaderString(*response_head,
                        header_names::kAccessControlAllowCredentials),
        request_.credentials_mode,
        tainted_ ? url::Origin() : *request_.request_initiator);
    if (error_status) {
      HandleComplete(URLLoaderCompletionStatus(*error_status));
      return;
    }
  }

  timing_allow_failed_flag_ = !PassesTimingAllowOriginCheck(*response_head);

  response_head->response_type = response_tainting_;
  response_head->timing_allow_passed = !timing_allow_failed_flag_;
  forwarding_client_->OnReceiveResponse(std::move(response_head));
}

void CorsURLLoader::OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                                      mojom::URLResponseHeadPtr response_head) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);

  if (request_.redirect_mode == mojom::RedirectMode::kManual) {
    deferred_redirect_url_ = std::make_unique<GURL>(redirect_info.new_url);
    forwarding_client_->OnReceiveRedirect(redirect_info,
                                          std::move(response_head));
    return;
  }

  // If |CORS flag| is set and a CORS check for |request| and |response| returns
  // failure, then return a network error.
  if (fetch_cors_flag_ && IsCorsEnabledRequestMode(request_.mode)) {
    const auto error_status = CheckAccess(
        request_.url,
        GetHeaderString(*response_head,
                        header_names::kAccessControlAllowOrigin),
        GetHeaderString(*response_head,
                        header_names::kAccessControlAllowCredentials),
        request_.credentials_mode,
        tainted_ ? url::Origin() : *request_.request_initiator);
    if (error_status) {
      HandleComplete(URLLoaderCompletionStatus(*error_status));
      return;
    }
  }

  timing_allow_failed_flag_ = !PassesTimingAllowOriginCheck(*response_head);

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
      redirect_info.new_url, request_.mode, request_.request_initiator,
      fetch_cors_flag_, tainted_);
  if (error_status) {
    HandleComplete(URLLoaderCompletionStatus(*error_status));
    return;
  }

  // If |actualResponse|’s status is not 303, |request|’s body is non-null, and
  // |request|’s body’s source is null, then return a network error.
  if (redirect_info.status_code != net::HTTP_SEE_OTHER &&
      network::URLLoader::HasFetchStreamingUploadBody(&request_)) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }

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

  // TODO(crbug.com/1073353): Implement the following:
  // If either |actualResponse|’s status is 301 or 302 and |request|’s method is
  // `POST`, or |actualResponse|’s status is 303, set |request|’s method to
  // `GET` and request’s body to null, and remove request-body-header name from
  // request's headers. Some of them are implemented in //net, but when we
  // create another request on exceptional redirect cases, such newly created
  // request doesn't reflect the spec comformant request modifications. See the
  // linked crbug for details. See also 4.4. HTTP-redirect fetch
  // (https://fetch.spec.whatwg.org/#http-redirect-fetch), step 11.

  // TODO(crbug.com/1073353): Implement the following:
  // Invoke |set request’s referrer policy on redirect| on |request| and
  // |actualResponse|. See 4.4. HTTP-redirect fetch
  // (https://fetch.spec.whatwg.org/#http-redirect-fetch), step 14.

  redirect_info_ = redirect_info;

  deferred_redirect_url_ = std::make_unique<GURL>(redirect_info.new_url);

  if (request_.redirect_mode == mojom::RedirectMode::kManual) {
    response_head->response_type = mojom::FetchResponseType::kOpaqueRedirect;
  } else {
    response_head->response_type = response_tainting_;
  }
  response_head->timing_allow_passed = !timing_allow_failed_flag_;
  forwarding_client_->OnReceiveRedirect(redirect_info,
                                        std::move(response_head));
}

void CorsURLLoader::OnUploadProgress(int64_t current_position,
                                     int64_t total_size,
                                     OnUploadProgressCallback ack_callback) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  forwarding_client_->OnUploadProgress(current_position, total_size,
                                       std::move(ack_callback));
}

void CorsURLLoader::OnReceiveCachedMetadata(mojo_base::BigBuffer data) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);
  forwarding_client_->OnReceiveCachedMetadata(std::move(data));
}

void CorsURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void CorsURLLoader::OnStartLoadingResponseBody(
    mojo::ScopedDataPipeConsumerHandle body) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);
  forwarding_client_->OnStartLoadingResponseBody(std::move(body));
}

void CorsURLLoader::OnComplete(const URLLoaderCompletionStatus& status) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);

  // |network_loader_| will call OnComplete at anytime when a problem happens
  // inside the URLLoader, e.g. on URLLoader::OnMojoDisconnect call. We need
  // to expect it also happens even during redirect handling.
  DCHECK(!deferred_redirect_url_ || status.error_code != net::OK);

  HandleComplete(status);
}

void CorsURLLoader::StartRequest() {
  if (fetch_cors_flag_ && !skip_cors_enabled_scheme_check_ &&
      !base::Contains(url::GetCorsEnabledSchemes(), request_.url.scheme())) {
    HandleComplete(URLLoaderCompletionStatus(
        CorsErrorStatus(mojom::CorsError::kCorsDisabledScheme)));
    return;
  }

  // If the |CORS flag| is set, |httpRequest|’s method is neither `GET` nor
  // `HEAD`, or |httpRequest|’s mode is "websocket", then append
  // `Origin`/the result of serializing a request origin with |httpRequest|, to
  // |httpRequest|’s header list.
  //
  // We exclude navigation requests to keep the existing behavior.
  // TODO(yhirano): Reconsider this.
  if (request_.mode != network::mojom::RequestMode::kNavigate &&
      request_.request_initiator &&
      (fetch_cors_flag_ ||
       (request_.method != net::HttpRequestHeaders::kGetMethod &&
        request_.method != net::HttpRequestHeaders::kHeadMethod))) {
    if (tainted_) {
      request_.headers.SetHeader(net::HttpRequestHeaders::kOrigin,
                                 url::Origin().Serialize());
    } else {
      request_.headers.SetHeader(net::HttpRequestHeaders::kOrigin,
                                 request_.request_initiator->Serialize());
    }
  }

  if (fetch_cors_flag_ && request_.mode == mojom::RequestMode::kSameOrigin) {
    DCHECK(request_.request_initiator);
    HandleComplete(URLLoaderCompletionStatus(
        CorsErrorStatus(mojom::CorsError::kDisallowedByMode)));
    return;
  }

  response_tainting_ = CalculateResponseTainting(
      request_.url, request_.mode, request_.request_initiator,
      request_.isolated_world_origin, fetch_cors_flag_, tainted_,
      origin_access_list_);

  // Note that even when |NeedsPreflight(request_)| holds we don't make a
  // preflight request when |fetch_cors_flag_| is false (e.g., when the origin
  // of the url is equal to the origin of the request.
  if (!fetch_cors_flag_ || !NeedsPreflight(request_)) {
    StartNetworkRequest(net::OK, base::nullopt);
    return;
  }

  // Since we're doing a preflight, we won't reuse the original request. Cancel
  // it now to free up the socket.
  network_loader_.reset();

  preflight_controller_->PerformPreflightCheck(
      base::BindOnce(&CorsURLLoader::StartNetworkRequest,
                     weak_factory_.GetWeakPtr()),
      request_,
      PreflightController::WithTrustedHeaderClient(
          options_ & mojom::kURLLoadOptionUseHeaderClient),
      tainted_, net::NetworkTrafficAnnotationTag(traffic_annotation_),
      network_loader_factory_, process_id_, isolation_info_);
}

void CorsURLLoader::StartNetworkRequest(
    int error_code,
    base::Optional<CorsErrorStatus> status) {
  if (error_code != net::OK) {
    HandleComplete(status ? URLLoaderCompletionStatus(*status)
                          : URLLoaderCompletionStatus(error_code));
    return;
  }
  DCHECK(!status);

  // Here we overwrite the credentials mode sent to URLLoader because
  // network::URLLoader doesn't understand |kSameOrigin|.
  // TODO(crbug.com/943939): Fix this.
  auto original_credentials_mode = request_.credentials_mode;
  if (original_credentials_mode == mojom::CredentialsMode::kSameOrigin) {
    request_.credentials_mode =
        CalculateCredentialsFlag(original_credentials_mode, response_tainting_)
            ? mojom::CredentialsMode::kInclude
            : mojom::CredentialsMode::kOmit;
  }

  // Binding |this| as an unretained pointer is safe because
  // |network_client_receiver_| shares this object's lifetime.
  network_loader_.reset();
  network_loader_factory_->CreateLoaderAndStart(
      network_loader_.BindNewPipeAndPassReceiver(), routing_id_, request_id_,
      options_, request_, network_client_receiver_.BindNewPipeAndPassRemote(),
      traffic_annotation_);
  network_client_receiver_.set_disconnect_handler(
      base::BindOnce(&CorsURLLoader::OnMojoDisconnect, base::Unretained(this)));

  request_.credentials_mode = original_credentials_mode;
}

void CorsURLLoader::HandleComplete(const URLLoaderCompletionStatus& status) {
  if (request_.trust_token_params) {
    HistogramTrustTokenOperationNetError(request_.trust_token_params->type,
                                         status.trust_token_operation_status,
                                         status.error_code);
  }

  // TODO(crbug.com/1152550): Remove this histogram after platform apps no
  // longer require relaxing CORB/CORS in their content scripts.
  if (status.error_code == net::OK) {
    UMA_HISTOGRAM_BOOLEAN("NetworkService.CorsForcedOffForIsolatedWorldOrigin",
                          has_cors_been_affected_by_isolated_world_origin_);
  }

  forwarding_client_->OnComplete(status);
  std::move(delete_callback_).Run(this);
  // |this| is deleted here.
}

void CorsURLLoader::OnMojoDisconnect() {
  HandleComplete(URLLoaderCompletionStatus(net::ERR_ABORTED));
}

// This should be identical to CalculateCorsFlag defined in
// //third_party/blink/renderer/platform/loader/cors/cors.cc.
void CorsURLLoader::SetCorsFlagIfNeeded() {
  if (fetch_cors_flag_) {
    return;
  }

  if (!network::cors::ShouldCheckCors(request_.url, request_.request_initiator,
                                      request_.mode)) {
    return;
  }

  if (HasSpecialAccessToDestination()) {
    has_cors_been_affected_by_isolated_world_origin_ =
        request_.isolated_world_origin.has_value();
    return;
  }

  fetch_cors_flag_ = true;
}

bool CorsURLLoader::HasSpecialAccessToDestination() const {
  // The source origin and destination URL pair may be in the allow list.
  switch (origin_access_list_->CheckAccessState(request_)) {
    case OriginAccessList::AccessState::kAllowed:
      return true;
    case OriginAccessList::AccessState::kBlocked:
    case OriginAccessList::AccessState::kNotListed:
      return false;
  }
}

// Keep this in sync with the identical function
// blink::cors::CalculateResponseTainting.
//
// static
mojom::FetchResponseType CorsURLLoader::CalculateResponseTainting(
    const GURL& url,
    mojom::RequestMode request_mode,
    const base::Optional<url::Origin>& origin,
    const base::Optional<url::Origin>& isolated_world_origin,
    bool cors_flag,
    bool tainted_origin,
    const OriginAccessList* origin_access_list) {
  if (url.SchemeIs(url::kDataScheme))
    return mojom::FetchResponseType::kBasic;

  if (cors_flag) {
    DCHECK(IsCorsEnabledRequestMode(request_mode));
    return mojom::FetchResponseType::kCors;
  }

  if (!origin) {
    // This is actually not defined in the fetch spec, but in this case CORS
    // is disabled so no one should care this value.
    return mojom::FetchResponseType::kBasic;
  }

  // OriginAccessList is in practice used to disable CORS for Chrome Extensions.
  // The extension origin can be found in either:
  // 1) |isolated_world_origin| (if this is a request from a content
  //    script;  in this case there is no point looking at (2) below.
  // 2) |origin| (if this is a request from an extension
  //    background page or from other extension frames).
  //
  // Note that similar code is present in OriginAccessList::CheckAccessState.
  //
  // TODO(lukasza): https://crbug.com/936310 and https://crbug.com/920638:
  // Once 1) there is no global OriginAccessList and 2) per-factory
  // OriginAccessList is only populated for URLLoaderFactory used by allowlisted
  // content scripts, then 3) there should no longer be a need to use origins as
  // a key in an OriginAccessList.
  const url::Origin& source_origin = isolated_world_origin.value_or(*origin);

  if (request_mode == mojom::RequestMode::kNoCors) {
    if (tainted_origin ||
        (!origin->IsSameOriginWith(url::Origin::Create(url)) &&
         origin_access_list->CheckAccessState(source_origin, url) !=
             OriginAccessList::AccessState::kAllowed)) {
      return mojom::FetchResponseType::kOpaque;
    }
  }
  return mojom::FetchResponseType::kBasic;
}

bool CorsURLLoader::PassesTimingAllowOriginCheck(
    const mojom::URLResponseHead& response) const {
  if (timing_allow_failed_flag_)
    return false;

  if (response_tainting_ == mojom::FetchResponseType::kBasic)
    return true;

  base::Optional<std::string> tao_header =
      GetHeaderString(response, kTimingAllowOrigin);
  if (!tao_header.has_value())
    return false;

  // Optimization for the common case when the header is a single '*'.
  if (tao_header == "*")
    return true;

  url::Origin origin = tainted_ ? url::Origin() : *request_.request_initiator;
  std::string serialized_origin = origin.Serialize();
  std::vector<std::string> tao_headers = base::SplitString(
      *tao_header, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  for (const std::string& header : tao_headers) {
    if (header == "*")
      return true;

    if (header == serialized_origin)
      return true;
  }
  return false;
}

// static
base::Optional<std::string> CorsURLLoader::GetHeaderString(
    const mojom::URLResponseHead& response,
    const std::string& header_name) {
  if (!response.headers)
    return base::nullopt;
  std::string header_value;
  if (!response.headers->GetNormalizedHeader(header_name, &header_value))
    return base::nullopt;
  return header_value;
}

}  // namespace cors

}  // namespace network

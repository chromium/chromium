// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_controller.h"

#include <algorithm>
#include <vector>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/optional.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/unguessable_token.h"
#include "net/base/load_flags.h"
#include "net/base/network_isolation_key.h"
#include "net/http/http_request_headers.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace network {

namespace cors {

namespace {

int RetrieveCacheFlags(int load_flags) {
  return load_flags & (net::LOAD_VALIDATE_CACHE | net::LOAD_BYPASS_CACHE |
                       net::LOAD_DISABLE_CACHE);
}

base::Optional<std::string> GetHeaderString(
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    const std::string& header_name) {
  std::string header_value;
  if (!headers || !headers->GetNormalizedHeader(header_name, &header_value))
    return base::nullopt;
  return header_value;
}

// Algorithm step 3 of the CORS-preflight fetch,
// https://fetch.spec.whatwg.org/#cors-preflight-fetch-0, that requires
//  - CORS-safelisted request-headers excluded
//  - duplicates excluded
//  - sorted lexicographically
//  - byte-lowercased
std::string CreateAccessControlRequestHeadersHeader(
    const net::HttpRequestHeaders& headers,
    bool is_revalidating) {
  // Exclude the forbidden headers because they may be added by the user
  // agent. They must be checked separately and rejected for
  // JavaScript-initiated requests.
  std::vector<std::string> filtered_headers =
      CorsUnsafeNotForbiddenRequestHeaderNames(headers.GetHeaderVector(),
                                               is_revalidating);
  if (filtered_headers.empty())
    return std::string();

  // Sort header names lexicographically.
  std::sort(filtered_headers.begin(), filtered_headers.end());

  return base::JoinString(filtered_headers, ",");
}

std::unique_ptr<ResourceRequest> CreatePreflightRequest(
    const ResourceRequest& request,
    bool tainted,
    const base::Optional<base::UnguessableToken>& devtools_request_id) {
  DCHECK(!request.url.has_username());
  DCHECK(!request.url.has_password());

  std::unique_ptr<ResourceRequest> preflight_request =
      std::make_unique<ResourceRequest>();

  // Algorithm step 1 through 5 of the CORS-preflight fetch,
  // https://fetch.spec.whatwg.org/#cors-preflight-fetch.
  preflight_request->url = request.url;
  preflight_request->method = net::HttpRequestHeaders::kOptionsMethod;
  preflight_request->priority = request.priority;
  preflight_request->destination = request.destination;
  preflight_request->referrer = request.referrer;
  preflight_request->referrer_policy = request.referrer_policy;
  preflight_request->mode = mojom::RequestMode::kCors;

  preflight_request->credentials_mode = mojom::CredentialsMode::kOmit;
  preflight_request->load_flags = RetrieveCacheFlags(request.load_flags);
  preflight_request->resource_type = request.resource_type;
  preflight_request->fetch_window_id = request.fetch_window_id;

  preflight_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                       kDefaultAcceptHeaderValue);

  preflight_request->headers.SetHeader(
      header_names::kAccessControlRequestMethod, request.method);

  std::string request_headers = CreateAccessControlRequestHeadersHeader(
      request.headers, request.is_revalidating);
  if (!request_headers.empty()) {
    preflight_request->headers.SetHeader(
        header_names::kAccessControlRequestHeaders, request_headers);
  }

  if (request.is_external_request) {
    preflight_request->headers.SetHeader(
        header_names::kAccessControlRequestExternal, "true");
  }

  DCHECK(request.request_initiator);
  preflight_request->request_initiator = request.request_initiator;
  preflight_request->headers.SetHeader(
      net::HttpRequestHeaders::kOrigin,
      (tainted ? url::Origin() : *request.request_initiator).Serialize());

  // We normally set User-Agent down in the network stack, but the DevTools
  // emulation override is applied on a higher level (renderer or browser),
  // so copy User-Agent from the original request, if present.
  // TODO(caseq, morlovich): do the same for client hints.
  std::string user_agent;
  if (request.headers.GetHeader(net::HttpRequestHeaders::kUserAgent,
                                &user_agent)) {
    preflight_request->headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                         user_agent);
  }

  // Additional headers that the algorithm in the spec does not require, but
  // it's better that CORS preflight requests have them.
  preflight_request->headers.SetHeader("Sec-Fetch-Mode", "cors");

  if (devtools_request_id) {
    // Set |enable_load_timing| flag to make URLLoader fill the LoadTimingInfo
    // in URLResponseHead, which will be sent to DevTools.
    preflight_request->enable_load_timing = true;
    // Set |devtools_request_id| to make URLLoader send the raw request and the
    // raw response to DevTools.
    preflight_request->devtools_request_id = devtools_request_id->ToString();
  }
  preflight_request->is_fetch_like_api = request.is_fetch_like_api;
  preflight_request->is_favicon = request.is_favicon;

  return preflight_request;
}

std::unique_ptr<PreflightResult> CreatePreflightResult(
    const GURL& final_url,
    const mojom::URLResponseHead& head,
    const ResourceRequest& original_request,
    bool tainted,
    base::Optional<CorsErrorStatus>* detected_error_status) {
  DCHECK(detected_error_status);

  *detected_error_status = CheckPreflightAccess(
      final_url, head.headers ? head.headers->response_code() : 0,
      GetHeaderString(head.headers, header_names::kAccessControlAllowOrigin),
      GetHeaderString(head.headers,
                      header_names::kAccessControlAllowCredentials),
      original_request.credentials_mode,
      tainted ? url::Origin() : *original_request.request_initiator);
  if (*detected_error_status)
    return nullptr;

  if (original_request.is_external_request) {
    *detected_error_status = CheckExternalPreflight(GetHeaderString(
        head.headers, header_names::kAccessControlAllowExternal));
    if (*detected_error_status)
      return nullptr;
  }

  base::Optional<mojom::CorsError> error;
  auto result = PreflightResult::Create(
      original_request.credentials_mode,
      GetHeaderString(head.headers, header_names::kAccessControlAllowMethods),
      GetHeaderString(head.headers, header_names::kAccessControlAllowHeaders),
      GetHeaderString(head.headers, header_names::kAccessControlMaxAge),
      &error);

  if (error)
    *detected_error_status = CorsErrorStatus(*error);
  return result;
}

base::Optional<CorsErrorStatus> CheckPreflightResult(
    PreflightResult* result,
    const ResourceRequest& original_request,
    PreflightResult::WithNonWildcardRequestHeadersSupport
        with_non_wildcard_request_headers_support) {
  base::Optional<CorsErrorStatus> status =
      result->EnsureAllowedCrossOriginMethod(original_request.method);
  if (status)
    return status;

  return result->EnsureAllowedCrossOriginHeaders(
      original_request.headers, original_request.is_revalidating,
      with_non_wildcard_request_headers_support);
}

}  // namespace

class PreflightController::PreflightLoader final {
 public:
  PreflightLoader(
      PreflightController* controller,
      CompletionCallback completion_callback,
      const ResourceRequest& request,
      WithTrustedHeaderClient with_trusted_header_client,
      WithNonWildcardRequestHeadersSupport
          with_non_wildcard_request_headers_support,
      bool tainted,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      const net::NetworkIsolationKey& network_isolation_key,
      mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer)
      : controller_(controller),
        completion_callback_(std::move(completion_callback)),
        original_request_(request),
        with_non_wildcard_request_headers_support_(
            with_non_wildcard_request_headers_support),
        tainted_(tainted),
        network_isolation_key_(network_isolation_key),
        devtools_observer_(std::move(devtools_observer)) {
    if (devtools_observer_)
      devtools_request_id_ = base::UnguessableToken::Create();
    auto preflight_request =
        CreatePreflightRequest(request, tainted, devtools_request_id_);

    if (devtools_observer_) {
      DCHECK(devtools_request_id_);
      devtools_observer_->OnCorsPreflightRequest(
          *devtools_request_id_, *preflight_request, original_request_.url,
          original_request_.devtools_request_id.value_or(""));
    }
    loader_ =
        SimpleURLLoader::Create(std::move(preflight_request), annotation_tag);
    uint32_t options = mojom::kURLLoadOptionAsCorsPreflight;
    if (with_trusted_header_client) {
      options |= mojom::kURLLoadOptionUseHeaderClient;
    }
    loader_->SetURLLoaderFactoryOptions(options);
  }

  void Request(mojom::URLLoaderFactory* loader_factory) {
    DCHECK(loader_);

    loader_->SetOnRedirectCallback(base::BindRepeating(
        &PreflightLoader::HandleRedirect, base::Unretained(this)));
    loader_->SetOnResponseStartedCallback(base::BindOnce(
        &PreflightLoader::HandleResponseHeader, base::Unretained(this)));

    loader_->DownloadToString(
        loader_factory,
        base::BindOnce(&PreflightLoader::HandleResponseBody,
                       base::Unretained(this)),
        0);
  }

 private:
  void HandleRedirect(const net::RedirectInfo& redirect_info,
                      const network::mojom::URLResponseHead& response_head,
                      std::vector<std::string>* to_be_removed_headers) {
    if (devtools_observer_) {
      DCHECK(devtools_request_id_);
      devtools_observer_->OnCorsPreflightRequestCompleted(
          *devtools_request_id_,
          network::URLLoaderCompletionStatus(net::ERR_INVALID_REDIRECT));
    }

    // Preflight should not allow any redirect.
    FinalizeLoader();

    std::move(completion_callback_)
        .Run(net::ERR_FAILED,
             CorsErrorStatus(mojom::CorsError::kPreflightDisallowedRedirect),
             false);

    RemoveFromController();
    // |this| is deleted here.
  }

  void HandleResponseHeader(const GURL& final_url,
                            const mojom::URLResponseHead& head) {
    if (devtools_observer_) {
      DCHECK(devtools_request_id_);
      devtools_observer_->OnCorsPreflightResponse(
          *devtools_request_id_, original_request_.url, head.Clone());
      devtools_observer_->OnCorsPreflightRequestCompleted(
          *devtools_request_id_, network::URLLoaderCompletionStatus(net::OK));
    }

    FinalizeLoader();

    base::Optional<CorsErrorStatus> detected_error_status;
    bool has_authorization_covered_by_wildcard = false;
    std::unique_ptr<PreflightResult> result = CreatePreflightResult(
        final_url, head, original_request_, tainted_, &detected_error_status);

    if (result) {
      // Preflight succeeded. Check |original_request_| with |result|.
      DCHECK(!detected_error_status);
      detected_error_status =
          CheckPreflightResult(result.get(), original_request_,
                               with_non_wildcard_request_headers_support_);
      has_authorization_covered_by_wildcard =
          result->HasAuthorizationCoveredByWildcard(original_request_.headers);
    }

    if (!(original_request_.load_flags & net::LOAD_DISABLE_CACHE) &&
        !detected_error_status) {
      controller_->AppendToCache(*original_request_.request_initiator,
                                 original_request_.url, network_isolation_key_,
                                 std::move(result));
    }

    std::move(completion_callback_)
        .Run(detected_error_status ? net::ERR_FAILED : net::OK,
             detected_error_status, has_authorization_covered_by_wildcard);

    RemoveFromController();
    // |this| is deleted here.
  }

  void HandleResponseBody(std::unique_ptr<std::string> response_body) {
    // Reached only when the request fails without receiving headers, e.g.
    // unknown hosts, unreachable remote, reset by peer, and so on.
    // See https://crbug.com/826868 for related discussion.
    DCHECK(!response_body);
    const int error = loader_->NetError();
    DCHECK_NE(error, net::OK);
    if (devtools_observer_) {
      DCHECK(devtools_request_id_);
      devtools_observer_->OnCorsPreflightRequestCompleted(
          *devtools_request_id_, network::URLLoaderCompletionStatus(error));
    }
    FinalizeLoader();
    std::move(completion_callback_).Run(error, base::nullopt, false);
    RemoveFromController();
    // |this| is deleted here.
  }

  void FinalizeLoader() {
    DCHECK(loader_);
    loader_.reset();
  }

  // Removes |this| instance from |controller_|. Once the method returns, |this|
  // is already removed.
  void RemoveFromController() { controller_->RemoveLoader(this); }

  // PreflightController owns all PreflightLoader instances, and should outlive.
  PreflightController* const controller_;

  // Holds SimpleURLLoader instance for the CORS-preflight request.
  std::unique_ptr<SimpleURLLoader> loader_;

  // Holds caller's information.
  PreflightController::CompletionCallback completion_callback_;
  const ResourceRequest original_request_;

  const WithNonWildcardRequestHeadersSupport
      with_non_wildcard_request_headers_support_;
  const bool tainted_;
  base::Optional<base::UnguessableToken> devtools_request_id_;
  const net::NetworkIsolationKey network_isolation_key_;
  mojo::Remote<mojom::DevToolsObserver> devtools_observer_;

  DISALLOW_COPY_AND_ASSIGN(PreflightLoader);
};

// static
std::unique_ptr<ResourceRequest>
PreflightController::CreatePreflightRequestForTesting(
    const ResourceRequest& request,
    bool tainted) {
  return CreatePreflightRequest(request, tainted, base::nullopt);
}

// static
std::unique_ptr<PreflightResult>
PreflightController::CreatePreflightResultForTesting(
    const GURL& final_url,
    const mojom::URLResponseHead& head,
    const ResourceRequest& original_request,
    bool tainted,
    base::Optional<CorsErrorStatus>* detected_error_status) {
  return CreatePreflightResult(final_url, head, original_request, tainted,
                               detected_error_status);
}

PreflightController::PreflightController(NetworkService* network_service)
    : network_service_(network_service) {}

PreflightController::~PreflightController() = default;

void PreflightController::PerformPreflightCheck(
    CompletionCallback callback,
    const ResourceRequest& request,
    WithTrustedHeaderClient with_trusted_header_client,
    WithNonWildcardRequestHeadersSupport
        with_non_wildcard_request_headers_support,
    bool tainted,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    mojom::URLLoaderFactory* loader_factory,
    const net::IsolationInfo& isolation_info,
    mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer) {
  DCHECK(request.request_initiator);

  const net::NetworkIsolationKey& network_isolation_key =
      !isolation_info.IsEmpty()
          ? isolation_info.network_isolation_key()
          : request.trusted_params.has_value()
                ? request.trusted_params->isolation_info.network_isolation_key()
                : net::NetworkIsolationKey();
  if (!RetrieveCacheFlags(request.load_flags) && !request.is_external_request &&
      cache_.CheckIfRequestCanSkipPreflight(
          request.request_initiator.value(), request.url, network_isolation_key,
          request.credentials_mode, request.method, request.headers,
          request.is_revalidating)) {
    std::move(callback).Run(net::OK, base::nullopt, false);
    return;
  }

  auto emplaced_pair = loaders_.emplace(std::make_unique<PreflightLoader>(
      this, std::move(callback), request, with_trusted_header_client,
      with_non_wildcard_request_headers_support, tainted, annotation_tag,
      network_isolation_key, std::move(devtools_observer)));
  (*emplaced_pair.first)->Request(loader_factory);
}

void PreflightController::RemoveLoader(PreflightLoader* loader) {
  auto it = loaders_.find(loader);
  DCHECK(it != loaders_.end());
  loaders_.erase(it);
}

void PreflightController::AppendToCache(
    const url::Origin& origin,
    const GURL& url,
    const net::NetworkIsolationKey& network_isolation_key,
    std::unique_ptr<PreflightResult> result) {
  cache_.AppendEntry(origin, url, network_isolation_key, std::move(result));
}

}  // namespace cors

}  // namespace network

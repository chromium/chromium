// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/preflight_controller.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_util.h"
#include "base/unguessable_token.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/network_isolation_key.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/log/net_log.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "services/network/cors/cors_util.h"
#include "services/network/network_service.h"
#include "services/network/private_network_access_checker.h"
#include "services/network/public/cpp/constants.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/cors_error_status.h"
#include "services/network/public/cpp/devtools_observer_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/clear_data_filter.mojom.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/parsed_headers.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_network_service_observer.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace network::cors {

namespace {

const char kLowerCaseTrue[] = "true";

int RetrieveCacheFlags(int load_flags) {
  return load_flags & (net::LOAD_VALIDATE_CACHE | net::LOAD_BYPASS_CACHE |
                       net::LOAD_DISABLE_CACHE);
}

std::optional<std::string> GetHeaderString(
    const scoped_refptr<net::HttpResponseHeaders>& headers,
    const std::string& header_name) {
  std::string header_value;
  if (!headers || !headers->GetNormalizedHeader(header_name, &header_value))
    return std::nullopt;
  return header_value;
}

bool ShouldEnforcePrivateNetworkAccessHeader(
    PrivateNetworkAccessPreflightBehavior behavior) {
  // Use a switch statement to guarantee this is updated when the enum
  // definition changes.
  switch (behavior) {
    case PrivateNetworkAccessPreflightBehavior::kEnforce:
      return true;
    case PrivateNetworkAccessPreflightBehavior::kWarnWithTimeout:
    case PrivateNetworkAccessPreflightBehavior::kWarn:
      return false;
  }
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
    const net::NetLogWithSource& net_log_for_actual_request,
    const std::optional<base::UnguessableToken>& devtools_request_id,
    const PreflightController::PreflightMode& preflight_mode) {
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

  if (preflight_mode.Has(PreflightController::PreflightType::kCors)) {
    preflight_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                         kDefaultAcceptHeaderValue);
  }

  preflight_request->headers.SetHeader(
      header_names::kAccessControlRequestMethod, request.method);

  if (preflight_mode.Has(PreflightController::PreflightType::kCors)) {
    std::string request_headers = CreateAccessControlRequestHeadersHeader(
        request.headers, request.is_revalidating);
    if (!request_headers.empty()) {
      preflight_request->headers.SetHeader(
          header_names::kAccessControlRequestHeaders, request_headers);
    }
  }

  preflight_request->target_ip_address_space = request.target_ip_address_space;
  if (preflight_mode.Has(
          PreflightController::PreflightType::kPrivateNetworkAccess)) {
    CHECK(preflight_request->target_ip_address_space !=
          mojom::IPAddressSpace::kUnknown);
    // See the CORS-preflight fetch algorithm modifications laid out in the
    // Private Network Access spec, in step 4 of the CORS preflight section as
    // of writing: https://wicg.github.io/private-network-access/#cors-preflight
    preflight_request->headers.SetHeader(
        header_names::kAccessControlRequestPrivateNetwork, "true");
  }

  if (request.trusted_params.has_value()) {
    preflight_request->trusted_params = ResourceRequest::TrustedParams();

    // Copy the client security state as well, if set in the request's trusted
    // params. Note that we clone the pointer unconditionally if the original
    // request has trusted params, but that the cloned pointer may be null. It
    // is unclear whether it is safe to copy all the trusted params, so we only
    // copy what we need for PNA.
    //
    // This is useful when the client security state is not specified through
    // the URL loader factory params, typically when a single URL loader factory
    // is shared by a few different client contexts. This is the case for
    // navigations and interest group auctions.
    preflight_request->trusted_params->client_security_state =
        request.trusted_params->client_security_state.Clone();

    // The preflight should use an IsolationInfo corresponding to that of the
    // request (if available) but with `IsolationInfo::RequestType::kOther`
    // since the preflights themselves are not considered navigations, and with
    // an empty `net::SiteForCookies()` since cookies aren't sent with these
    // requests.
    if (!request.trusted_params->isolation_info.IsEmpty()) {
      preflight_request->trusted_params->isolation_info =
          net::IsolationInfo::Create(
              net::IsolationInfo::RequestType::kOther,
              *request.trusted_params->isolation_info.top_frame_origin(),
              *request.trusted_params->isolation_info.frame_origin(),
              net::SiteForCookies(),
              request.trusted_params->isolation_info.nonce());
      // Ensure consistency of this IsolationInfo's SiteForCookies with the
      // SiteForCookies in the request.
      CHECK(preflight_request->site_for_cookies.IsEquivalent(
          preflight_request->trusted_params->isolation_info
              .site_for_cookies()));
    }
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
  std::optional<std::string> user_agent =
      request.headers.GetHeader(net::HttpRequestHeaders::kUserAgent);
  if (user_agent) {
    preflight_request->headers.SetHeader(net::HttpRequestHeaders::kUserAgent,
                                         *user_agent);
  }

  // Additional headers that the algorithm in the spec does not require, but
  // it's better that CORS preflight requests have them.
  preflight_request->headers.SetHeader("Sec-Fetch-Mode", "cors");

  if (devtools_request_id) {
    // Set `enable_load_timing` flag to make URLLoader fill the LoadTimingInfo
    // in URLResponseHead, which will be sent to DevTools.
    preflight_request->enable_load_timing = true;
    // Set `devtools_request_id` to make URLLoader send the raw request and
    // the raw response to DevTools.
    preflight_request->devtools_request_id = devtools_request_id->ToString();
  }
  preflight_request->is_fetch_like_api = request.is_fetch_like_api;
  preflight_request->is_fetch_later_api = request.is_fetch_later_api;
  preflight_request->is_favicon = request.is_favicon;

  // Set `net_log_reference_info` to reference actual request from preflight
  // request in NetLog.
  preflight_request->net_log_reference_info =
      net_log_for_actual_request.source();

  net::NetLogSource net_log_source_for_preflight = net::NetLogSource(
      net::NetLogSourceType::URL_REQUEST, net::NetLog::Get()->NextID());
  net_log_for_actual_request.AddEventReferencingSource(
      net::NetLogEventType::CORS_PREFLIGHT_URL_REQUEST,
      net_log_source_for_preflight);
  // Set `net_log_create_info` to specify NetLog source used in preflight
  // URL Request.
  preflight_request->net_log_create_info = net_log_source_for_preflight;

  return preflight_request;
}

// Performs a CORS access check on the CORS-preflight response parameters.
// According to the note at https://fetch.spec.whatwg.org/#cors-preflight-fetch
// step 6, even for a preflight check, `credentials_mode` should be checked on
// the actual request rather than preflight one.
base::expected<void, CorsErrorStatus> CheckPreflightAccess(
    const GURL& response_url,
    const int response_status_code,
    const std::optional<std::string>& allow_origin_header,
    const std::optional<std::string>& allow_credentials_header,
    mojom::CredentialsMode actual_credentials_mode,
    const url::Origin& origin) {
  // Step 7 of https://fetch.spec.whatwg.org/#cors-preflight-fetch
  auto cors_result =
      CheckAccess(response_url, allow_origin_header, allow_credentials_header,
                  actual_credentials_mode, origin);
  const bool has_ok_status = IsSuccessfulStatus(response_status_code);

  if (cors_result.has_value()) {
    if (has_ok_status) {
      return base::ok();
    }
    return base::unexpected(
        CorsErrorStatus(mojom::CorsError::kPreflightInvalidStatus));
  }

  // Prefer using a preflight specific error code.
  const auto map_to_preflight_error_codes = [](mojom::CorsError error) {
    switch (error) {
      case mojom::CorsError::kWildcardOriginNotAllowed:
        return mojom::CorsError::kPreflightWildcardOriginNotAllowed;
      case mojom::CorsError::kMissingAllowOriginHeader:
        return mojom::CorsError::kPreflightMissingAllowOriginHeader;
      case mojom::CorsError::kMultipleAllowOriginValues:
        return mojom::CorsError::kPreflightMultipleAllowOriginValues;
      case mojom::CorsError::kInvalidAllowOriginValue:
        return mojom::CorsError::kPreflightInvalidAllowOriginValue;
      case mojom::CorsError::kAllowOriginMismatch:
        return mojom::CorsError::kPreflightAllowOriginMismatch;
      case mojom::CorsError::kInvalidAllowCredentials:
        return mojom::CorsError::kPreflightInvalidAllowCredentials;
      default:
        NOTREACHED();
    }
  };
  cors_result.error().cors_error =
      map_to_preflight_error_codes(cors_result.error().cors_error);
  return cors_result;
}

// Checks errors for the "Access-Control-Allow-Private-Network" header.
//
// See the CORS-preflight fetch algorithm modifications laid out in the Private
// Network Access spec, in step 4 of the CORS preflight section as of writing:
// https://wicg.github.io/private-network-access/#cors-preflight
std::optional<CorsErrorStatus> CheckAllowPrivateNetworkHeader(
    const mojom::URLResponseHead& head,
    const ResourceRequest& original_request) {
  if (original_request.target_ip_address_space ==
      mojom::IPAddressSpace::kUnknown) {
    // Not a Private Network Access preflight.
    return std::nullopt;
  }

  std::optional<std::string> header = GetHeaderString(
      head.headers, header_names::kAccessControlAllowPrivateNetwork);
  if (!header) {
    CorsErrorStatus status(
        mojom::CorsError::kPreflightMissingAllowPrivateNetwork);
    status.target_address_space = original_request.target_ip_address_space;
    return status;
  }

  if (*header != kLowerCaseTrue) {
    CorsErrorStatus status(
        mojom::CorsError::kPreflightInvalidAllowPrivateNetwork, *header);
    status.target_address_space = original_request.target_ip_address_space;
    return status;
  }

  return std::nullopt;
}

std::unique_ptr<PreflightResult> CreatePreflightResult(
    const GURL& final_url,
    const mojom::URLResponseHead& head,
    const ResourceRequest& original_request,
    bool tainted,
    PrivateNetworkAccessPreflightBehavior private_network_access_behavior,
    const mojom::ClientSecurityStatePtr& client_security_state,
    base::WeakPtr<mojo::Remote<mojom::DevToolsObserver>> devtools_observer,
    const PreflightController::PreflightMode& preflight_mode,
    std::optional<CorsErrorStatus>* detected_error_status) {
  CHECK(detected_error_status);

  auto check_result = CheckPreflightAccess(
      final_url, head.headers ? head.headers->response_code() : 0,
      GetHeaderString(head.headers, header_names::kAccessControlAllowOrigin),
      GetHeaderString(head.headers,
                      header_names::kAccessControlAllowCredentials),
      original_request.credentials_mode,
      tainted ? url::Origin() : *original_request.request_initiator);
  if (!check_result.has_value()) {
    *detected_error_status = std::move(check_result.error());
    return nullptr;
  }

  if (preflight_mode.Has(
          PreflightController::PreflightType::kPrivateNetworkAccess)) {
    *detected_error_status =
        CheckAllowPrivateNetworkHeader(head, original_request);
    if (detected_error_status->has_value() &&
        ShouldEnforcePrivateNetworkAccessHeader(
            private_network_access_behavior)) {
      return nullptr;
    }
  }

  std::optional<mojom::CorsError> error;
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

std::optional<CorsErrorStatus> CheckPreflightResult(
    const PreflightResult& result,
    const ResourceRequest& original_request,
    NonWildcardRequestHeadersSupport non_wildcard_request_headers_support,
    bool acam_preflight_spec_conformant,
    const PreflightController::PreflightMode& preflight_mode) {
  std::optional<CorsErrorStatus> status = result.EnsureAllowedCrossOriginMethod(
      original_request.method, acam_preflight_spec_conformant);
  if (status)
    return status;

  if (preflight_mode.Has(PreflightController::PreflightType::kCors)) {
    return result.EnsureAllowedCrossOriginHeaders(
        original_request.headers, original_request.is_revalidating,
        non_wildcard_request_headers_support);
  }

  return std::nullopt;
}

}  // namespace

const char kPreflightErrorHistogramName[] = "Net.Cors.PreflightCheckError2";
const char kPreflightWarningHistogramName[] = "Net.Cors.PreflightCheckWarning";

class PreflightController::PreflightLoader final {
 public:
  PreflightLoader(
      PreflightController* controller,
      CompletionCallback completion_callback,
      const ResourceRequest& request,
      WithTrustedHeaderClient with_trusted_header_client,
      NonWildcardRequestHeadersSupport non_wildcard_request_headers_support,
      PrivateNetworkAccessPreflightBehavior private_network_access_behavior,
      bool tainted,
      const net::NetworkTrafficAnnotationTag& annotation_tag,
      const net::NetworkIsolationKey& network_isolation_key,
      mojom::ClientSecurityStatePtr client_security_state,
      base::WeakPtr<mojo::Remote<mojom::DevToolsObserver>> devtools_observer,
      const net::NetLogWithSource net_log,
      bool acam_preflight_spec_conformant,
      mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
          url_loader_network_service_observer,
      const PreflightMode& preflight_mode)
      : controller_(controller),
        completion_callback_(std::move(completion_callback)),
        original_request_(request),
        non_wildcard_request_headers_support_(
            non_wildcard_request_headers_support),
        private_network_access_behavior_(private_network_access_behavior),
        tainted_(tainted),
        network_isolation_key_(network_isolation_key),
        client_security_state_(std::move(client_security_state)),
        devtools_observer_(std::move(devtools_observer)),
        net_log_(net_log),
        acam_preflight_spec_conformant_(acam_preflight_spec_conformant),
        url_loader_network_service_observer_(
            std::move(url_loader_network_service_observer)),
        preflight_mode_(preflight_mode) {
    if (devtools_observer_)
      devtools_request_id_ = base::UnguessableToken::Create();
    auto preflight_request = CreatePreflightRequest(
        request, tainted, net_log, devtools_request_id_, preflight_mode);

    if (devtools_observer_ && *devtools_observer_) {
      DCHECK(devtools_request_id_);
      network::mojom::URLRequestDevToolsInfoPtr request_info =
          network::ExtractDevToolsInfo(*preflight_request);
      (*devtools_observer_)
          ->OnCorsPreflightRequest(
              *devtools_request_id_, preflight_request->headers,
              std::move(request_info), original_request_.url,
              original_request_.devtools_request_id.value_or(""));
    }
    loader_ =
        SimpleURLLoader::Create(std::move(preflight_request), annotation_tag);
    uint32_t options = mojom::kURLLoadOptionAsCorsPreflight;
    if (with_trusted_header_client) {
      options |= mojom::kURLLoadOptionUseHeaderClient;
    }
    loader_->SetURLLoaderFactoryOptions(options);

    // When private network access preflights are sent in warning mode, we
    // should not wait around forever for a response. Certain servers never
    // respond, and that should not fail the overall request. Instead, we should
    // wait a short while then move on. See also https://crbug.com/1299382.
    if (private_network_access_behavior_ ==
            PrivateNetworkAccessPreflightBehavior::kWarnWithTimeout &&
        base::FeatureList::IsEnabled(
            features::kPrivateNetworkAccessPreflightShortTimeout)) {
      loader_->SetTimeoutDuration(base::Milliseconds(200));
    }
  }

  PreflightLoader(const PreflightLoader&) = delete;
  PreflightLoader& operator=(const PreflightLoader&) = delete;

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
  void HandleRedirect(const GURL& url_before_redirect,
                      const net::RedirectInfo& redirect_info,
                      const network::mojom::URLResponseHead& response_head,
                      std::vector<std::string>* to_be_removed_headers) {
    if (devtools_observer_ && *devtools_observer_) {
      DCHECK(devtools_request_id_);
      (*devtools_observer_)
          ->OnCorsPreflightRequestCompleted(
              *devtools_request_id_,
              network::URLLoaderCompletionStatus(net::ERR_INVALID_REDIRECT));
    }

    std::move(completion_callback_)
        .Run(net::ERR_FAILED,
             CorsErrorStatus(mojom::CorsError::kPreflightDisallowedRedirect),
             false);

    RemoveFromController();
    // `this` is deleted here.
  }

  void HandleResponseHeader(const GURL& final_url,
                            const mojom::URLResponseHead& head) {
    if (devtools_observer_ && *devtools_observer_) {
      DCHECK(devtools_request_id_);
      mojom::URLResponseHeadDevToolsInfoPtr head_info =
          ExtractDevToolsInfo(head);
      (*devtools_observer_)
          ->OnCorsPreflightResponse(*devtools_request_id_,
                                    original_request_.url,
                                    std::move(head_info));
      (*devtools_observer_)
          ->OnCorsPreflightRequestCompleted(
              *devtools_request_id_,
              network::URLLoaderCompletionStatus(net::OK));
    }

    std::optional<CorsErrorStatus> detected_error_status;
    std::unique_ptr<PreflightResult> result = CreatePreflightResult(
        final_url, head, original_request_, tainted_,
        private_network_access_behavior_, client_security_state_,
        devtools_observer_, preflight_mode_, &detected_error_status);

    if (!result) {
      std::move(completion_callback_)
          .Run(net::ERR_FAILED, std::move(detected_error_status), false);
      return;
    }

    // NOTE: `detected_error_status` may be non-nullopt if a PNA warning was
    // encountered in `CreatePreflightResult()`.

    // Only log if there is a result to log.
    net_log_.AddEvent(net::NetLogEventType::CORS_PREFLIGHT_RESULT,
                      [&result] { return result->NetLogParams(); });

    // Preflight succeeded. Check `original_request_` with `result`.
    net::Error net_error = net::OK;
    std::optional<CorsErrorStatus> check_error_status = CheckPreflightResult(
        *result, original_request_, non_wildcard_request_headers_support_,
        acam_preflight_spec_conformant_, preflight_mode_);

    // Avoid overwriting if `CheckPreflightResult()` succeeds, just in case
    // there was a PNA warning in `detected_error_status`.
    // TODO(crbug.com/40204695): Simplify this by always overwriting
    // `detected_error_status` once preflights are always enforced.
    if (check_error_status.has_value()) {
      net_error = net::ERR_FAILED;
      detected_error_status = std::move(check_error_status);
    }

    // Check if we need user permission to access the private network. This
    // only happens if we skipped the mixed content check before sending the
    // preflight.
    const bool needs_permission =
        client_security_state_ &&
        PrivateNetworkAccessChecker::NeedPermission(
            original_request_.url,
            client_security_state_->is_web_secure_context,
            original_request_.required_ip_address_space);

    if (!needs_permission) {
      FinishHandleResponseHeader(net_error, std::move(detected_error_status),
                                 std::move(result));
      return;
    }

    // Check if it is valid to show the permission prompt, which means:
    // * The target IP address space shouldn't be unknown or public.
    // * The preflight response contains `Private-Network-Access-Id` and
    // `Private-Network-Access-Name` headers to claim its identity.
    // * Able to access permission in the browser process from
    // URLLoaderNetworkService.
    std::optional<std::string> id =
        GetHeaderString(head.headers, header_names::kPrivateNetworkDeviceId);
    std::optional<std::string> name =
        GetHeaderString(head.headers, header_names::kPrivateNetworkDeviceName);

    // TODO(crbug.com/40272755): `target_ip_address_space` should be
    // checked in `CorsURLLoaderFactory`. Remove the following bit after that.
    if (!url_loader_network_service_observer_ ||
        original_request_.target_ip_address_space ==
            mojom::IPAddressSpace::kUnknown ||
        original_request_.target_ip_address_space ==
            mojom::IPAddressSpace::kPublic) {
      FinishHandleResponseHeader(
          net::ERR_FAILED,
          CorsErrorStatus(
              mojom::CorsError::kPrivateNetworkAccessPermissionUnavailable),
          std::move(result));
      return;
    }

    // Ask for private network access permission.
    // base::Unretained() is safe because once HandleResponseHeader is called,
    // PreflightController will at least keep alive until completion being
    // called as a result of HandlePrivateNetworkAccessPermissionResult being
    // called.
    (*url_loader_network_service_observer_)
        .OnPrivateNetworkAccessPermissionRequired(
            std::move(original_request_.url),
            std::move(head.remote_endpoint.address()), id, name,
            base::BindOnce(
                &PreflightLoader::HandlePrivateNetworkAccessPermissionResult,
                base::Unretained(this), net_error,
                std::move(detected_error_status), std::move(result)));
    permission_state_ = PermissionState::kRequested;
  }

  void HandlePrivateNetworkAccessPermissionResult(
      net::Error net_error,
      std::optional<CorsErrorStatus> detected_error_status,
      std::unique_ptr<PreflightResult> result,
      bool permission_granted) {
    if (!permission_granted) {
      net_error = net::ERR_FAILED;
      detected_error_status = CorsErrorStatus(
          mojom::CorsError::kPrivateNetworkAccessPermissionDenied);
    }
    FinishHandleResponseHeader(std::move(net_error),
                               std::move(detected_error_status),
                               std::move(result));
  }

  void FinishHandleResponseHeader(
      net::Error net_error,
      std::optional<CorsErrorStatus> detected_error_status,
      std::unique_ptr<PreflightResult> result) {
    bool has_authorization_covered_by_wildcard =
        result->HasAuthorizationCoveredByWildcard(original_request_.headers);

    if (!(original_request_.load_flags & net::LOAD_DISABLE_CACHE) &&
        net_error == net::OK) {
      controller_->AppendToCache(*original_request_.request_initiator,
                                 original_request_.url, network_isolation_key_,
                                 original_request_.target_ip_address_space,
                                 std::move(result));
    }

    std::move(completion_callback_)
        .Run(net_error, detected_error_status,
             has_authorization_covered_by_wildcard);

    if (permission_state_ ==
        PermissionState::kRequestedAndFinishedLoadingBody) {
      RemoveFromController();
      // `this` is deleted here.
    }
  }

  void HandleResponseBody(std::unique_ptr<std::string> response_body) {
    const int error = loader_->NetError();
    const std::optional<URLLoaderCompletionStatus>& status =
        loader_->CompletionStatus();

    if (permission_state_ == PermissionState::kRequested) {
      permission_state_ = PermissionState::kRequestedAndFinishedLoadingBody;
      return;
    }

    if (!completion_callback_.is_null()) {
      // As HandleResponseHeader() isn't called due to a request failure, such
      // as unknown hosts. unreachable remote, reset by peer, and so on, we
      // still hold `completion_callback_` to invoke.
      if (devtools_observer_ && *devtools_observer_) {
        DCHECK(devtools_request_id_);
        (*devtools_observer_)
            ->OnCorsPreflightRequestCompleted(
                *devtools_request_id_,
                status.has_value() ? *status
                                   : network::URLLoaderCompletionStatus(error));
      }
      std::move(completion_callback_)
          .Run(error,
               status.has_value() ? status->cors_error_status : std::nullopt,
               false);
    }

    RemoveFromController();
    // `this` is deleted here.
  }

  // Removes `this` instance from `controller_`. Once the method returns, `this`
  // is already removed.
  void RemoveFromController() { controller_->RemoveLoader(this); }

  // PreflightController owns all PreflightLoader instances, and should outlive.
  const raw_ptr<PreflightController> controller_;

  // Holds SimpleURLLoader instance for the CORS-preflight request.
  std::unique_ptr<SimpleURLLoader> loader_;

  // Holds caller's information.
  PreflightController::CompletionCallback completion_callback_;
  const ResourceRequest original_request_;

  const NonWildcardRequestHeadersSupport non_wildcard_request_headers_support_;
  const PrivateNetworkAccessPreflightBehavior private_network_access_behavior_;
  const bool tainted_;
  std::optional<base::UnguessableToken> devtools_request_id_;
  const net::NetworkIsolationKey network_isolation_key_;
  const mojom::ClientSecurityStatePtr client_security_state_;
  base::WeakPtr<mojo::Remote<mojom::DevToolsObserver>> devtools_observer_;
  const net::NetLogWithSource net_log_;
  const bool acam_preflight_spec_conformant_;
  mojo::Remote<mojom::URLLoaderNetworkServiceObserver>
      url_loader_network_service_observer_;

  enum class PermissionState {
    // Private Network Device permission haven't been requested.
    kNotRequired,
    // Private Network Device permission requested but no response yet.
    kRequested,
    // Private Network Device permission requested and finished loading body.
    // Able to complete handling the response after permission callback.
    kRequestedAndFinishedLoadingBody,
  };

  // permission_state_ is always `kNotRequired` if
  // network::feature::kPrivateNetworkAccessPermissionPrompt is disabled.
  PermissionState permission_state_ = PermissionState::kNotRequired;

  // Whether the preflight is only for CORS or Private Network Access  or both.
  PreflightMode preflight_mode_;
};

// static
std::unique_ptr<ResourceRequest>
PreflightController::CreatePreflightRequestForTesting(
    const ResourceRequest& request,
    bool tainted) {
  return CreatePreflightRequest(
      request, tainted,
      net::NetLogWithSource::Make(net::NetLog::Get(),
                                  net::NetLogSourceType::URL_REQUEST),
      /*devtools_request_id=*/std::nullopt,
      PreflightMode{PreflightType ::kCors});
}

// static
std::unique_ptr<PreflightResult>
PreflightController::CreatePreflightResultForTesting(
    const GURL& final_url,
    const mojom::URLResponseHead& head,
    const ResourceRequest& original_request,
    bool tainted,
    PrivateNetworkAccessPreflightBehavior private_network_access_behavior,
    std::optional<CorsErrorStatus>* detected_error_status) {
  return CreatePreflightResult(
      final_url, head, original_request, tainted,
      private_network_access_behavior,
      /*client_security_state=*/nullptr,
      /*devtools_observer=*/
      base::WeakPtr<mojo::Remote<mojom::DevToolsObserver>>(),
      PreflightMode{PreflightType ::kCors}, detected_error_status);
}

// static
base::expected<void, CorsErrorStatus>
PreflightController::CheckPreflightAccessForTesting(
    const GURL& response_url,
    const int response_status_code,
    const std::optional<std::string>& allow_origin_header,
    const std::optional<std::string>& allow_credentials_header,
    mojom::CredentialsMode actual_credentials_mode,
    const url::Origin& origin) {
  return CheckPreflightAccess(response_url, response_status_code,
                              allow_origin_header, allow_credentials_header,
                              actual_credentials_mode, origin);
}

PreflightController::PreflightController(NetworkService* network_service)
    : network_service_(network_service) {}

PreflightController::~PreflightController() = default;

void PreflightController::PerformPreflightCheck(
    CompletionCallback callback,
    const ResourceRequest& request,
    WithTrustedHeaderClient with_trusted_header_client,
    NonWildcardRequestHeadersSupport non_wildcard_request_headers_support,
    PrivateNetworkAccessPreflightBehavior private_network_access_behavior,
    bool tainted,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    mojom::URLLoaderFactory* loader_factory,
    const net::IsolationInfo& isolation_info,
    mojom::ClientSecurityStatePtr client_security_state,
    base::WeakPtr<mojo::Remote<mojom::DevToolsObserver>> devtools_observer,
    const net::NetLogWithSource& net_log,
    bool acam_preflight_spec_conformant,
    mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver>
        url_loader_network_service_observer,
    const PreflightMode& preflight_mode) {
  DCHECK(request.request_initiator);

  const net::NetworkIsolationKey& network_isolation_key =
      !isolation_info.IsEmpty()
          ? isolation_info.network_isolation_key()
          : request.trusted_params.has_value()
                ? request.trusted_params->isolation_info.network_isolation_key()
                : net::NetworkIsolationKey();
  if (!RetrieveCacheFlags(request.load_flags) &&
      cache_.CheckIfRequestCanSkipPreflight(
          request.request_initiator.value(), request.url, network_isolation_key,
          request.target_ip_address_space, request.credentials_mode,
          request.method, request.headers, request.is_revalidating, net_log,
          acam_preflight_spec_conformant)) {
    std::move(callback).Run(net::OK, std::nullopt, false);
    return;
  }

  auto emplaced_pair = loaders_.emplace(std::make_unique<PreflightLoader>(
      this, std::move(callback), request, with_trusted_header_client,
      non_wildcard_request_headers_support, private_network_access_behavior,
      tainted, annotation_tag, network_isolation_key,
      std::move(client_security_state), devtools_observer, net_log,
      acam_preflight_spec_conformant,
      std::move(url_loader_network_service_observer), preflight_mode));
  (*emplaced_pair.first)->Request(loader_factory);
}

void PreflightController::ClearCorsPreflightCache(
    mojom::ClearDataFilterPtr url_filter) {
  cache_.ClearCache(std::move(url_filter));
}

void PreflightController::RemoveLoader(PreflightLoader* loader) {
  auto it = loaders_.find(loader);
  CHECK(it != loaders_.end(), base::NotFatalUntil::M130);
  loaders_.erase(it);
}

void PreflightController::AppendToCache(
    const url::Origin& origin,
    const GURL& url,
    const net::NetworkIsolationKey& network_isolation_key,
    mojom::IPAddressSpace target_ip_address_space,
    std::unique_ptr<PreflightResult> result) {
  cache_.AppendEntry(origin, url, network_isolation_key,
                     target_ip_address_space, std::move(result));
}

}  // namespace network::cors

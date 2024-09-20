// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cors/cors_url_loader.h"

#include <optional>
#include <sstream>
#include <utility>

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/dcheck_is_on.h"
#include "base/debug/crash_logging.h"
#include "base/debug/dump_without_crashing.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/types/optional_util.h"
#include "net/base/load_flags.h"
#include "net/cookies/cookie_partition_key.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_status_code.h"
#include "net/log/net_log_values.h"
#include "net/shared_dictionary/shared_dictionary.h"
#include "net/url_request/url_request_context.h"
#include "services/network/cookie_manager.h"
#include "services/network/cors/cors_url_loader_factory.h"
#include "services/network/cors/cors_util.h"
#include "services/network/cors/preflight_controller.h"
#include "services/network/network_context.h"
#include "services/network/private_network_access_checker.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/record_ontransfersizeupdate_utils.h"
#include "services/network/public/cpp/request_mode.h"
#include "services/network/public/cpp/timing_allow_origin_parser.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/early_hints.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "services/network/public/mojom/ip_address_space.mojom.h"
#include "services/network/public/mojom/shared_dictionary_error.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/shared_dictionary/shared_dictionary_access_checker.h"
#include "services/network/shared_dictionary/shared_dictionary_constants.h"
#include "services/network/shared_dictionary/shared_dictionary_data_pipe_writer.h"
#include "services/network/shared_dictionary/shared_dictionary_manager.h"
#include "services/network/shared_dictionary/shared_dictionary_storage.h"
#include "services/network/shared_dictionary/shared_dictionary_writer.h"
#include "services/network/shared_storage/shared_storage_header_utils.h"
#include "services/network/trust_tokens/trust_token_operation_metrics_recorder.h"
#include "services/network/url_loader.h"
#include "services/network/url_loader_factory.h"
#include "url/scheme_host_port.h"
#include "url/url_util.h"

namespace network::cors {

namespace {

enum class PreflightRequiredReason {
  kPrivateNetworkAccess,
  kCorsWithForcedPreflightMode,
  kDisallowedMethod,
  kDisallowedHeader
};

std::optional<PreflightRequiredReason> NeedsPrivateNetworkAccessPreflight(
    const ResourceRequest& request) {
  if (request.target_ip_address_space != mojom::IPAddressSpace::kUnknown) {
    // Force a preflight after a private network request was detected. See the
    // HTTP-no-service-worker fetch algorithm defined in the Private Network
    // Access spec:
    // https://wicg.github.io/private-network-access/#http-no-service-worker-fetch
    return PreflightRequiredReason::kPrivateNetworkAccess;
  }

  return std::nullopt;
}

std::optional<PreflightRequiredReason> NeedsCorsPreflight(
    const ResourceRequest& request) {
  if (!IsCorsEnabledRequestMode(request.mode))
    return std::nullopt;

  if (request.mode == mojom::RequestMode::kCorsWithForcedPreflight) {
    return PreflightRequiredReason::kCorsWithForcedPreflightMode;
  }

  if (request.cors_preflight_policy ==
      mojom::CorsPreflightPolicy::kPreventPreflight) {
    return std::nullopt;
  }

  if (!IsCorsSafelistedMethod(request.method))
    return PreflightRequiredReason::kDisallowedMethod;

  if (!CorsUnsafeNotForbiddenRequestHeaderNames(
           request.headers.GetHeaderVector(), request.is_revalidating)
           .empty())
    return PreflightRequiredReason::kDisallowedHeader;

  return std::nullopt;
}

// Returns std::nullopt when a preflight isn't needed. Otherwise returns the
// reason why a preflight is needed.
std::optional<PreflightRequiredReason> NeedsPreflight(
    const ResourceRequest& request) {
  std::optional<PreflightRequiredReason> needs_pna_preflight =
      NeedsPrivateNetworkAccessPreflight(request);
  if (needs_pna_preflight.has_value()) {
    return needs_pna_preflight;
  }
  return NeedsCorsPreflight(request);
}

base::Value::Dict NetLogCorsURLLoaderStartParams(
    const ResourceRequest& request) {
  std::string cors_preflight_policy;
  switch (request.cors_preflight_policy) {
    case mojom::CorsPreflightPolicy::kConsiderPreflight:
      cors_preflight_policy = "consider_preflight";
      break;
    case mojom::CorsPreflightPolicy::kPreventPreflight:
      cors_preflight_policy = "prevent_preflight";
      break;
  }

  return base::Value::Dict()
      .Set("url", request.url.possibly_invalid_spec())
      .Set("method", request.method)
      .Set("headers", net::NetLogStringValue(request.headers.ToString()))
      .Set("is_revalidating", request.is_revalidating)
      .Set("cors_preflight_policy", cors_preflight_policy);
}

base::Value::Dict NetLogPreflightRequiredParams(
    std::optional<PreflightRequiredReason> preflight_required_reason) {
  auto dict = base::Value::Dict().Set("preflight_required",
                                      preflight_required_reason.has_value());
  if (preflight_required_reason) {
    std::string preflight_required_reason_param;
    switch (preflight_required_reason.value()) {
      case PreflightRequiredReason::kPrivateNetworkAccess:
        preflight_required_reason_param = "private_network_access";
        break;
      case PreflightRequiredReason::kCorsWithForcedPreflightMode:
        preflight_required_reason_param = "cors_with_forced_preflight_mode";
        break;
      case PreflightRequiredReason::kDisallowedMethod:
        preflight_required_reason_param = "disallowed_method";
        break;
      case PreflightRequiredReason::kDisallowedHeader:
        preflight_required_reason_param = "disallowed_header";
        break;
    }
    dict.Set("preflight_required_reason", preflight_required_reason_param);
  }
  return dict;
}

// Returns net log params for the `CORS_PREFLIGHT_ERROR` event type.
base::Value::Dict NetLogPreflightErrorParams(
    int net_error,
    const std::optional<CorsErrorStatus>& status) {
  auto dict =
      base::Value::Dict().Set("error", net::ErrorToShortString(net_error));
  if (status) {
    dict.Set("cors-error", static_cast<int>(status->cors_error));
    if (!status->failed_parameter.empty()) {
      dict.Set("failed-parameter", status->failed_parameter);
    }
  }

  return dict;
}

// Returns the response tainting value
// (https://fetch.spec.whatwg.org/#concept-request-response-tainting) for a
// request and the CORS flag, as specified in
// https://fetch.spec.whatwg.org/#main-fetch.
// Keep this in sync with the identical function
// blink::cors::CalculateResponseTainting.
mojom::FetchResponseType CalculateResponseTainting(
    const GURL& url,
    mojom::RequestMode request_mode,
    const std::optional<url::Origin>& origin,
    const std::optional<url::Origin>& isolated_world_origin,
    bool cors_flag,
    bool tainted_origin,
    const OriginAccessList& origin_access_list) {
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
  // 1) `isolated_world_origin` (if this is a request from a content
  //    script;  in this case there is no point looking at (2) below.
  // 2) `origin` (if this is a request from an extension
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
        (!origin->IsSameOriginWith(url) &&
         origin_access_list.CheckAccessState(source_origin, url) !=
             OriginAccessList::AccessState::kAllowed)) {
      return mojom::FetchResponseType::kOpaque;
    }
  }
  return mojom::FetchResponseType::kBasic;
}

// Given a redirected-to URL, checks if the location is allowed
// according to CORS. That is:
// - the URL has a CORS supported scheme and
// - the URL does not contain the userinfo production.
std::optional<CorsErrorStatus> CheckRedirectLocation(
    const GURL& url,
    mojom::RequestMode request_mode,
    const std::optional<url::Origin>& origin,
    bool cors_flag,
    bool tainted) {
  // If `actualResponse`’s location URL’s scheme is not an HTTP(S) scheme,
  // then return a network error.
  // This should be addressed in //net.

  // Note: The redirect count check is done elsewhere.

  const bool url_has_credentials = url.has_username() || url.has_password();
  // If `request`’s mode is "cors", `actualResponse`’s location URL includes
  // credentials, and either `request`’s tainted origin flag is set or
  // `request`’s origin is not same origin with `actualResponse`’s location
  // URL’s origin, then return a network error.
  DCHECK(!IsCorsEnabledRequestMode(request_mode) || origin);
  if (IsCorsEnabledRequestMode(request_mode) && url_has_credentials &&
      (tainted || !origin->IsSameOriginWith(url))) {
    return CorsErrorStatus(mojom::CorsError::kRedirectContainsCredentials);
  }

  // If CORS flag is set and `actualResponse`’s location URL includes
  // credentials, then return a network error.
  if (cors_flag && url_has_credentials)
    return CorsErrorStatus(mojom::CorsError::kRedirectContainsCredentials);

  return std::nullopt;
}

void RecordNetworkLoaderCompletionTime(const char* suffix,
                                       base::TimeDelta elapsed) {
  base::UmaHistogramTimes(
      base::StrCat({"NetworkService.NetworkLoaderCompletionTime2.", suffix}),
      elapsed);
}

constexpr const char kTimingAllowOrigin[] = "Timing-Allow-Origin";

}  // namespace

CorsURLLoader::CorsURLLoader(
    mojo::PendingReceiver<mojom::URLLoader> loader_receiver,
    int32_t process_id,
    int32_t request_id,
    uint32_t options,
    DeleteCallback delete_callback,
    ResourceRequest resource_request,
    bool ignore_isolated_world_origin,
    bool skip_cors_enabled_scheme_check,
    mojo::PendingRemote<mojom::URLLoaderClient> client,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    mojom::URLLoaderFactory* network_loader_factory,
    URLLoaderFactory* sync_network_loader_factory,
    const OriginAccessList* origin_access_list,
    bool allow_any_cors_exempt_header,
    HasFactoryOverride has_factory_override,
    const net::IsolationInfo& isolation_info,
    mojo::PendingRemote<mojom::DevToolsObserver> devtools_observer,
    const mojom::ClientSecurityState* factory_client_security_state,
    mojo::Remote<mojom::URLLoaderNetworkServiceObserver>*
        url_loader_network_service_observer,
    const CrossOriginEmbedderPolicy& cross_origin_embedder_policy,
    scoped_refptr<SharedDictionaryStorage> shared_dictionary_storage,
    raw_ptr<mojom::SharedDictionaryAccessObserver> shared_dictionary_observer,
    NetworkContext* context,
    net::CookieSettingOverrides factory_cookie_setting_overrides)
    : receiver_(this, std::move(loader_receiver)),
      process_id_(process_id),
      request_id_(request_id),
      options_(options),
      delete_callback_(std::move(delete_callback)),
      network_loader_factory_(network_loader_factory),
      sync_network_loader_factory_(sync_network_loader_factory),
      request_(std::move(resource_request)),
      forwarding_client_(std::move(client)),
      traffic_annotation_(traffic_annotation),
      origin_access_list_(origin_access_list),
      skip_cors_enabled_scheme_check_(skip_cors_enabled_scheme_check),
      allow_any_cors_exempt_header_(allow_any_cors_exempt_header),
      has_factory_override_(has_factory_override),
      isolation_info_(isolation_info),
      factory_client_security_state_(factory_client_security_state),
      url_loader_network_service_observer_(url_loader_network_service_observer),
      cross_origin_embedder_policy_(cross_origin_embedder_policy),
      devtools_observer_(std::move(devtools_observer)),
      weak_devtools_observer_factory_(&devtools_observer_),
      // CORS preflight related events are logged in a series of URL_REQUEST
      // logs.
      net_log_(net::NetLogWithSource::Make(net::NetLog::Get(),
                                           net::NetLogSourceType::URL_REQUEST)),
      context_(context),
      shared_dictionary_storage_(std::move(shared_dictionary_storage)),
      shared_dictionary_observer_(shared_dictionary_observer),
      factory_cookie_setting_overrides_(factory_cookie_setting_overrides) {
  TRACE_EVENT("loading", "CorsURLLoader::CorsURLLoader",
              perfetto::Flow::ProcessScoped(net_log_.source().id));
  CHECK(url_loader_network_service_observer_ != nullptr);
  if (ignore_isolated_world_origin)
    request_.isolated_world_origin = std::nullopt;

  receiver_.set_disconnect_handler(
      base::BindOnce(&CorsURLLoader::OnMojoDisconnect, base::Unretained(this)));
  request_.net_log_create_info = net_log_.source();
  DCHECK(network_loader_factory_);
  DCHECK(origin_access_list_);
  SetCorsFlagIfNeeded();

  if (shared_dictionary_storage_) {
    if (request_.mode != mojom::RequestMode::kNoCors) {
      request_.load_flags |= net::LOAD_CAN_USE_SHARED_DICTIONARY;
    } else if (request_.request_initiator &&
               request_.request_initiator->IsSameOriginWith(request_.url)) {
      // For no-cors mode requests, we can use shared dictionaries only for same
      // origin requests. When redirected to another origin,
      // net::URLRequest::Redirect() disables the LOAD_CAN_USE_SHARED_DICTIONARY
      // flag.
      request_.load_flags |= net::LOAD_CAN_USE_SHARED_DICTIONARY;
      request_.load_flags |=
          net::LOAD_DISABLE_SHARED_DICTIONARY_AFTER_CROSS_ORIGIN_REDIRECT;
    }
    // This is intended to load the dictionary as soon as possible. Without
    // this, the dictionary will be loaded from the disk when
    // `HttpNetworkTransaction` builds the request header just before sending it
    // to the server.
    shared_dictionary_storage_->GetDictionary(
        request_.url, request_.destination,
        base::BindOnce(
            [](base::WeakPtr<CorsURLLoader> loader,
               scoped_refptr<net::SharedDictionary> shared_dictionary) {
              if (loader) {
                loader->shared_dictionary_ = std::move(shared_dictionary);
              }
            },
            weak_factory_.GetWeakPtr()));
  }
}

CorsURLLoader::~CorsURLLoader() {
  TRACE_EVENT("loading", "CorsURLLoader::~CorsURLLoader",
              perfetto::Flow::ProcessScoped(net_log_.source().id));
  // Reset pipes first to ignore possible subsequent callback invocations
  // caused by `network_loader_`
  network_client_receiver_.reset();
}

void CorsURLLoader::Start() {
  TRACE_EVENT("loading", "CorsURLLoader::Start",
              perfetto::Flow::ProcessScoped(net_log_.source().id));
  if (fetch_cors_flag_ && IsCorsEnabledRequestMode(request_.mode)) {
    // Username and password should be stripped in a CORS-enabled request.
    if (request_.url.has_username() || request_.url.has_password()) {
      GURL::Replacements replacements;
      replacements.SetUsernameStr("");
      replacements.SetPasswordStr("");
      request_.url = request_.url.ReplaceComponents(replacements);
    }
  }

  last_response_url_ = request_.url;

  net_log_.BeginEvent(net::NetLogEventType::CORS_REQUEST,
                      [&] { return NetLogCorsURLLoaderStartParams(request_); });
  StartRequest();
}

void CorsURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const net::HttpRequestHeaders& modified_headers,
    const net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<GURL>& new_url) {
  // If this is a navigation from a renderer, then its a service worker
  // passthrough of a navigation request.  Since this case uses manual
  // redirect mode FollowRedirect() should never be called.
  if (process_id_ != mojom::kBrowserProcessId &&
      request_.mode == mojom::RequestMode::kNavigate) {
    mojo::ReportBadMessage(
        "CorsURLLoader: navigate from non-browser-process should not call "
        "FollowRedirect");
    HandleComplete(URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  if (!network_loader_ || !deferred_redirect_url_) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_FAILED));
    return;
  }

  if (new_url && (new_url->DeprecatedGetOriginAsURL() !=
                  deferred_redirect_url_->DeprecatedGetOriginAsURL())) {
    NOTREACHED_IN_MIGRATION()
        << "Can only change the URL within the same origin.";
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

  // Does not allow modifying headers that are stored in `cors_exempt_headers`.
  for (const auto& header : modified_headers.GetHeaderVector()) {
    if (request_.cors_exempt_headers.HasHeader(header.key)) {
      LOG(WARNING) << "A client is trying to modify header value for '"
                   << header.key << "', but it is not permitted.";
      HandleComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
      return;
    }
  }

  for (const auto& name : removed_headers) {
    request_.headers.RemoveHeader(name);
    request_.cors_exempt_headers.RemoveHeader(name);
  }
  request_.headers.MergeFrom(modified_headers);

  if (GetSecSharedStorageWritableHeader(modified_headers)) {
    request_.shared_storage_writable_eligible = true;
  } else if (base::Contains(removed_headers, kSecSharedStorageWritableHeader)) {
    request_.shared_storage_writable_eligible = false;
  }

  if (!allow_any_cors_exempt_header_ &&
      !CorsURLLoaderFactory::IsValidCorsExemptHeaders(
          *context_->cors_exempt_header_list(), modified_cors_exempt_headers)) {
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

  // When we follow a redirect, we should not expect the IP address space of
  // the target server to stay the same. The new target server's IP address
  // space will be recomputed and Private Network Access checks will apply anew.
  //
  // This only affects redirects where a new request is initiated at this layer
  // instead of being handled in `network::URLLoader`.
  //
  // See also: https://crbug.com/1293891
  request_.target_ip_address_space = mojom::IPAddressSpace::kUnknown;

  // Similarly, when we follow a redirect, we may make a different decision as
  // to whether and why we should send a preflight request. Maybe the request
  // is now same-origin when it was cross-origin, or vice-versa. Maybe the
  // request now does not target the private network. In any case, we will set
  // this bit back to true if we need to.
  sending_pna_only_warning_preflight_ = false;

  const bool original_fetch_cors_flag = fetch_cors_flag_;
  SetCorsFlagIfNeeded();

  // We cannot use FollowRedirect for a request with preflight (i.e., when
  // `fetch_cors_flag_` is true and `NeedsPreflight(request_)` is not nullopt).
  //
  // When `original_fetch_cors_flag` is false, `fetch_cors_flag_` is true and
  // `NeedsPreflight(request)` is nullopt, the net/ implementation won't attach
  // an "origin" header on redirect, as the original request didn't have one.
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
    sync_client_receiver_factory_.InvalidateWeakPtrs();
    StartRequest();
    return;
  }

  response_tainting_ = CalculateResponseTainting(
      request_.url, request_.mode, request_.request_initiator,
      request_.isolated_world_origin, fetch_cors_flag_, tainted_,
      *origin_access_list_);
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

void CorsURLLoader::OnReceiveEarlyHints(mojom::EarlyHintsPtr early_hints) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);

  // Only forward Early Hints for navigation.
  if (request_.mode == mojom::RequestMode::kNavigate)
    forwarding_client_->OnReceiveEarlyHints(std::move(early_hints));
}

void CorsURLLoader::OnReceiveResponse(
    mojom::URLResponseHeadPtr response_head,
    mojo::ScopedDataPipeConsumerHandle body,
    std::optional<mojo_base::BigBuffer> cached_metadata) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);

  // See 10.7.4 of https://fetch.spec.whatwg.org/#http-network-or-cache-fetch
  const bool is_304_for_revalidation =
      request_.is_revalidating && response_head->headers &&
      response_head->headers->response_code() == 304;
  if (fetch_cors_flag_ && !is_304_for_revalidation) {
    const auto result = CheckAccessAndReportMetrics(
        request_.url,
        GetHeaderString(*response_head,
                        header_names::kAccessControlAllowOrigin),
        GetHeaderString(*response_head,
                        header_names::kAccessControlAllowCredentials),
        request_.credentials_mode,
        tainted_ ? url::Origin() : *request_.request_initiator);
    if (!result.has_value()) {
      HandleComplete(URLLoaderCompletionStatus(result.error()));
      return;
    }
  }

  if (request_.destination ==
      mojom::RequestDestination::kSharedStorageWorklet) {
    CHECK(request_.request_initiator);

    if (!request_.request_initiator->IsSameOriginWith(request_.url) &&
        !CheckSharedStorageCrossOriginWorkletAllowedResponseHeaderIfNeeded(
            *response_head)) {
      HandleComplete(URLLoaderCompletionStatus(net::ERR_FAILED));
      return;
    }
  }

  std::optional<std::string> use_as_dictionary_header = GetHeaderString(
      *response_head, shared_dictionary::kUseAsDictionaryHeaderName);
  if (use_as_dictionary_header) {
    base::expected<scoped_refptr<SharedDictionaryWriter>,
                   mojom::SharedDictionaryError>
        writer_or_error = SharedDictionaryStorage::MaybeCreateWriter(
            *use_as_dictionary_header,
            request_.shared_dictionary_writer_enabled,
            shared_dictionary_storage_.get(), request_.mode, response_tainting_,
            request_.url, response_head->request_time,
            response_head->response_time, *response_head->headers,
            response_head->was_fetched_via_cache,
            base::BindOnce(
                &SharedDictionaryAccessChecker::CheckAllowedToWriteAndReport,
                std::make_unique<SharedDictionaryAccessChecker>(
                    *context_, shared_dictionary_observer_),
                request_.url, request_.site_for_cookies, isolation_info_));
    if (writer_or_error.has_value()) {
      CHECK(writer_or_error.value());
      shared_dictionary_data_pipe_writer_ =
          SharedDictionaryDataPipeWriter::Create(
              body, std::move(writer_or_error.value()),
              base::BindOnce(&CorsURLLoader::OnSharedDictionaryWritten,
                             base::Unretained(this)));
      if (!shared_dictionary_data_pipe_writer_) {
        MaybeReportSharedDictionaryErrorToDevTools(
            mojom::SharedDictionaryError::kWriteErrorInsufficientResources);
        HandleComplete(
            URLLoaderCompletionStatus(net::ERR_INSUFFICIENT_RESOURCES));
        return;
      }
    } else {
      MaybeReportSharedDictionaryErrorToDevTools(writer_or_error.error());
    }
  }

  if (!response_head->did_use_shared_dictionary && shared_dictionary_) {
    if (!(request_.load_flags & net::LOAD_CAN_USE_SHARED_DICTIONARY)) {
      // There are matching dictionary, but we can't use it because
      // the request is no-cors cross origin request.
      MaybeReportSharedDictionaryErrorToDevTools(
          mojom::SharedDictionaryError::kUseErrorCrossOriginNoCorsRequest);
    } else {
      MaybeReportSharedDictionaryErrorToDevTools(
          mojom::SharedDictionaryError::kUseErrorMatchingDictionaryNotUsed);
    }
  }

  // Opaque response tainting requests must not use shared dictionary.
  CHECK(!(response_head->did_use_shared_dictionary &&
          (response_tainting_ == mojom::FetchResponseType::kOpaque)));

  has_forwarded_response_ = true;
  timing_allow_failed_flag_ = !PassesTimingAllowOriginCheck(*response_head);

  response_head->response_type = response_tainting_;
  response_head->timing_allow_passed = !timing_allow_failed_flag_;
  response_head->has_authorization_covered_by_wildcard_on_preflight =
      has_authorization_covered_by_wildcard_;

  response_head->private_network_access_preflight_result =
      TakePrivateNetworkAccessPreflightResult();

  forwarding_client_->OnReceiveResponse(
      std::move(response_head), std::move(body), std::move(cached_metadata));
}

void CorsURLLoader::CheckTainted(const net::RedirectInfo& redirect_info) {
  // If `actualResponse`’s location URL’s origin is not same origin with
  // `request`’s current url’s origin and `request`’s origin is not same origin
  // with `request`’s current url’s origin, then set `request`’s tainted origin
  // flag.
  if (request_.request_initiator &&
      (!url::IsSameOriginWith(redirect_info.new_url, request_.url) &&
       !request_.request_initiator->IsSameOriginWith(request_.url))) {
    tainted_ = true;
  }
}

void CorsURLLoader::OnReceiveRedirect(const net::RedirectInfo& redirect_info,
                                      mojom::URLResponseHeadPtr response_head) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);

  response_head->private_network_access_preflight_result =
      TakePrivateNetworkAccessPreflightResult();

  // If `CORS flag` is set and a CORS check for `request` and `response` returns
  // failure, then return a network error.
  if (fetch_cors_flag_ && IsCorsEnabledRequestMode(request_.mode)) {
    const auto result = CheckAccessAndReportMetrics(
        request_.url,
        GetHeaderString(*response_head,
                        header_names::kAccessControlAllowOrigin),
        GetHeaderString(*response_head,
                        header_names::kAccessControlAllowCredentials),
        request_.credentials_mode,
        tainted_ ? url::Origin() : *request_.request_initiator);
    if (!result.has_value()) {
      HandleComplete(URLLoaderCompletionStatus(result.error()));
      return;
    }
  }

  timing_allow_failed_flag_ = !PassesTimingAllowOriginCheck(*response_head);
  last_response_url_ = redirect_info.new_url;

  if (request_.redirect_mode == mojom::RedirectMode::kManual) {
    CheckTainted(redirect_info);
    deferred_redirect_url_ = std::make_unique<GURL>(redirect_info.new_url);
    forwarding_client_->OnReceiveRedirect(redirect_info,
                                          std::move(response_head));
    return;
  }

  // Because we initiate a new request on redirect in some cases, we cannot
  // rely on the redirect logic in the network stack. Hence we need to
  // implement some logic in
  // https://fetch.spec.whatwg.org/#http-redirect-fetch here.

  // If `request`’s redirect count is twenty, return a network error.
  // Increase `request`’s redirect count by one.
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

  // If `actualResponse`’s status is not 303, `request`’s body is non-null, and
  // `request`’s body’s source is null, then return a network error.
  if (redirect_info.status_code != net::HTTP_SEE_OTHER &&
      network::URLLoader::HasFetchStreamingUploadBody(&request_)) {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_INVALID_ARGUMENT));
    return;
  }

  CheckTainted(redirect_info);

  // TODO(crbug.com/40686262): Implement the following:
  // If either `actualResponse`’s status is 301 or 302 and `request`’s method is
  // `POST`, or `actualResponse`’s status is 303, set `request`’s method to
  // `GET` and request’s body to null, and remove request-body-header name from
  // request's headers. Some of them are implemented in //net, but when we
  // create another request on exceptional redirect cases, such newly created
  // request doesn't reflect the spec comformant request modifications. See the
  // linked crbug for details. See also 4.4. HTTP-redirect fetch
  // (https://fetch.spec.whatwg.org/#http-redirect-fetch), step 11.

  // TODO(crbug.com/40686262): Implement the following:
  // Invoke `set request’s referrer policy on redirect` on `request` and
  // `actualResponse`. See 4.4. HTTP-redirect fetch
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

void CorsURLLoader::OnTransferSizeUpdated(int32_t transfer_size_diff) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);
  DCHECK(!deferred_redirect_url_);
  network::RecordOnTransferSizeUpdatedUMA(
      network::OnTransferSizeUpdatedFrom::kCorsURLLoader);
  forwarding_client_->OnTransferSizeUpdated(transfer_size_diff);
}

void CorsURLLoader::OnComplete(const URLLoaderCompletionStatus& status) {
  DCHECK(network_loader_);
  DCHECK(forwarding_client_);

  if (status.error_code == net::ERR_DICTIONARY_LOAD_FAILED) {
    MaybeReportSharedDictionaryErrorToDevTools(
        mojom::SharedDictionaryError::kUseErrorDictionaryLoadFailure);
  } else if (status.error_code ==
             net::ERR_UNEXPECTED_CONTENT_DICTIONARY_HEADER) {
    MaybeReportSharedDictionaryErrorToDevTools(
        mojom::SharedDictionaryError::
            kUseErrorUnexpectedContentDictionaryHeader);
  }

  // `network_loader_` will call OnComplete at anytime when a problem happens
  // inside the URLLoader, e.g. on URLLoader::OnMojoDisconnect call. We need
  // to expect it also happens even during redirect handling.
  DCHECK(!deferred_redirect_url_ || status.error_code != net::OK);

  if (shared_dictionary_data_pipe_writer_) {
    deferred_completion_status_ = status;
    shared_dictionary_data_pipe_writer_->OnComplete(status.error_code ==
                                                    net::OK);
  } else {
    HandleComplete(status);
  }
}

void CorsURLLoader::CancelRequestIfNonceMatchesAndUrlNotExempted(
    const base::UnguessableToken& nonce,
    const std::set<GURL>& exemptions) {
  if (isolation_info_.nonce() == nonce) {
    if (!exemptions.contains(request_.url.GetWithoutFilename())) {
      HandleComplete(
          URLLoaderCompletionStatus(net::ERR_NETWORK_ACCESS_REVOKED));
    }
  }
}

void CorsURLLoader::StartRequest() {
  TRACE_EVENT("loading", "CorsURLLoader::StartRequest",
              perfetto::Flow::ProcessScoped(net_log_.source().id));
  // All results should be reported to `forwarding_client_` as part of a
  // `URLResponseHead`, then `pna_preflight_result_` reset to `kNone`.
  CHECK_EQ(pna_preflight_result_,
           mojom::PrivateNetworkAccessPreflightResult::kNone);

  if (fetch_cors_flag_ && !skip_cors_enabled_scheme_check_ &&
      !base::Contains(url::GetCorsEnabledSchemes(), request_.url.scheme())) {
    HandleComplete(URLLoaderCompletionStatus(
        CorsErrorStatus(mojom::CorsError::kCorsDisabledScheme)));
    return;
  }

  auto should_include_origin_header = [&]() -> bool {
    if (!request_.request_initiator) {
      return false;
    }

    if (request_.credentials_mode == mojom::CredentialsMode::kInclude &&
        context_->cookie_manager()
            ->cookie_settings()
            .IsStorageAccessHeadersEnabled(
                request_.url, isolation_info_.top_frame_origin()) &&
        context_->cookie_manager()->cookie_settings().GetStorageAccessStatus(
            request_.url, request_.site_for_cookies,
            isolation_info_.top_frame_origin(),
            network::URLLoader::CalculateCookieSettingOverrides(
                factory_cookie_setting_overrides_, request_)) ==
            net::cookie_util::StorageAccessStatus::kInactive) {
      // Lower layers will add the Sec-Fetch-Storage-Access header, and the
      // server may respond with a "retry" header. The server needs to know the
      // origin in that event.
      return true;
    }

    // If the `CORS flag` is set, `httpRequest`’s method is neither `GET` nor
    // `HEAD`, or `httpRequest`’s mode is "websocket", then append
    // `Origin`/the result of serializing a request origin with `httpRequest`,
    // to `httpRequest`’s header list.
    //
    // We exclude navigation requests to keep the existing behavior.
    // TODO(yhirano): Reconsider this.
    if (request_.mode == network::mojom::RequestMode::kNavigate) {
      return false;
    }
    if (fetch_cors_flag_) {
      return true;
    }
    return request_.method != net::HttpRequestHeaders::kGetMethod &&
           request_.method != net::HttpRequestHeaders::kHeadMethod;
  };

  if (should_include_origin_header()) {
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
      *origin_access_list_);

  // Note that even when `needs_preflight` holds we might not make a preflight
  // request. This happens when `fetch_cors_flag_` is false, e.g. when the
  // origin of the url is equal to the origin of the request, and the preflight
  // reason is not `kPrivateNetworkAccess`. In the case of a private network
  // access we always send a preflight, even for CORS-disabled requests.
  //
  // See the first step of the HTTP-no-service-worker fetch algorithm defined in
  // the Private Network Access spec:
  // https://wicg.github.io/private-network-access/#http-no-service-worker-fetch
  std::optional<PreflightRequiredReason> needs_preflight =
      NeedsPreflight(request_);
  bool preflight_required =
      needs_preflight.has_value() &&
      (fetch_cors_flag_ ||
       *needs_preflight == PreflightRequiredReason::kPrivateNetworkAccess);
  net_log_.AddEvent(net::NetLogEventType::CHECK_CORS_PREFLIGHT_REQUIRED, [&] {
    return NetLogPreflightRequiredParams(needs_preflight);
  });

  has_authorization_covered_by_wildcard_ = false;
  if (!preflight_required) {
    StartNetworkRequest();
    return;
  }

  preflight_mode_.Clear();
  if (fetch_cors_flag_ && NeedsCorsPreflight(request_).has_value()) {
    preflight_mode_.Put(PreflightController::PreflightType::kCors);
  }
  if (NeedsPrivateNetworkAccessPreflight(request_).has_value()) {
    preflight_mode_.Put(
        PreflightController::PreflightType::kPrivateNetworkAccess);
  }
  CHECK(!preflight_mode_.empty());

  // Since we're doing a preflight, we won't reuse the original request. Cancel
  // it now to free up the socket.
  network_loader_.reset();

  mojo::PendingRemote<mojom::URLLoaderNetworkServiceObserver> remote_observer;

  if (needs_preflight.has_value() &&
      *needs_preflight == PreflightRequiredReason::kPrivateNetworkAccess) {
    // TODO(crbug.com/40229602): Create a base function and clean up all
    // need_pna_permission check in the code base.
    const mojom::ClientSecurityState* state = GetClientSecurityState();
    const bool needs_pna_permission =
        state && PrivateNetworkAccessChecker::NeedPermission(
                     request_.url, state->is_web_secure_context,
                     request_.required_ip_address_space);
    if (needs_pna_permission &&
        url_loader_network_service_observer_->is_bound()) {
      // Fail the request if `targetAddressSpace` on fetch option is not the
      // same as the real target address space.
      if (request_.required_ip_address_space !=
          request_.target_ip_address_space) {
        HandleComplete(URLLoaderCompletionStatus(
            CorsErrorStatus(mojom::CorsError::kInvalidPrivateNetworkAccess)));
        return;
      }
      (*url_loader_network_service_observer_)
          ->Clone(remote_observer.InitWithNewPipeAndPassReceiver());
    }
  }
  context_->cors_preflight_controller()->PerformPreflightCheck(
      base::BindOnce(&CorsURLLoader::OnPreflightRequestComplete,
                     weak_factory_.GetWeakPtr()),
      request_,
      PreflightController::WithTrustedHeaderClient(
          options_ & mojom::kURLLoadOptionUseHeaderClient),
      context_->cors_non_wildcard_request_headers_support(),
      GetPrivateNetworkAccessPreflightBehavior(
          request_.required_ip_address_space),
      tainted_, net::NetworkTrafficAnnotationTag(traffic_annotation_),
      network_loader_factory_, isolation_info_, CloneClientSecurityState(),
      weak_devtools_observer_factory_.GetWeakPtr(), net_log_,
      context_->acam_preflight_spec_conformant(), std::move(remote_observer),
      preflight_mode_);
}

void CorsURLLoader::ReportCorsErrorToDevTools(const CorsErrorStatus& status,
                                              bool is_warning) {
  DCHECK(devtools_observer_);

  devtools_observer_->OnCorsError(
      request_.devtools_request_id, request_.request_initiator,
      CloneClientSecurityState(), request_.url, status, is_warning);
}

void CorsURLLoader::ReportOrbErrorToDevTools() {
  devtools_observer_->OnOrbError(request_.devtools_request_id, request_.url);
}

void CorsURLLoader::MaybeReportSharedDictionaryErrorToDevTools(
    mojom::SharedDictionaryError error) {
  // No need to send AlreadyRegistered error to DevTools.
  if (error == mojom::SharedDictionaryError::kWriteErrorAlreadyRegistered) {
    return;
  }
  if (devtools_observer_ && request_.devtools_request_id) {
    devtools_observer_->OnSharedDictionaryError(*request_.devtools_request_id,
                                                request_.url, error);
  }
}

std::optional<URLLoaderCompletionStatus> CorsURLLoader::ConvertPreflightResult(
    int net_error,
    std::optional<CorsErrorStatus> status) {
  std::optional<PreflightRequiredReason> reason = NeedsPreflight(request_);
  CHECK(reason.has_value());  // Otherwise we should not have sent a preflight.

  // Unmitigated success: no error and no warning.
  if (net_error == net::OK && !status.has_value()) {
    // If the preflight was sent for PNA, record the success so we can report it
    // to `forwarding_client_`.
    if (*reason == PreflightRequiredReason::kPrivateNetworkAccess) {
      pna_preflight_result_ =
          mojom::PrivateNetworkAccessPreflightResult::kSuccess;
    }

    return std::nullopt;
  }

  if (net_error != net::OK) {
    net_log_.AddEvent(net::NetLogEventType::CORS_PREFLIGHT_ERROR, [&] {
      return NetLogPreflightErrorParams(net_error, status);
    });
  }

  // `kInvalidResponse` is never returned by the preflight controller, so we use
  // it to record the case where there was a net error and no CORS error.
  auto histogram_error = mojom::CorsError::kInvalidResponse;
  if (status) {
    DCHECK(status->cors_error != mojom::CorsError::kInvalidResponse);
    histogram_error = status->cors_error;

    // Report the target IP address space unconditionally as part of the error
    // if there was one. This allows higher layers to understand that a PNA
    // preflight request was attempted.
    status->target_address_space = request_.target_ip_address_space;
  }

  // Private Network Access warning: ignore net and CORS errors.
  if (net_error == net::OK || sending_pna_only_warning_preflight_) {
    CHECK(ShouldIgnorePrivateNetworkAccessErrors(
        request_.required_ip_address_space));
    CHECK_EQ(*reason, PreflightRequiredReason::kPrivateNetworkAccess);

    // Record the existence of the warning so that we can report it to
    // `forwarding_client_` in the next `URLResponseHead` we construct.
    pna_preflight_result_ =
        mojom::PrivateNetworkAccessPreflightResult::kWarning;

    // Even if we ignore the error, record the warning in metrics and DevTools.
    base::UmaHistogramEnumeration(kPreflightWarningHistogramName,
                                  histogram_error);

    if (devtools_observer_) {
      if (!status) {
        // Set the resource IP address space to the target IP address space for
        // better error messages in DevTools. If the resource address space had
        // not matched, the request would likely have failed with
        // `CorsError::kInvalidPrivateNetwork`. If the error happened before we
        // ever obtained a connection to the remote endpoint, then this value
        // is incorrect - we cannot tell what value it would have been. Given
        // that this is used for debugging only, the slight incorrectness is
        // worth the increased debuggability.
        status = CorsErrorStatus(mojom::CorsError::kInvalidResponse,
                                 request_.target_ip_address_space,
                                 request_.target_ip_address_space);
      }

      ReportCorsErrorToDevTools(*status, /*is_warning=*/true);
    }

    return std::nullopt;
  }

  // Failure.
  CHECK(net_error != net::OK);

  base::UmaHistogramEnumeration(kPreflightErrorHistogramName, histogram_error);
  auto result = status ? URLLoaderCompletionStatus(*std::move(status))
                       : URLLoaderCompletionStatus(net_error);

  if (*reason == PreflightRequiredReason::kPrivateNetworkAccess) {
    pna_preflight_result_ = mojom::PrivateNetworkAccessPreflightResult::kError;
    result.error_code = net::ERR_BLOCKED_BY_PRIVATE_NETWORK_ACCESS_CHECKS;
  }

  return result;
}

void CorsURLLoader::OnPreflightRequestComplete(
    int net_error,
    std::optional<CorsErrorStatus> status,
    bool has_authorization_covered_by_wildcard) {
  has_authorization_covered_by_wildcard_ =
      has_authorization_covered_by_wildcard;

  std::optional<URLLoaderCompletionStatus> completion_status =
      ConvertPreflightResult(net_error, std::move(status));
  if (completion_status) {
    HandleComplete(*std::move(completion_status));
    return;
  }

  StartNetworkRequest();
}

void CorsURLLoader::StartNetworkRequest() {
  TRACE_EVENT("loading", "CorsURLLoader::StartNetworkRequest",
              perfetto::Flow::ProcessScoped(net_log_.source().id));
  // Here we overwrite the credentials mode sent to URLLoader because
  // network::URLLoader doesn't understand |kSameOrigin|.
  // TODO(crbug.com/40619226): Fix this.
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

  network_loader_start_time_ = base::TimeTicks::Now();

  if (sync_network_loader_factory_) {
    sync_network_loader_factory_->CreateLoaderAndStartWithSyncClient(
        network_loader_.BindNewPipeAndPassReceiver(), request_id_, options_,
        request_, network_client_receiver_.BindNewPipeAndPassRemote(),
        sync_client_receiver_factory_.GetWeakPtr(), traffic_annotation_);
  } else {
    network_loader_factory_->CreateLoaderAndStart(
        network_loader_.BindNewPipeAndPassReceiver(), request_id_, options_,
        request_, network_client_receiver_.BindNewPipeAndPassRemote(),
        traffic_annotation_);
  }
  network_client_receiver_.set_disconnect_handler(base::BindOnce(
      &CorsURLLoader::OnNetworkClientMojoDisconnect, base::Unretained(this)));

  request_.credentials_mode = original_credentials_mode;
}

void CorsURLLoader::HandleComplete(URLLoaderCompletionStatus status) {
  if (request_.trust_token_params) {
    HistogramTrustTokenOperationNetError(request_.trust_token_params->operation,
                                         status.trust_token_operation_status,
                                         status.error_code);
  }

  if (status.error_code == net::OK) {
    DCHECK_GE(status.completion_time, network_loader_start_time_);
    base::TimeDelta elapsed =
        status.completion_time - network_loader_start_time_;
    if (status.exists_in_cache) {
      RecordNetworkLoaderCompletionTime("DiskCache", elapsed);
    } else {
      RecordNetworkLoaderCompletionTime("Network", elapsed);
    }
  }

  if (devtools_observer_ && status.cors_error_status) {
    ReportCorsErrorToDevTools(*status.cors_error_status);
  }
  // ORB "v0.1" (and earlier) signal ORB-related errors with a flag.
  // ORB "v0.2" (and later) use a network error code. We should always report
  // the error-code style error to DevTools, since it has a less spammy
  // way of displaying them compared to just dumping them on the console.
  if (devtools_observer_ && (status.should_report_orb_blocking ||
                             status.error_code == net::ERR_BLOCKED_BY_ORB)) {
    ReportOrbErrorToDevTools();
  }

  // If we detect a private network access when we were not expecting one, we
  // restart the request and force a preflight request. This preflight and the
  // following request expect the resource to be in the same IP address space
  // as was originally observed. Spec:
  // https://wicg.github.io/private-network-access/#http-no-service-worker-fetch
  if (status.cors_error_status &&
      status.cors_error_status->cors_error ==
          mojom::CorsError::kUnexpectedPrivateNetworkAccess) {
    DCHECK(status.cors_error_status->resource_address_space !=
           mojom::IPAddressSpace::kUnknown);

    // We should sent at most one PNA preflight per request (and per redirect).
    CHECK_EQ(pna_preflight_result_,
             mojom::PrivateNetworkAccessPreflightResult::kNone);

    // We should never send a preflight request for PNA after having already
    // forwarded response headers to our client. See https://crbug.com/1279376.
    if (!has_forwarded_response_) {
      // If we only send a preflight because of Private Network Access, and we
      // are configured to ignore errors caused by Private Network Access, then
      // we should ignore any preflight error, as if we had never sent the
      // preflight. Otherwise, if we had sent a preflight before we noticed the
      // private network access, then we rely on `PreflightController` to ignore
      // PNA-specific preflight errors during this second preflight request.
      sending_pna_only_warning_preflight_ =
          ShouldIgnorePrivateNetworkAccessErrors(
              request_.required_ip_address_space) &&
          !(NeedsPreflight(request_).has_value() && fetch_cors_flag_);

      network_client_receiver_.reset();
      request_.target_ip_address_space =
          status.cors_error_status->resource_address_space;
      StartRequest();
      return;
    }

    // DCHECK that we never run into this scenario, but fail the request for
    // safety if this ever happens in production.
    DUMP_WILL_BE_NOTREACHED();
  }

  status.private_network_access_preflight_result =
      TakePrivateNetworkAccessPreflightResult();

  net_log_.EndEvent(net::NetLogEventType::CORS_REQUEST);
  forwarding_client_->OnComplete(std::move(status));
  std::move(delete_callback_).Run(this);
  // |this| is deleted here.
}

void CorsURLLoader::OnMojoDisconnect() {
  HandleComplete(URLLoaderCompletionStatus(net::ERR_ABORTED));
}

void CorsURLLoader::OnNetworkClientMojoDisconnect() {
  if (shared_dictionary_data_pipe_writer_) {
    // If we already received URLLoaderCompletionStatus, ignores this disconnect
    // error.
    if (!deferred_completion_status_) {
      deferred_completion_status_ = URLLoaderCompletionStatus(net::ERR_ABORTED);
      shared_dictionary_data_pipe_writer_->OnComplete(/*success=*/false);
    }
  } else {
    HandleComplete(URLLoaderCompletionStatus(net::ERR_ABORTED));
  }
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

  if (HasSpecialAccessToDestination())
    return;

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

// static
mojom::FetchResponseType CorsURLLoader::CalculateResponseTaintingForTesting(
    const GURL& url,
    mojom::RequestMode request_mode,
    const std::optional<url::Origin>& origin,
    const std::optional<url::Origin>& isolated_world_origin,
    bool cors_flag,
    bool tainted_origin,
    const OriginAccessList& origin_access_list) {
  return CalculateResponseTainting(url, request_mode, origin,
                                   isolated_world_origin, cors_flag,
                                   tainted_origin, origin_access_list);
}

// static
std::optional<CorsErrorStatus> CorsURLLoader::CheckRedirectLocationForTesting(
    const GURL& url,
    mojom::RequestMode request_mode,
    const std::optional<url::Origin>& origin,
    bool cors_flag,
    bool tainted) {
  return CheckRedirectLocation(url, request_mode, origin, cors_flag, tainted);
}

// https://fetch.spec.whatwg.org/#tao-check
bool CorsURLLoader::PassesTimingAllowOriginCheck(
    const mojom::URLResponseHead& response) const {
  // If request’s timing allow failed flag is set, then return failure.
  if (timing_allow_failed_flag_)
    return false;

  // Let values be the result of getting, decoding, and splitting
  // `Timing-Allow-Origin` from response’s header list.
  std::optional<std::string> tao_header_value =
      GetHeaderString(response, kTimingAllowOrigin);

  if (tao_header_value && request_.request_initiator) {
    mojom::TimingAllowOriginPtr tao = ParseTimingAllowOrigin(*tao_header_value);
    url::Origin origin = tainted_ ? url::Origin() : *request_.request_initiator;

    if (TimingAllowOriginCheck(tao, origin))
      return true;
  }

  // If request’s mode is "navigate" and request’s current URL’s origin is not
  // same origin with request’s origin, then return failure.
  if (request_.mode == mojom::RequestMode::kNavigate &&
      request_.request_initiator &&
      (tainted_ ||
       !request_.request_initiator->IsSameOriginWith(last_response_url_))) {
    return false;
  }

  // If request’s response tainting is "basic", then return success.
  if (response_tainting_ == mojom::FetchResponseType::kBasic)
    return true;

  return false;
}

// Computes the client security state to use, given the factory and
// request-specific values.
//
// WARNING: This should be kept in sync with similar logic in
// `network::URLLoader::GetClientSecurityState()`.
const mojom::ClientSecurityState* CorsURLLoader::GetClientSecurityState()
    const {
  if (factory_client_security_state_) {
    return factory_client_security_state_;
  }

  if (request_.trusted_params) {
    // NOTE: This could return nullptr.
    return request_.trusted_params->client_security_state.get();
  }

  return nullptr;
}

mojom::ClientSecurityStatePtr CorsURLLoader::CloneClientSecurityState() const {
  const mojom::ClientSecurityState* state = GetClientSecurityState();
  if (!state) {
    return nullptr;
  }

  return state->Clone();
}

bool CorsURLLoader::ShouldIgnorePrivateNetworkAccessErrors(
    mojom::IPAddressSpace target_address_space) const {
  const mojom::ClientSecurityState* state = GetClientSecurityState();
  if (!state) {
    return false;
  }
  // When the PNA permission prompt shown, we should always respect the
  // preflight results, otherwise it would be a bypass of mixed content checker.
  if (PrivateNetworkAccessChecker::NeedPermission(
          request_.url, state->is_web_secure_context, target_address_space)) {
    return false;
  }
  return state->private_network_request_policy ==
         mojom::PrivateNetworkRequestPolicy::kPreflightWarn;
}

PrivateNetworkAccessPreflightBehavior
CorsURLLoader::GetPrivateNetworkAccessPreflightBehavior(
    mojom::IPAddressSpace target_address_space) const {
  if (!ShouldIgnorePrivateNetworkAccessErrors(target_address_space)) {
    return PrivateNetworkAccessPreflightBehavior::kEnforce;
  }
  if (sending_pna_only_warning_preflight_) {
    return PrivateNetworkAccessPreflightBehavior::kWarnWithTimeout;
  }
  return PrivateNetworkAccessPreflightBehavior::kWarn;
}

void CorsURLLoader::OnSharedDictionaryWritten(bool success) {
  shared_dictionary_data_pipe_writer_.reset();
  if (!success) {
    MaybeReportSharedDictionaryErrorToDevTools(
        mojom::SharedDictionaryError::kWriteErrorRequestAborted);
  }
  if (deferred_completion_status_) {
    HandleComplete(*deferred_completion_status_);
    return;
  }
}

mojom::PrivateNetworkAccessPreflightResult
CorsURLLoader::TakePrivateNetworkAccessPreflightResult() {
  mojom::PrivateNetworkAccessPreflightResult result = pna_preflight_result_;
  pna_preflight_result_ = mojom::PrivateNetworkAccessPreflightResult::kNone;
  return result;
}

// static
std::optional<std::string> CorsURLLoader::GetHeaderString(
    const mojom::URLResponseHead& response,
    const std::string& header_name) {
  if (!response.headers)
    return std::nullopt;
  std::string header_value;
  if (!response.headers->GetNormalizedHeader(header_name, &header_value))
    return std::nullopt;
  return header_value;
}

bool CorsURLLoader::
    CheckSharedStorageCrossOriginWorkletAllowedResponseHeaderIfNeeded(
        const mojom::URLResponseHead& response) {
  // We currently only set the "Sec-Shared-Storage-Data-Origin" request header
  // for requests of cross-origin shared storage worklet module script where the
  // script origin is used as the data origin. Moreover, the request header is a
  // forbidden request header (non-modifiable by regular JavaScript), and it is
  // set in the browser process, using the serialized script origin (which is
  // not allowed to be opaque) as the value.
  //
  // Extensions could have modified or removed the
  // "Sec-Shared-Storage-Data-Origin" request header before the request was sent
  // to the server, but the `CorsURLLoader` still sees the original header, if
  // any, set by `SharedStorageURLLoaderFactoryProxy`.
  std::optional<std::string> request_header =
      request_.headers.GetHeader("Sec-Shared-Storage-Data-Origin");
  if (!request_header) {
    // The data partition origin used is the invoking context's origin, so we
    // don't require the "Shared-Storage-Cross-Origin-Worklet-Allowed" response
    // header.
    return true;
  }

  GURL data_origin_url(*request_header);
  CHECK(data_origin_url.is_valid());
  CHECK(url::Origin::Create(data_origin_url).IsSameOriginWith(request_.url));

  std::optional<std::string> response_header =
      GetHeaderString(response, "Shared-Storage-Cross-Origin-Worklet-Allowed");
  if (!response_header) {
    return false;
  }

  std::optional<net::structured_headers::Item> item =
      net::structured_headers::ParseBareItem(*response_header);

  return item && item->is_boolean() && item->GetBoolean();
}

}  // namespace network::cors

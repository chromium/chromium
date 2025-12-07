// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/url_loader_util.h"

#include <algorithm>
#include <optional>

#include "base/containers/enum_set.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_util.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_file_element_reader.h"
#include "net/cookies/cookie_util.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_response_info.h"
#include "net/storage_access_api/status.h"
#include "net/url_request/url_request.h"
#include "services/network/ad_heuristic_cookie_overrides.h"
#include "services/network/attribution/attribution_request_helper.h"
#include "services/network/chunked_data_pipe_upload_data_stream.h"
#include "services/network/cookie_manager.h"
#include "services/network/cookie_settings.h"
#include "services/network/data_pipe_element_reader.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/cors/cors.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/header_util.h"
#include "services/network/public/cpp/parsed_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/sri_message_signatures.h"
#include "services/network/public/cpp/unencoded_digests.h"
#include "services/network/public/mojom/client_security_state.mojom.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "services/network/public/mojom/network_context.mojom-shared.h"
#include "services/network/public/mojom/url_request.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/sec_header_helpers.h"
#include "services/network/shared_resource_checker.h"

namespace network::url_loader_util {

namespace {

// TODO(https://crbug.com/375352611): add the check for enabling third-party
// cookies.
constexpr uint64_t kAllowedDevToolsCookieSettingOverrides =
    1u << static_cast<int>(
        net::CookieSettingOverride::kForceDisableThirdPartyCookies) |
    1u << static_cast<int>(
        net::CookieSettingOverride::kForceEnableThirdPartyCookieMitigations) |
    1u << static_cast<int>(net::CookieSettingOverride::kSkipTPCDMetadataGrant) |
    1u << static_cast<int>(
        net::CookieSettingOverride::kSkipTPCDHeuristicsGrant);

bool IsMultiplexedConnection(const net::HttpResponseInfo& response_info) {
  switch (net::HttpConnectionInfoToCoarse(response_info.connection_info)) {
    case net::HttpConnectionInfoCoarse::kHTTP1:
      return false;
    case net::HttpConnectionInfoCoarse::kHTTP2:
    case net::HttpConnectionInfoCoarse::kQUIC:
      return true;
    case net::HttpConnectionInfoCoarse::kOTHER:
      return false;
  }
}

const char* GetDestinationTypePartString(
    network::mojom::RequestDestination destination) {
  if (destination == network::mojom::RequestDestination::kDocument) {
    return "MainFrame";
  } else if (destination == network::mojom::RequestDestination::kFrame ||
             destination == network::mojom::RequestDestination::kIframe) {
    return "SubFrame";
  }
  return "Subresource";
}

const char* GetCertStatePartString(const net::SSLInfo& ssl_info) {
  if (!ssl_info.cert.get()) {
    return "NoCert";
  }
  return ssl_info.is_issued_by_known_root ? "KnownRootCert" : "UnknownRootCert";
}

// Returns true if the |credentials_mode| of the request allows sending
// credentials.
bool ShouldAllowCredentials(mojom::CredentialsMode credentials_mode) {
  switch (credentials_mode) {
    case mojom::CredentialsMode::kInclude:
    // TODO(crbug.com/40619226): Make this work with
    // CredentialsMode::kSameOrigin.
    case mojom::CredentialsMode::kSameOrigin:
      return true;

    case mojom::CredentialsMode::kOmit:
    case mojom::CredentialsMode::kOmitBug_775438_Workaround:
      return false;
  }
}

// Returns true when the |credentials_mode| of the request allows sending client
// certificates.
bool ShouldSendClientCertificates(mojom::CredentialsMode credentials_mode) {
  switch (credentials_mode) {
    case mojom::CredentialsMode::kInclude:
    case mojom::CredentialsMode::kSameOrigin:
      return true;

    // TODO(crbug.com/40089326): Due to a bug, the default behavior does
    // not properly correspond to Fetch's "credentials mode", in that client
    // certificates will be sent if available, or the handshake will be aborted
    // to allow selecting a client cert.
    // With the feature kOmitCorsClientCert enabled, the correct
    // behavior is done; omit all client certs and continue the handshake
    // without sending one if requested.
    case mojom::CredentialsMode::kOmit:
      return !base::FeatureList::IsEnabled(features::kOmitCorsClientCert);

    case mojom::CredentialsMode::kOmitBug_775438_Workaround:
      return false;
  }
}

// Returns whether sending/storing credentials is allowed by COEP and
// Document-Isolation-Policy.
// |url| is the latest request URL, either the original URL or
// `redirect_info.new_url`.
// When Cross-Origin-Embedder-Policy: credentialless or
// Document-Isolation-Policy: isolate-and-credentialless are set, do not send
// or store credentials for no-cors cross-origin request.
// [spec]:
// https://fetch.spec.whatwg.org/#cross-origin-embedder-policy-allows-credentials
bool WebPoliciesAllowCredentials(
    const GURL& url,
    const network::mojom::ClientSecurityStatePtr& client_security_state,
    mojom::RequestMode request_mode,
    const std::optional<url::Origin>& initiator) {
  // [spec]: To check if Cross-Origin-Embedder-Policy allows credentials, given
  //         a request request, run these steps:

  // [spec]  1. If request’s mode is not "no-cors", then return true.
  switch (request_mode) {
    case mojom::RequestMode::kCors:
    case mojom::RequestMode::kCorsWithForcedPreflight:
    case mojom::RequestMode::kNavigate:
    case mojom::RequestMode::kSameOrigin:
      return true;

    case mojom::RequestMode::kNoCors:
      break;
  }

  // [spec]: 2. If request’s client is null, then return true.
  if (!client_security_state) {
    return true;
  }

  // [spec]: 3. If request’s client’s policy container’s embedder policy’s value
  //            is not "credentialless", then return true.
  // Document-Isolation-Policy: also check that Document-Isolation-Policy allows
  // credentials.
  if (client_security_state->cross_origin_embedder_policy.value !=
          mojom::CrossOriginEmbedderPolicyValue::kCredentialless &&
      client_security_state->document_isolation_policy.value !=
          mojom::DocumentIsolationPolicyValue::kIsolateAndCredentialless) {
    return true;
  }

  // [spec]: 4. If request’s origin is same origin with request’s current URL’s
  //            origin and request does not have a redirect-tainted origin, then
  //            return true.
  url::Origin request_initiator = initiator.value_or(url::Origin());
  if (request_initiator.IsSameOriginWith(url)) {
    return true;
  }

  // [spec]: 5. Return false.
  return false;
}

bool ShouldForceIgnoreSiteForCookies(
    const ResourceRequest& request,
    const cors::OriginAccessList& origin_access_list) {
  // Ignore site for cookies in requests from an initiator covered by the
  // same-origin-policy exclusions in `origin_access_list_` (typically requests
  // initiated by Chrome Extensions).
  if (request.request_initiator.has_value() &&
      cors::OriginAccessList::AccessState::kAllowed ==
          origin_access_list.CheckAccessState(request.request_initiator.value(),
                                              request.url)) {
    return true;
  }

  // Convert `site_for_cookies` into an origin (an opaque origin if
  // `net::SiteForCookies::IsNull()` returns true).
  //
  // Note that `site_for_cookies` is a _site_ rather than an _origin_, but for
  // Chrome Extensions the _site_ and _origin_ of a host are the same extension
  // id.  Thanks to this, for Chrome Extensions, we can pass a _site_ into
  // OriginAccessChecks (which normally expect an _origin_).
  url::Origin site_origin =
      url::Origin::Create(request.site_for_cookies.RepresentativeUrl());

  // If `site_for_cookies` represents an origin that is granted access to the
  // initiator and the target by `origin_access_list_` (typically such
  // `site_for_cookies` represents a Chrome Extension), then we also should
  // force ignoring of site for cookies if the initiator and the target are
  // same-site.
  //
  // Ideally we would walk up the frame tree and check that each ancestor is
  // first-party to the main frame (treating the `origin_access_list_`
  // exceptions as "first-party").  But walking up the tree is not possible in
  // //services/network and so we make do with just checking the direct
  // initiator of the request.
  //
  // We also check same-siteness between the initiator and the requested URL,
  // because setting `force_ignore_site_for_cookies` to true causes Strict
  // cookies to be attached, and having the initiator be same-site to the
  // request URL is a requirement for Strict cookies (see
  // net::cookie_util::ComputeSameSiteContext).
  if (!site_origin.opaque() && request.request_initiator.has_value()) {
    bool site_can_access_target =
        cors::OriginAccessList::AccessState::kAllowed ==
        origin_access_list.CheckAccessState(site_origin, request.url);
    bool site_can_access_initiator =
        cors::OriginAccessList::AccessState::kAllowed ==
        origin_access_list.CheckAccessState(
            site_origin, request.request_initiator->GetURL());
    net::SiteForCookies site_of_initiator =
        net::SiteForCookies::FromOrigin(request.request_initiator.value());
    bool are_initiator_and_target_same_site =
        site_of_initiator.IsFirstParty(request.url);
    if (site_can_access_initiator && site_can_access_target &&
        are_initiator_and_target_same_site) {
      return true;
    }
  }

  return false;
}

// A subclass of net::UploadBytesElementReader which owns
// ResourceRequestBody.
class BytesElementReader : public net::UploadBytesElementReader {
 public:
  BytesElementReader(ResourceRequestBody* resource_request_body,
                     const DataElementBytes& element)
      : net::UploadBytesElementReader(element.bytes()),
        resource_request_body_(resource_request_body) {}

  BytesElementReader(const BytesElementReader&) = delete;
  BytesElementReader& operator=(const BytesElementReader&) = delete;

  ~BytesElementReader() override = default;

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;
};

// A subclass of net::UploadFileElementReader which owns
// ResourceRequestBody.
// This class is necessary to ensure the BlobData and any attached shareable
// files survive until upload completion.
class FileElementReader : public net::UploadFileElementReader {
 public:
  FileElementReader(ResourceRequestBody* resource_request_body,
                    base::TaskRunner* task_runner,
                    const DataElementFile& element,
                    base::File&& file)
      : net::UploadFileElementReader(task_runner,
                                     std::move(file),
                                     element.path(),
                                     element.offset(),
                                     element.length(),
                                     element.expected_modification_time()),
        resource_request_body_(resource_request_body) {}

  FileElementReader(const FileElementReader&) = delete;
  FileElementReader& operator=(const FileElementReader&) = delete;

  ~FileElementReader() override = default;

 private:
  scoped_refptr<ResourceRequestBody> resource_request_body_;
};

}  // namespace

// Returns if `request` is fetch upload request with a streaming body.
bool HasFetchStreamingUploadBody(const ResourceRequest& request) {
  const ResourceRequestBody* request_body = request.request_body.get();
  if (!request_body) {
    return false;
  }
  const std::vector<DataElement>* elements = request_body->elements();
  if (elements->size() != 1u) {
    return false;
  }
  const auto& element = elements->front();
  return element.type() == mojom::DataElementDataView::Tag::kChunkedDataPipe &&
         element.As<network::DataElementChunkedDataPipe>().read_only_once();
}

std::unique_ptr<net::UploadDataStream> CreateUploadDataStream(
    ResourceRequestBody* body,
    std::vector<base::File>& opened_files,
    base::SequencedTaskRunner* file_task_runner) {
  // In the case of a chunked upload, there will just be one element.
  if (body->elements()->size() == 1) {
    if (body->elements()->begin()->type() ==
        network::mojom::DataElementDataView::Tag::kChunkedDataPipe) {
      auto& element =
          body->elements_mutable()->at(0).As<DataElementChunkedDataPipe>();
      const bool has_null_source = element.read_only_once().value();
      auto upload_data_stream =
          std::make_unique<ChunkedDataPipeUploadDataStream>(
              body, element.ReleaseChunkedDataPipeGetter(), has_null_source);
      if (element.read_only_once()) {
        upload_data_stream->EnableCache();
      }
      return upload_data_stream;
    }
  }

  auto opened_file = opened_files.begin();
  std::vector<std::unique_ptr<net::UploadElementReader>> element_readers;
  for (const auto& element : *body->elements()) {
    switch (element.type()) {
      case network::mojom::DataElementDataView::Tag::kBytes:
        element_readers.push_back(std::make_unique<BytesElementReader>(
            body, element.As<DataElementBytes>()));
        break;
      case network::mojom::DataElementDataView::Tag::kFile:
        CHECK(opened_file != opened_files.end());
        element_readers.push_back(std::make_unique<FileElementReader>(
            body, file_task_runner, element.As<network::DataElementFile>(),
            std::move(*opened_file++)));
        break;
      case network::mojom::DataElementDataView::Tag::kDataPipe: {
        element_readers.push_back(std::make_unique<DataPipeElementReader>(
            body,
            element.As<network::DataElementDataPipe>().CloneDataPipeGetter()));
        break;
      }
      case network::mojom::DataElementDataView::Tag::kChunkedDataPipe: {
        // This shouldn't happen, as the traits logic should ensure that if
        // there's a chunked pipe, there's one and only one element.
        NOTREACHED();
      }
    }
  }
  DCHECK(opened_file == opened_files.end());

  return std::make_unique<net::ElementsUploadDataStream>(
      std::move(element_readers), body->identifier());
}

net::CookieSettingOverrides CalculateCookieSettingOverrides(
    net::CookieSettingOverrides factory_overrides,
    net::CookieSettingOverrides devtools_overrides,
    const ResourceRequest& request,
    bool emit_metrics) {
  net::CookieSettingOverrides overrides(factory_overrides);
  if (request.is_outermost_main_frame &&
      network::cors::IsCorsEnabledRequestMode(request.mode)) {
    overrides.Put(
        net::CookieSettingOverride::kTopLevelStorageAccessGrantEligible);
  }

  AddAdsHeuristicCookieSettingOverrides(request.is_ad_tagged, overrides,
                                        emit_metrics);
  // Only apply the DevTools overrides if the request is from devtools enabled
  // context.
  if (request.devtools_request_id.has_value()) {
    CHECK_EQ(devtools_overrides.ToEnumBitmask() &
                 ~kAllowedDevToolsCookieSettingOverrides,
             0u);
    overrides = base::Union(overrides, devtools_overrides);
  }

  // If `factory_overrides` contains
  // `net::CookieSettingOverride::kStorageAccessGrantEligible`, then
  // well-behaved clients ensure that `request.storage_access_api_status` is
  // `net::StorageAccessApiStatus::kAccessViaAPI`. But since clients may be
  // compromised, we do not CHECK that this holds.
  //
  // Note: `kStorageAccessGrantEligible` and `request.storage_access_api_status`
  // are not trusted for security or privacy decisions.

  // Add the Storage Access override enum based on whether the request's url and
  // initiator are same-site, to prevent cross-site sibling iframes benefit from
  // each other's storage access API grants. This must be updated on redirects.
  if (net::cookie_util::ShouldAddInitialStorageAccessApiOverride(
          request.url, request.storage_access_api_status,
          request.request_initiator)) {
    overrides.Put(net::CookieSettingOverride::kStorageAccessGrantEligible);
  }

  // The `kStorageAccessGrantEligibleViaHeader` override will be applied
  // (in-place) by individual request jobs as appropriate, but should not be
  // present initially.
  CHECK(!overrides.Has(
      net::CookieSettingOverride::kStorageAccessGrantEligibleViaHeader));

  return overrides;
}

std::optional<net::IsolationInfo> GetIsolationInfo(
    const net::IsolationInfo& factory_isolation_info,
    bool automatically_assign_isolation_info,
    const ResourceRequest& request) {
  if (!factory_isolation_info.IsEmpty()) {
    return factory_isolation_info;
  }

  if (request.trusted_params &&
      !request.trusted_params->isolation_info.IsEmpty()) {
    if (request.credentials_mode != network::mojom::CredentialsMode::kOmit) {
      DCHECK(request.trusted_params->isolation_info.site_for_cookies()
                 .IsEquivalent(request.site_for_cookies));
    }
    return request.trusted_params->isolation_info;
  }

  if (automatically_assign_isolation_info) {
    url::Origin origin = url::Origin::Create(request.url);
    return net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                      origin, origin, net::SiteForCookies());
  }

  return std::nullopt;
}

// Retrieves the Cookie header from either `cors_exempt_headers` or `headers`.
std::string GetCookiesFromHeaders(
    const net::HttpRequestHeaders& headers,
    const net::HttpRequestHeaders& cors_exempt_headers) {
  std::optional<std::string> cookies =
      cors_exempt_headers.GetHeader(net::HttpRequestHeaders::kCookie);
  if (!cookies) {
    cookies = headers.GetHeader(net::HttpRequestHeaders::kCookie);
  }
  return std::move(cookies).value_or(std::string());
}

void RecordURLLoaderRequestMetrics(const net::URLRequest& url_request,
                                   size_t raw_request_line_size,
                                   size_t raw_request_headers_size) {
  // All histograms recorded here are of the form:
  // "NetworkService.Requests.{Multiplexed}.{RequestType}.{Method}.{Result}
  // .{Metric}".
  // For example:
  // "NetworkService.Requests.Simple.MainFrame.Get.Success.TotalRequestSize".
  absl::InlinedVector<std::string_view, 10> histogram_prefix_pieces = {
      "NetworkService", "Requests"};

  const net::HttpResponseInfo& response_info = url_request.response_info();
  if (IsMultiplexedConnection(response_info)) {
    histogram_prefix_pieces.push_back("Multiplexed");
  } else {
    histogram_prefix_pieces.push_back("Simple");
  }

  switch (url_request.isolation_info().request_type()) {
    case net::IsolationInfo::RequestType::kMainFrame:
      histogram_prefix_pieces.push_back("MainFrame");
      break;
    case net::IsolationInfo::RequestType::kSubFrame:
    case net::IsolationInfo::RequestType::kOther:
      // TODO(crbug.com/362787712): Add metrics for other types of requests.
      return;
  }

  if (url_request.method() == "GET") {
    histogram_prefix_pieces.push_back("Get");
  } else {
    // Other types of requests need to be handled differently e.g. the total
    // request size of a POST request needs to include the body.
    // TODO(crbug.com/362787712): Add metrics for other types of requests.
    return;
  }

  const int response_code = response_info.headers->response_code();
  if (response_code < 199) {
    // Ignore information responses because they are not complete requests.
    return;
  } else if (response_code < 299 || response_code < 399) {
    // We consider redirects a success.
    histogram_prefix_pieces.push_back("Success");
  } else if (response_code < 499) {
    histogram_prefix_pieces.push_back("ClientError");
  } else if (response_code < 599) {
    histogram_prefix_pieces.push_back("ServerError");
  } else {
    // Ignore unexpected server response codes.
    return;
  }

  auto make_histogram_name =
      [&histogram_prefix_pieces](std::string_view metric) {
        histogram_prefix_pieces.push_back(metric);
        std::string name = base::JoinString(histogram_prefix_pieces, ".");
        histogram_prefix_pieces.pop_back();
        return name;
      };

  base::UmaHistogramCounts100000(make_histogram_name("TotalUrlSize"),
                                 url_request.url().spec().size());

  // HTTP/2 and HTTP/3 requests don't separate request line from headers so no
  // need to record header metrics separately.
  if (!IsMultiplexedConnection(response_info)) {
    base::UmaHistogramCounts100000(make_histogram_name("TotalHeadersSize"),
                                   raw_request_headers_size);
  }

  // For HTTP/2 and HTTP/3 the request line is included in the headers, but
  // `raw_request_line_size_` is 0 for these requests, so we can add it
  // unconditionally for all requests.
  size_t total_request_size = raw_request_headers_size + raw_request_line_size;
  base::UmaHistogramCounts100000(make_histogram_name("TotalRequestSize"),
                                 total_request_size);
}

void MaybeRecordSharedDictionaryUsedResponseMetrics(
    int error_code,
    network::mojom::RequestDestination destination,
    const net::HttpResponseInfo& response_info,
    bool shared_dictionary_allowed_check_passed) {
  if (response_info.was_cached) {
    return;
  }
  if (response_info.did_use_shared_dictionary) {
    base::UmaHistogramSparse(
        base::StrCat({"Net.SharedDictionaryUsedResponseErrorCodes2.",
                      GetDestinationTypePartString(destination), ".",
                      GetCertStatePartString(response_info.ssl_info)}),
        -error_code);
  }

  if (shared_dictionary_allowed_check_passed &&
      destination == network::mojom::RequestDestination::kDocument) {
    base::UmaHistogramBoolean(
        base::StrCat(
            {"Net.SharedDictionaryUsedByResponseWhenAvailable2.MainFrame.",
             net::HttpConnectionInfoCoarseToString(
                 net::HttpConnectionInfoToCoarse(
                     response_info.connection_info)),
             ".", GetCertStatePartString(response_info.ssl_info)}),
        response_info.did_use_shared_dictionary);
  }
}

void ConfigureUrlRequest(const ResourceRequest& request,
                         const mojom::URLLoaderFactoryParams& factory_params,
                         const cors::OriginAccessList& origin_access_list,
                         net::URLRequest& url_request,
                         SharedResourceChecker& shared_resource_checker) {
  url_request.set_method(request.method);
  url_request.set_site_for_cookies(request.site_for_cookies);
  url_request.set_force_ignore_site_for_cookies(
      ShouldForceIgnoreSiteForCookies(request, origin_access_list));
  if (!request.navigation_redirect_chain.empty()) {
    DCHECK_EQ(request.mode, mojom::RequestMode::kNavigate);
    url_request.SetURLChain(request.navigation_redirect_chain);
  }
  url_request.SetReferrer(request.referrer.GetAsReferrer().spec());
  url_request.set_referrer_policy(request.referrer_policy);
  url_request.set_upgrade_if_insecure(request.upgrade_if_insecure);
  url_request.set_ad_tagged(request.is_ad_tagged);
  url_request.set_client_side_content_decoding_enabled(
      request.client_side_content_decoding_enabled);

  auto isolation_info = GetIsolationInfo(
      factory_params.isolation_info,
      factory_params.automatically_assign_isolation_info, request);
  if (isolation_info) {
    // set_isolation_info sets the url_request's cookie_partition_key which can
    // then be used when checking IsSharedResource().
    url_request.set_isolation_info(std::move(isolation_info).value());
    url_request.set_is_shared_resource(shared_resource_checker.IsSharedResource(
        request, url_request.isolation_info().top_frame_origin(),
        url_request.cookie_partition_key()));
  }

  // When a service worker forwards a navigation request it uses the
  // service worker's IsolationInfo.  This causes the cookie code to fail
  // to send SameSite=Lax cookies for main-frame navigations passed through
  // a service worker.  To fix this we check to see if the original destination
  // of the request was a main frame document and then set a flag indicating
  // SameSite cookies should treat it as a main frame navigation.
  url_request.set_force_main_frame_for_same_site_cookies(
      request.mode == mojom::RequestMode::kNavigate &&
      request.destination == mojom::RequestDestination::kEmpty &&
      request.original_destination == mojom::RequestDestination::kDocument);

  url_request.SetSecureDnsPolicy(
      factory_params.disable_secure_dns ||
              (request.trusted_params &&
               request.trusted_params->disable_secure_dns)
          ? net::SecureDnsPolicy::kDisable
          : net::SecureDnsPolicy::kAllow);

  // |cors_exempt_headers| must be merged here to avoid breaking CORS checks.
  // They are non-empty when the values are given by the UA code, therefore
  // they should be ignored by CORS checks.
  net::HttpRequestHeaders merged_headers = request.headers;
  merged_headers.MergeFrom(ComputeAttributionReportingHeaders(request));
  merged_headers.MergeFrom(request.cors_exempt_headers);
  // This should be ensured by the CorsURLLoaderFactory(), which is called
  // before URLLoaders are created.
  DCHECK(AreRequestHeadersSafe(merged_headers));
  url_request.SetExtraRequestHeaders(std::move(merged_headers));

  url_request.set_accepted_stream_types(request.devtools_accepted_stream_types);

  url_request.set_initiator(request.request_initiator);

  // Note: There are some ordering dependencies here. `SetRequestCredentials`
  // depends on `SetLoadFlags`; `CalculateStorageAccessStatus` depends on
  // `cookie_setting_overrides` and `SetRequestCredentials`.
  // `SetFetchMetadataHeaders` depends on
  // `url_request.storage_access_status()`.
  url_request.cookie_setting_overrides() = CalculateCookieSettingOverrides(
      factory_params.cookie_setting_overrides,
      factory_params.devtools_cookie_setting_overrides, request,
      /*emit_metrics=*/true);
  url_request.SetLoadFlags(request.load_flags);
  SetRequestCredentials(request.url, factory_params.client_security_state,
                        request.mode, request.credentials_mode,
                        request.request_initiator, url_request);
  if (request.credentials_mode == mojom::CredentialsMode::kInclude) {
    url_request.set_storage_access_status(
        url_request.CalculateStorageAccessStatus());
  }

  SetFetchMetadataHeaders(
      url_request, request.mode,
      request.trusted_params && request.trusted_params->has_user_activation,
      request.destination, /*pending_redirect_url=*/std::nullopt,
      factory_params, origin_access_list, request.credentials_mode);

  MaybeSetAcceptSignatureHeader(&url_request, request.expected_public_keys);

  url_request.set_first_party_url_policy(
      request.update_first_party_url_on_redirect
          ? net::RedirectInfo::FirstPartyURLPolicy::UPDATE_URL_ON_REDIRECT
          : net::RedirectInfo::FirstPartyURLPolicy::NEVER_CHANGE_URL);

  url_request.SetPriorityIncremental(request.priority_incremental);
  if (request.socket_tag != net::SocketTag()) {
    url_request.set_socket_tag(request.socket_tag);
  }

  url_request.set_allows_device_bound_session_registration(
      request.allows_device_bound_session_registration);

  if (base::FeatureList::IsEnabled(features::kSendSameSiteLaxForFedCM) &&
      (request.destination == mojom::RequestDestination::kWebIdentity ||
       request.destination == mojom::RequestDestination::kEmailVerification)) {
    // This check is enforced by CorsURLLoaderFactory::IsValidRequest.
    CHECK(request.redirect_mode == mojom::RedirectMode::kError ||
          request.credentials_mode == mojom::CredentialsMode::kOmit);
    url_request.set_ignore_unsafe_method_for_same_site_lax(true);
  }
}

void SetRequestCredentials(
    const GURL& url,
    const network::mojom::ClientSecurityStatePtr& client_security_state,
    mojom::RequestMode request_mode,
    mojom::CredentialsMode credentials_mode,
    const std::optional<url::Origin>& initiator,
    net::URLRequest& url_request) {
  bool policies_allow_credentials = WebPoliciesAllowCredentials(
      url, client_security_state, request_mode, initiator);

  bool allow_credentials =
      ShouldAllowCredentials(credentials_mode) && policies_allow_credentials;

  bool allow_client_certificates =
      ShouldSendClientCertificates(credentials_mode) &&
      policies_allow_credentials;

  // The decision not to include credentials is sticky. This is equivalent to
  // checking the tainted origin flag in the fetch specification.
  if (!allow_credentials) {
    url_request.set_disallow_credentials();
  }
  if (!allow_client_certificates) {
    url_request.set_send_client_certs(false);
  }

  // Contrary to Firefox or blink's cache, the HTTP cache doesn't distinguish
  // requests including user's credentials from the anonymous ones yet. See
  // https://docs.google.com/document/d/1lvbiy4n-GM5I56Ncw304sgvY5Td32R6KHitjRXvkZ6U
  // As a workaround until a solution is implemented, the cached responses
  // aren't used for those requests.
  if (!policies_allow_credentials) {
    url_request.SetLoadFlags(url_request.load_flags() | net::LOAD_BYPASS_CACHE);
  }
}

const mojom::ClientSecurityState* SelectClientSecurityState(
    const mojom::ClientSecurityState*
        url_loader_factory_params_client_security_state,
    const mojom::ClientSecurityState* trusted_params_client_security_state) {
  if (trusted_params_client_security_state) {
    return trusted_params_client_security_state;
  }
  return url_loader_factory_params_client_security_state;
}

mojom::URLResponseHeadPtr BuildResponseHead(
    const net::URLRequest& url_request,
    const net::cookie_util::ParsedRequestCookies& request_cookies,
    network::mojom::IPAddressSpace client_address_space,
    network::mojom::IPAddressSpace response_address_space,
    int32_t url_load_options,
    bool load_with_storage_access,
    bool is_load_timing_enabled,
    bool include_load_timing_internal_info_with_response,
    base::TimeTicks response_start,
    const raw_ptr<mojom::DevToolsObserver> devtools_observer,
    const std::string& devtools_request_id) {
  auto response = mojom::URLResponseHead::New();
  response->request_time = url_request.request_time();
  response->response_time = url_request.response_time();
  response->original_response_time = url_request.original_response_time();
  response->headers = url_request.response_headers();
  response->parsed_headers =
      PopulateParsedHeaders(response->headers.get(), url_request.url());

  url_request.GetCharset(&response->charset);
  response->content_length = url_request.GetExpectedContentSize();
  url_request.GetMimeType(&response->mime_type);

  const net::HttpResponseInfo& response_info = url_request.response_info();
  response->was_fetched_via_spdy = response_info.was_fetched_via_spdy;
  response->was_alpn_negotiated = response_info.was_alpn_negotiated;
  response->alpn_negotiated_protocol = response_info.alpn_negotiated_protocol;
  response->alternate_protocol_usage = response_info.alternate_protocol_usage;
  response->connection_info = response_info.connection_info;
  response->remote_endpoint = response_info.remote_endpoint;
  response->was_fetched_via_cache = url_request.was_cached();
  response->is_validated = (response_info.cache_entry_status ==
                            net::HttpResponseInfo::ENTRY_VALIDATED);
  response->proxy_chain = url_request.proxy_chain();
  response->network_accessed = response_info.network_accessed;
  response->async_revalidation_requested =
      response_info.async_revalidation_requested;
  response->was_in_prefetch_cache =
      !(url_request.load_flags() & net::LOAD_PREFETCH) &&
      response_info.unused_since_prefetch;
  response->did_use_shared_dictionary = response_info.did_use_shared_dictionary;
  response->did_use_server_http_auth = response_info.did_use_server_http_auth;
  response->device_bound_session_usage =
      static_cast<network::mojom::DeviceBoundSessionUsage>(
          url_request.device_bound_session_usage());

  // IsInclude() true means the cookie was sent.
  response->was_cookie_in_request = std::ranges::any_of(
      url_request.maybe_sent_cookies(),
      [](const auto& cookie_with_access_result) {
        return cookie_with_access_result.access_result.status.IsInclude();
      });

  if (is_load_timing_enabled) {
    url_request.GetLoadTimingInfo(&response->load_timing);
  }

  if (include_load_timing_internal_info_with_response) {
    response->load_timing_internal_info =
        url_request.GetLoadTimingInternalInfo();
  }
  CHECK(include_load_timing_internal_info_with_response ||
        !response->load_timing_internal_info);

  if (url_request.ssl_info().cert.get()) {
    response->cert_status = url_request.ssl_info().cert_status;
    if ((url_load_options & mojom::kURLLoadOptionSendSSLInfoWithResponse) ||
        (net::IsCertStatusError(url_request.ssl_info().cert_status) &&
         (url_load_options &
          mojom::kURLLoadOptionSendSSLInfoForCertificateError))) {
      response->ssl_info = url_request.ssl_info();
    }
  }

  response->request_cookies = request_cookies;
  response->request_start = url_request.creation_time();
  response->response_start = response_start;
  response->encoded_data_length = url_request.GetTotalReceivedBytes();
  response->auth_challenge_info = url_request.auth_challenge_info();
  response->has_range_requested = url_request.extra_request_headers().HasHeader(
      net::HttpRequestHeaders::kRange);
  response->dns_aliases =
      base::ToVector(url_request.response_info().dns_aliases);
  // [spec]: https://fetch.spec.whatwg.org/#http-network-or-cache-fetch
  // 13. Set response’s request-includes-credentials to includeCredentials.
  response->request_include_credentials = url_request.allow_credentials();
  response->client_address_space = client_address_space;
  response->response_address_space = response_address_space;
  response->load_with_storage_access = load_with_storage_access;

  url_request.GetClientSideContentDecodingTypes(
      &response->client_side_content_decoding_types);

  if (response->headers) {
    response->unencoded_digests =
        ParseUnencodedDigestsFromHeaders(*response->headers.get());
    ReportUnencodedDigestIssuesToDevtools(
        response->unencoded_digests, devtools_observer, devtools_request_id,
        url_request.url());
  }

  return response;
}

}  // namespace network::url_loader_util

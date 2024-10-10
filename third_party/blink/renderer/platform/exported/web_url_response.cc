/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/web_url_response.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "net/ssl/ssl_info.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/mojom/cors.mojom-shared.h"
#include "services/network/public/mojom/ip_address_space.mojom-shared.h"
#include "services/network/public/mojom/load_timing_info.mojom.h"
#include "services/network/public/mojom/service_worker_router_info.mojom-blink.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/fetch/service_worker_router_info.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

// Converts timing data from |load_timing| to the mojo type.
// TODO:(https://crbug.com/1379780): Consider removing unnecessary type
// conversions.
network::mojom::LoadTimingInfo ToMojoLoadTiming(
    const net::LoadTimingInfo& load_timing) {
  DCHECK(!load_timing.request_start.is_null());

  return network::mojom::LoadTimingInfo(
      load_timing.socket_reused, load_timing.socket_log_id,
      load_timing.request_start_time, load_timing.request_start,
      load_timing.proxy_resolve_start, load_timing.proxy_resolve_end,
      load_timing.connect_timing, load_timing.send_start, load_timing.send_end,
      load_timing.receive_headers_start, load_timing.receive_headers_end,
      load_timing.receive_non_informational_headers_start,
      load_timing.first_early_hints_time, load_timing.push_start,
      load_timing.push_end, load_timing.service_worker_start_time,
      load_timing.service_worker_ready_time,
      load_timing.service_worker_fetch_start,
      load_timing.service_worker_respond_with_settled,
      load_timing.service_worker_router_evaluation_start,
      load_timing.service_worker_cache_lookup_start);
}

// TODO(https://crbug.com/862940): Use KURL here.
void SetSecurityStyleAndDetails(const GURL& url,
                                const network::mojom::URLResponseHead& head,
                                WebURLResponse* response,
                                bool report_security_info) {
  if (!report_security_info) {
    response->SetSecurityStyle(SecurityStyle::kUnknown);
    return;
  }
  if (!url.SchemeIsCryptographic()) {
    // Some origins are considered secure even though they're not cryptographic,
    // so treat them as secure in the UI.
    if (network::IsUrlPotentiallyTrustworthy(url))
      response->SetSecurityStyle(SecurityStyle::kSecure);
    else
      response->SetSecurityStyle(SecurityStyle::kInsecure);
    return;
  }

  // The resource loader does not provide a guarantee that requests always have
  // security info (such as a certificate) attached. Use SecurityStyleUnknown
  // in this case where there isn't enough information to be useful.
  if (!head.ssl_info.has_value()) {
    response->SetSecurityStyle(SecurityStyle::kUnknown);
    return;
  }

  const net::SSLInfo& ssl_info = *head.ssl_info;
  if (net::IsCertStatusError(head.cert_status)) {
    response->SetSecurityStyle(SecurityStyle::kInsecure);
  } else {
    response->SetSecurityStyle(SecurityStyle::kSecure);
  }

  if (!ssl_info.cert) {
    NOTREACHED_IN_MIGRATION();
    response->SetSecurityStyle(SecurityStyle::kUnknown);
    return;
  }

  response->SetSSLInfo(ssl_info);
}

}  // namespace

// static
WebURLResponse WebURLResponse::Create(
    const WebURL& url,
    const network::mojom::URLResponseHead& head,
    bool report_security_info,
    int request_id) {
  WebURLResponse response;

  response.SetCurrentRequestUrl(url);
  response.SetResponseTime(head.response_time);
  response.SetMimeType(WebString::FromUTF8(head.mime_type));
  response.SetTextEncodingName(WebString::FromUTF8(head.charset));
  response.SetExpectedContentLength(head.content_length);
  response.SetHasMajorCertificateErrors(
      net::IsCertStatusError(head.cert_status));
  response.SetHasRangeRequested(head.has_range_requested);
  response.SetTimingAllowPassed(head.timing_allow_passed);
  response.SetWasCached(!head.load_timing.request_start_time.is_null() &&
                        head.response_time <
                            head.load_timing.request_start_time);
  response.SetConnectionID(head.load_timing.socket_log_id);
  response.SetConnectionReused(head.load_timing.socket_reused);
  response.SetWasFetchedViaSPDY(head.was_fetched_via_spdy);
  response.SetWasFetchedViaServiceWorker(head.was_fetched_via_service_worker);
  response.SetDidUseSharedDictionary(head.did_use_shared_dictionary);
  response.SetServiceWorkerResponseSource(head.service_worker_response_source);
  if (!head.service_worker_router_info.is_null()) {
    response.SetServiceWorkerRouterInfo(*head.service_worker_router_info);
  }
  response.SetType(head.response_type);
  response.SetPadding(head.padding);
  WebVector<KURL> url_list_via_service_worker(
      head.url_list_via_service_worker.size());
  base::ranges::transform(head.url_list_via_service_worker,
                          url_list_via_service_worker.begin(),
                          [](const GURL& h) { return KURL(h); });
  response.SetUrlListViaServiceWorker(url_list_via_service_worker);
  response.SetCacheStorageCacheName(
      head.service_worker_response_source ==
              network::mojom::FetchResponseSource::kCacheStorage
          ? WebString::FromUTF8(head.cache_storage_cache_name)
          : WebString());

  WebVector<WebString> dns_aliases(head.dns_aliases.size());
  base::ranges::transform(head.dns_aliases, dns_aliases.begin(),
                          &WebString::FromASCII);
  response.SetDnsAliases(dns_aliases);
  response.SetRemoteIPEndpoint(head.remote_endpoint);
  response.SetAddressSpace(head.response_address_space);
  response.SetClientAddressSpace(head.client_address_space);
  response.SetPrivateNetworkAccessPreflightResult(
      head.private_network_access_preflight_result);

  WebVector<WebString> cors_exposed_header_names(
      head.cors_exposed_header_names.size());
  base::ranges::transform(head.cors_exposed_header_names,
                          cors_exposed_header_names.begin(),
                          [](const auto& header_name) {
                            return WebString::FromLatin1(header_name);
                          });
  response.SetCorsExposedHeaderNames(cors_exposed_header_names);
  response.SetDidServiceWorkerNavigationPreload(
      head.did_service_worker_navigation_preload);
  response.SetIsValidated(head.is_validated);
  response.SetEncodedDataLength(head.encoded_data_length);
  response.SetEncodedBodyLength(
      head.encoded_body_length ? head.encoded_body_length->value : 0);
  response.SetWasAlpnNegotiated(head.was_alpn_negotiated);
  response.SetAlpnNegotiatedProtocol(
      WebString::FromUTF8(head.alpn_negotiated_protocol));
  response.SetAlternateProtocolUsage(head.alternate_protocol_usage);
  response.SetHasAuthorizationCoveredByWildcardOnPreflight(
      head.has_authorization_covered_by_wildcard_on_preflight);
  response.SetWasAlternateProtocolAvailable(
      head.was_alternate_protocol_available);
  response.SetConnectionInfo(head.connection_info);
  response.SetAsyncRevalidationRequested(head.async_revalidation_requested);
  response.SetNetworkAccessed(head.network_accessed);
  response.SetRequestId(request_id);
  response.SetIsSignedExchangeInnerResponse(
      head.is_signed_exchange_inner_response);
  response.SetIsWebBundleInnerResponse(head.is_web_bundle_inner_response);
  response.SetWasInPrefetchCache(head.was_in_prefetch_cache);
  response.SetWasCookieInRequest(head.was_cookie_in_request);
  response.SetRecursivePrefetchToken(head.recursive_prefetch_token);

  SetSecurityStyleAndDetails(GURL(KURL(url)), head, &response,
                             report_security_info);

  // If there's no received headers end time, don't set load timing.  This is
  // the case for non-HTTP requests, requests that don't go over the wire, and
  // certain error cases.
  if (!head.load_timing.receive_headers_end.is_null()) {
    response.SetLoadTiming(ToMojoLoadTiming(head.load_timing));
  }

  response.SetEmittedExtraInfo(head.emitted_extra_info);

  response.SetAuthChallengeInfo(head.auth_challenge_info);
  response.SetRequestIncludeCredentials(head.request_include_credentials);

  response.SetShouldUseSourceHashForJSCodeCache(
      head.should_use_source_hash_for_js_code_cache);

  const net::HttpResponseHeaders* headers = head.headers.get();
  if (!headers)
    return response;

  WebURLResponse::HTTPVersion version = WebURLResponse::kHTTPVersionUnknown;
  if (headers->GetHttpVersion() == net::HttpVersion(0, 9))
    version = WebURLResponse::kHTTPVersion_0_9;
  else if (headers->GetHttpVersion() == net::HttpVersion(1, 0))
    version = WebURLResponse::kHTTPVersion_1_0;
  else if (headers->GetHttpVersion() == net::HttpVersion(1, 1))
    version = WebURLResponse::kHTTPVersion_1_1;
  else if (headers->GetHttpVersion() == net::HttpVersion(2, 0))
    version = WebURLResponse::kHTTPVersion_2_0;
  response.SetHttpVersion(version);
  response.SetHttpStatusCode(headers->response_code());
  response.SetHttpStatusText(WebString::FromLatin1(headers->GetStatusText()));

  // Build up the header map.
  size_t iter = 0;
  std::string name;
  std::string value;
  while (headers->EnumerateHeaderLines(&iter, &name, &value)) {
    response.AddHttpHeaderField(WebString::FromLatin1(name),
                                WebString::FromLatin1(value));
  }

  return response;
}

WebURLResponse::~WebURLResponse() = default;

WebURLResponse::WebURLResponse()
    : owned_resource_response_(std::make_unique<ResourceResponse>()),
      resource_response_(owned_resource_response_.get()) {}

WebURLResponse::WebURLResponse(const WebURLResponse& r)
    : owned_resource_response_(
          std::make_unique<ResourceResponse>(*r.resource_response_)),
      resource_response_(owned_resource_response_.get()) {}

WebURLResponse::WebURLResponse(const WebURL& current_request_url)
    : WebURLResponse() {
  SetCurrentRequestUrl(current_request_url);
}

WebURLResponse& WebURLResponse::operator=(const WebURLResponse& r) {
  // Copying subclasses that have different m_resourceResponse ownership
  // semantics via this operator is just not supported.
  DCHECK(owned_resource_response_);
  DCHECK(resource_response_);
  if (&r != this)
    *resource_response_ = *r.resource_response_;
  return *this;
}

bool WebURLResponse::IsNull() const {
  return resource_response_->IsNull();
}

WebURL WebURLResponse::CurrentRequestUrl() const {
  return resource_response_->CurrentRequestUrl();
}

void WebURLResponse::SetCurrentRequestUrl(const WebURL& url) {
  resource_response_->SetCurrentRequestUrl(url);
}

WebURL WebURLResponse::ResponseUrl() const {
  return resource_response_->ResponseUrl();
}

void WebURLResponse::SetConnectionID(unsigned connection_id) {
  resource_response_->SetConnectionID(connection_id);
}

void WebURLResponse::SetConnectionReused(bool connection_reused) {
  resource_response_->SetConnectionReused(connection_reused);
}

void WebURLResponse::SetLoadTiming(
    const network::mojom::LoadTimingInfo& mojo_timing) {
  auto timing = ResourceLoadTiming::Create();
  timing->SetRequestTime(mojo_timing.request_start);
  timing->SetProxyStart(mojo_timing.proxy_resolve_start);
  timing->SetProxyEnd(mojo_timing.proxy_resolve_end);
  timing->SetDomainLookupStart(mojo_timing.connect_timing.domain_lookup_start);
  timing->SetDomainLookupEnd(mojo_timing.connect_timing.domain_lookup_end);
  timing->SetConnectStart(mojo_timing.connect_timing.connect_start);
  timing->SetConnectEnd(mojo_timing.connect_timing.connect_end);
  timing->SetWorkerStart(mojo_timing.service_worker_start_time);
  timing->SetWorkerRouterEvaluationStart(
      mojo_timing.service_worker_router_evaluation_start);
  timing->SetWorkerCacheLookupStart(
      mojo_timing.service_worker_cache_lookup_start);
  timing->SetWorkerReady(mojo_timing.service_worker_ready_time);
  timing->SetWorkerFetchStart(mojo_timing.service_worker_fetch_start);
  timing->SetWorkerRespondWithSettled(
      mojo_timing.service_worker_respond_with_settled);
  timing->SetSendStart(mojo_timing.send_start);
  timing->SetSendEnd(mojo_timing.send_end);
  timing->SetReceiveHeadersStart(mojo_timing.receive_headers_start);
  timing->SetReceiveHeadersEnd(mojo_timing.receive_headers_end);
  timing->SetReceiveNonInformationalHeaderStart(
      mojo_timing.receive_non_informational_headers_start);
  timing->SetReceiveEarlyHintsStart(mojo_timing.first_early_hints_time);
  timing->SetSslStart(mojo_timing.connect_timing.ssl_start);
  timing->SetSslEnd(mojo_timing.connect_timing.ssl_end);
  timing->SetPushStart(mojo_timing.push_start);
  timing->SetPushEnd(mojo_timing.push_end);
  resource_response_->SetResourceLoadTiming(std::move(timing));
}

base::Time WebURLResponse::ResponseTime() const {
  return resource_response_->ResponseTime();
}

void WebURLResponse::SetResponseTime(base::Time response_time) {
  resource_response_->SetResponseTime(response_time);
}

WebString WebURLResponse::MimeType() const {
  return resource_response_->MimeType();
}

void WebURLResponse::SetMimeType(const WebString& mime_type) {
  resource_response_->SetMimeType(mime_type);
}

int64_t WebURLResponse::ExpectedContentLength() const {
  return resource_response_->ExpectedContentLength();
}

void WebURLResponse::SetExpectedContentLength(int64_t expected_content_length) {
  resource_response_->SetExpectedContentLength(expected_content_length);
}

void WebURLResponse::SetTextEncodingName(const WebString& text_encoding_name) {
  resource_response_->SetTextEncodingName(text_encoding_name);
}

WebURLResponse::HTTPVersion WebURLResponse::HttpVersion() const {
  return static_cast<HTTPVersion>(resource_response_->HttpVersion());
}

void WebURLResponse::SetHttpVersion(HTTPVersion version) {
  resource_response_->SetHttpVersion(
      static_cast<ResourceResponse::HTTPVersion>(version));
}

int WebURLResponse::RequestId() const {
  return resource_response_->RequestId();
}

void WebURLResponse::SetRequestId(int request_id) {
  resource_response_->SetRequestId(request_id);
}

int WebURLResponse::HttpStatusCode() const {
  return resource_response_->HttpStatusCode();
}

void WebURLResponse::SetHttpStatusCode(int http_status_code) {
  resource_response_->SetHttpStatusCode(http_status_code);
}

WebString WebURLResponse::HttpStatusText() const {
  return resource_response_->HttpStatusText();
}

void WebURLResponse::SetHttpStatusText(const WebString& http_status_text) {
  resource_response_->SetHttpStatusText(http_status_text);
}

void WebURLResponse::SetEmittedExtraInfo(bool emitted_extra_info) {
  resource_response_->SetEmittedExtraInfo(emitted_extra_info);
}

WebString WebURLResponse::HttpHeaderField(const WebString& name) const {
  return resource_response_->HttpHeaderField(name);
}

void WebURLResponse::SetHttpHeaderField(const WebString& name,
                                        const WebString& value) {
  resource_response_->SetHttpHeaderField(name, value);
}

void WebURLResponse::AddHttpHeaderField(const WebString& name,
                                        const WebString& value) {
  if (name.IsNull() || value.IsNull())
    return;

  resource_response_->AddHttpHeaderField(name, value);
}

void WebURLResponse::ClearHttpHeaderField(const WebString& name) {
  resource_response_->ClearHttpHeaderField(name);
}

void WebURLResponse::VisitHttpHeaderFields(
    WebHTTPHeaderVisitor* visitor) const {
  const HTTPHeaderMap& map = resource_response_->HttpHeaderFields();
  for (HTTPHeaderMap::const_iterator it = map.begin(); it != map.end(); ++it)
    visitor->VisitHeader(it->key, it->value);
}

void WebURLResponse::SetHasMajorCertificateErrors(bool value) {
  resource_response_->SetHasMajorCertificateErrors(value);
}

void WebURLResponse::SetHasRangeRequested(bool value) {
  resource_response_->SetHasRangeRequested(value);
}

bool WebURLResponse::TimingAllowPassed() const {
  return resource_response_->TimingAllowPassed();
}

void WebURLResponse::SetTimingAllowPassed(bool value) {
  resource_response_->SetTimingAllowPassed(value);
}

void WebURLResponse::SetSecurityStyle(SecurityStyle security_style) {
  resource_response_->SetSecurityStyle(security_style);
}

void WebURLResponse::SetSSLInfo(const net::SSLInfo& ssl_info) {
  resource_response_->SetSSLInfo(ssl_info);
}

const ResourceResponse& WebURLResponse::ToResourceResponse() const {
  return *resource_response_;
}

void WebURLResponse::SetWasCached(bool value) {
  resource_response_->SetWasCached(value);
}

bool WebURLResponse::WasFetchedViaSPDY() const {
  return resource_response_->WasFetchedViaSPDY();
}

void WebURLResponse::SetWasFetchedViaSPDY(bool value) {
  resource_response_->SetWasFetchedViaSPDY(value);
}

bool WebURLResponse::WasFetchedViaServiceWorker() const {
  return resource_response_->WasFetchedViaServiceWorker();
}

void WebURLResponse::SetWasFetchedViaServiceWorker(bool value) {
  resource_response_->SetWasFetchedViaServiceWorker(value);
}

network::mojom::FetchResponseSource
WebURLResponse::GetServiceWorkerResponseSource() const {
  return resource_response_->GetServiceWorkerResponseSource();
}

void WebURLResponse::SetServiceWorkerRouterInfo(
    const network::mojom::ServiceWorkerRouterInfo& value) {
  auto info = ServiceWorkerRouterInfo::Create();
  info->SetRuleIdMatched(value.rule_id_matched);
  info->SetMatchedSourceType(value.matched_source_type);
  info->SetActualSourceType(value.actual_source_type);
  info->SetRouteRuleNum(value.route_rule_num);
  info->SetEvaluationWorkerStatus(value.evaluation_worker_status);
  info->SetRouterEvaluationTime(value.router_evaluation_time);
  info->SetCacheLookupTime(value.cache_lookup_time);
  resource_response_->SetServiceWorkerRouterInfo(std::move(info));
}

void WebURLResponse::SetServiceWorkerResponseSource(
    network::mojom::FetchResponseSource value) {
  resource_response_->SetServiceWorkerResponseSource(value);
}

void WebURLResponse::SetDidUseSharedDictionary(bool did_use_shared_dictionary) {
  resource_response_->SetDidUseSharedDictionary(did_use_shared_dictionary);
}

void WebURLResponse::SetType(network::mojom::FetchResponseType value) {
  resource_response_->SetType(value);
}

network::mojom::FetchResponseType WebURLResponse::GetType() const {
  return resource_response_->GetType();
}

void WebURLResponse::SetPadding(int64_t padding) {
  resource_response_->SetPadding(padding);
}

int64_t WebURLResponse::GetPadding() const {
  return resource_response_->GetPadding();
}

void WebURLResponse::SetUrlListViaServiceWorker(
    const WebVector<WebURL>& url_list_via_service_worker) {
  Vector<KURL> url_list(
      base::checked_cast<wtf_size_t>(url_list_via_service_worker.size()));
  base::ranges::copy(url_list_via_service_worker, url_list.begin());
  resource_response_->SetUrlListViaServiceWorker(url_list);
}

bool WebURLResponse::HasUrlListViaServiceWorker() const {
  DCHECK(resource_response_->UrlListViaServiceWorker().size() == 0 ||
         WasFetchedViaServiceWorker());
  return resource_response_->UrlListViaServiceWorker().size() > 0;
}

WebString WebURLResponse::CacheStorageCacheName() const {
  return resource_response_->CacheStorageCacheName();
}

void WebURLResponse::SetCacheStorageCacheName(
    const WebString& cache_storage_cache_name) {
  resource_response_->SetCacheStorageCacheName(cache_storage_cache_name);
}

WebVector<WebString> WebURLResponse::CorsExposedHeaderNames() const {
  return resource_response_->CorsExposedHeaderNames();
}

void WebURLResponse::SetCorsExposedHeaderNames(
    const WebVector<WebString>& header_names) {
  Vector<String> exposed_header_names;
  exposed_header_names.AppendSpan(base::span(header_names));
  resource_response_->SetCorsExposedHeaderNames(exposed_header_names);
}

void WebURLResponse::SetDidServiceWorkerNavigationPreload(bool value) {
  resource_response_->SetDidServiceWorkerNavigationPreload(value);
}

net::IPEndPoint WebURLResponse::RemoteIPEndpoint() const {
  return resource_response_->RemoteIPEndpoint();
}

void WebURLResponse::SetRemoteIPEndpoint(
    const net::IPEndPoint& remote_ip_endpoint) {
  resource_response_->SetRemoteIPEndpoint(remote_ip_endpoint);
}

network::mojom::IPAddressSpace WebURLResponse::AddressSpace() const {
  return resource_response_->AddressSpace();
}

void WebURLResponse::SetAddressSpace(
    network::mojom::IPAddressSpace remote_ip_address_space) {
  resource_response_->SetAddressSpace(remote_ip_address_space);
}

network::mojom::IPAddressSpace WebURLResponse::ClientAddressSpace() const {
  return resource_response_->ClientAddressSpace();
}

void WebURLResponse::SetClientAddressSpace(
    network::mojom::IPAddressSpace client_address_space) {
  resource_response_->SetClientAddressSpace(client_address_space);
}

network::mojom::PrivateNetworkAccessPreflightResult
WebURLResponse::PrivateNetworkAccessPreflightResult() const {
  return resource_response_->PrivateNetworkAccessPreflightResult();
}

void WebURLResponse::SetPrivateNetworkAccessPreflightResult(
    network::mojom::PrivateNetworkAccessPreflightResult result) {
  resource_response_->SetPrivateNetworkAccessPreflightResult(result);
}

void WebURLResponse::SetIsValidated(bool is_validated) {
  resource_response_->SetIsValidated(is_validated);
}

void WebURLResponse::SetEncodedDataLength(int64_t length) {
  resource_response_->SetEncodedDataLength(length);
}

int64_t WebURLResponse::EncodedBodyLength() const {
  return resource_response_->EncodedBodyLength();
}

void WebURLResponse::SetEncodedBodyLength(uint64_t length) {
  resource_response_->SetEncodedBodyLength(length);
}

void WebURLResponse::SetIsSignedExchangeInnerResponse(
    bool is_signed_exchange_inner_response) {
  resource_response_->SetIsSignedExchangeInnerResponse(
      is_signed_exchange_inner_response);
}

void WebURLResponse::SetIsWebBundleInnerResponse(
    bool is_web_bundle_inner_response) {
  resource_response_->SetIsWebBundleInnerResponse(is_web_bundle_inner_response);
}

void WebURLResponse::SetWasInPrefetchCache(bool was_in_prefetch_cache) {
  resource_response_->SetWasInPrefetchCache(was_in_prefetch_cache);
}

void WebURLResponse::SetWasCookieInRequest(bool was_cookie_in_request) {
  resource_response_->SetWasCookieInRequest(was_cookie_in_request);
}

void WebURLResponse::SetRecursivePrefetchToken(
    const std::optional<base::UnguessableToken>& token) {
  resource_response_->SetRecursivePrefetchToken(token);
}

bool WebURLResponse::WasAlpnNegotiated() const {
  return resource_response_->WasAlpnNegotiated();
}

void WebURLResponse::SetWasAlpnNegotiated(bool was_alpn_negotiated) {
  resource_response_->SetWasAlpnNegotiated(was_alpn_negotiated);
}

WebString WebURLResponse::AlpnNegotiatedProtocol() const {
  return resource_response_->AlpnNegotiatedProtocol();
}

void WebURLResponse::SetAlpnNegotiatedProtocol(
    const WebString& alpn_negotiated_protocol) {
  resource_response_->SetAlpnNegotiatedProtocol(alpn_negotiated_protocol);
}

void WebURLResponse::SetAlternateProtocolUsage(
    const net::AlternateProtocolUsage alternate_protocol_usage) {
  resource_response_->SetAlternateProtocolUsage(alternate_protocol_usage);
}

bool WebURLResponse::HasAuthorizationCoveredByWildcardOnPreflight() const {
  return resource_response_->HasAuthorizationCoveredByWildcardOnPreflight();
}

void WebURLResponse::SetHasAuthorizationCoveredByWildcardOnPreflight(bool b) {
  resource_response_->SetHasAuthorizationCoveredByWildcardOnPreflight(b);
}

bool WebURLResponse::WasAlternateProtocolAvailable() const {
  return resource_response_->WasAlternateProtocolAvailable();
}

void WebURLResponse::SetWasAlternateProtocolAvailable(
    bool was_alternate_protocol_available) {
  resource_response_->SetWasAlternateProtocolAvailable(
      was_alternate_protocol_available);
}

net::HttpConnectionInfo WebURLResponse::ConnectionInfo() const {
  return resource_response_->ConnectionInfo();
}

void WebURLResponse::SetConnectionInfo(
    net::HttpConnectionInfo connection_info) {
  resource_response_->SetConnectionInfo(connection_info);
}

void WebURLResponse::SetAsyncRevalidationRequested(bool requested) {
  resource_response_->SetAsyncRevalidationRequested(requested);
}

void WebURLResponse::SetNetworkAccessed(bool network_accessed) {
  resource_response_->SetNetworkAccessed(network_accessed);
}

bool WebURLResponse::FromArchive() const {
  return resource_response_->FromArchive();
}

void WebURLResponse::SetDnsAliases(const WebVector<WebString>& aliases) {
  Vector<String> dns_aliases(base::checked_cast<wtf_size_t>(aliases.size()));
  base::ranges::transform(aliases, dns_aliases.begin(),
                          &WebString::operator WTF::String);
  resource_response_->SetDnsAliases(std::move(dns_aliases));
}

void WebURLResponse::SetAuthChallengeInfo(
    const std::optional<net::AuthChallengeInfo>& auth_challenge_info) {
  resource_response_->SetAuthChallengeInfo(auth_challenge_info);
}

const std::optional<net::AuthChallengeInfo>& WebURLResponse::AuthChallengeInfo()
    const {
  return resource_response_->AuthChallengeInfo();
}

void WebURLResponse::SetRequestIncludeCredentials(
    bool request_include_credentials) {
  resource_response_->SetRequestIncludeCredentials(request_include_credentials);
}

bool WebURLResponse::RequestIncludeCredentials() const {
  return resource_response_->RequestIncludeCredentials();
}

void WebURLResponse::SetShouldUseSourceHashForJSCodeCache(
    bool should_use_source_hash_for_js_code_cache) {
  resource_response_->SetShouldUseSourceHashForJSCodeCache(
      should_use_source_hash_for_js_code_cache);
}

bool WebURLResponse::ShouldUseSourceHashForJSCodeCache() const {
  return resource_response_->ShouldUseSourceHashForJSCodeCache();
}

WebURLResponse::WebURLResponse(ResourceResponse& r) : resource_response_(&r) {}

}  // namespace blink

/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_RESPONSE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_RESPONSE_H_

#include <memory>
#include <utility>

#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "third_party/blink/public/platform/web_url_response.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_parsers.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"

#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class ResourceLoadTiming;
struct ResourceLoadInfo;

// A ResourceResponse is a "response" object used in blink. Conceptually
// it is https://fetch.spec.whatwg.org/#concept-response, but it contains
// a lot of blink specific fields. WebURLResponse is the "public version"
// of this class and public classes (i.e., classes in public/platform) use it.
//
// This class is thread-bound. Do not copy/pass an instance across threads.
class PLATFORM_EXPORT ResourceResponse final {
  USING_FAST_MALLOC(ResourceResponse);

 public:
  enum HTTPVersion : uint8_t {
    kHTTPVersionUnknown,
    kHTTPVersion_0_9,
    kHTTPVersion_1_0,
    kHTTPVersion_1_1,
    kHTTPVersion_2_0
  };

  enum CTPolicyCompliance {
    kCTPolicyComplianceDetailsNotAvailable,
    kCTPolicyComplies,
    kCTPolicyDoesNotComply
  };

  class PLATFORM_EXPORT SignedCertificateTimestamp final {
    DISALLOW_NEW();

   public:
    SignedCertificateTimestamp(String status,
                               String origin,
                               String log_description,
                               String log_id,
                               int64_t timestamp,
                               String hash_algorithm,
                               String signature_algorithm,
                               String signature_data)
        : status_(status),
          origin_(origin),
          log_description_(log_description),
          log_id_(log_id),
          timestamp_(timestamp),
          hash_algorithm_(hash_algorithm),
          signature_algorithm_(signature_algorithm),
          signature_data_(signature_data) {}
    explicit SignedCertificateTimestamp(
        const struct blink::WebURLResponse::SignedCertificateTimestamp&);
    SignedCertificateTimestamp IsolatedCopy() const;

    String status_;
    String origin_;
    String log_description_;
    String log_id_;
    int64_t timestamp_;
    String hash_algorithm_;
    String signature_algorithm_;
    String signature_data_;
  };

  using SignedCertificateTimestampList =
      WTF::Vector<SignedCertificateTimestamp>;

  struct SecurityDetails {
    DISALLOW_NEW();
    SecurityDetails(const String& protocol,
                    const String& key_exchange,
                    const String& key_exchange_group,
                    const String& cipher,
                    const String& mac,
                    const String& subject_name,
                    const Vector<String>& san_list,
                    const String& issuer,
                    time_t valid_from,
                    time_t valid_to,
                    const Vector<AtomicString>& certificate,
                    const SignedCertificateTimestampList& sct_list)
        : protocol(protocol),
          key_exchange(key_exchange),
          key_exchange_group(key_exchange_group),
          cipher(cipher),
          mac(mac),
          subject_name(subject_name),
          san_list(san_list),
          issuer(issuer),
          valid_from(valid_from),
          valid_to(valid_to),
          certificate(certificate),
          sct_list(sct_list) {}
    // All strings are human-readable values.
    String protocol;
    // keyExchange is the empty string if not applicable for the connection's
    // protocol.
    String key_exchange;
    // keyExchangeGroup is the empty string if not applicable for the
    // connection's key exchange.
    String key_exchange_group;
    String cipher;
    // mac is the empty string when the connection cipher suite does not
    // have a separate MAC value (i.e. if the cipher suite is AEAD).
    String mac;
    String subject_name;
    Vector<String> san_list;
    String issuer;
    time_t valid_from;
    time_t valid_to;
    // DER-encoded X509Certificate certificate chain.
    Vector<AtomicString> certificate;
    SignedCertificateTimestampList sct_list;
  };

  ResourceResponse();
  explicit ResourceResponse(const KURL& current_request_url);
  ResourceResponse(const ResourceResponse&);
  ResourceResponse& operator=(const ResourceResponse&);
  ~ResourceResponse();

  bool IsNull() const { return is_null_; }
  bool IsHTTP() const;

  // The current request URL for this resource (the URL after redirects).
  // Corresponds to:
  // https://fetch.spec.whatwg.org/#concept-request-current-url
  //
  // Beware that this might not be the same the response URL, so it is usually
  // incorrect to use this in security checks. Use GetType() to determine origin
  // sameness.
  //
  // Specifically, if a service worker responded to the request for this
  // resource, it may have fetched an entirely different URL and responded with
  // that resource. WasFetchedViaServiceWorker() and ResponseUrl() can be used
  // to determine whether and how a service worker responded to the request.
  // Example service worker code:
  //
  // onfetch = (event => {
  //   if (event.request.url == 'https://abc.com')
  //     event.respondWith(fetch('https://def.com'));
  // });
  //
  // If this service worker responds to an "https://abc.com" request, then for
  // the resulting ResourceResponse, CurrentRequestUrl() is "https://abc.com",
  // WasFetchedViaServiceWorker() is true, and ResponseUrl() is
  // "https://def.com".
  const KURL& CurrentRequestUrl() const;
  void SetCurrentRequestUrl(const KURL&);

  // The response URL of this resource. Corresponds to:
  // https://fetch.spec.whatwg.org/#concept-response-url
  //
  // This returns the same URL as CurrentRequestUrl() unless a service worker
  // responded to the request. See the comments for that function.
  KURL ResponseUrl() const;

  // Returns true if this response is the result of a service worker
  // effectively calling `evt.respondWith(fetch(evt.request))`.  Specifically,
  // it returns false for synthetic constructed responses, responses fetched
  // from different URLs, and responses produced by cache_storage.
  bool IsServiceWorkerPassThrough() const;

  const AtomicString& MimeType() const;
  void SetMimeType(const AtomicString&);

  int64_t ExpectedContentLength() const;
  void SetExpectedContentLength(int64_t);

  const AtomicString& TextEncodingName() const;
  void SetTextEncodingName(const AtomicString&);

  int HttpStatusCode() const;
  void SetHttpStatusCode(int);

  const AtomicString& HttpStatusText() const;
  void SetHttpStatusText(const AtomicString&);

  const AtomicString& HttpHeaderField(const AtomicString& name) const;
  void SetHttpHeaderField(const AtomicString& name, const AtomicString& value);
  void AddHttpHeaderField(const AtomicString& name, const AtomicString& value);
  void AddHttpHeaderFieldWithMultipleValues(const AtomicString& name,
                                            const Vector<AtomicString>& values);
  void ClearHttpHeaderField(const AtomicString& name);
  const HTTPHeaderMap& HttpHeaderFields() const;

  bool IsAttachment() const;

  AtomicString HttpContentType() const;

  // These functions return parsed values of the corresponding response headers.
  // NaN means that the header was not present or had invalid value.
  bool CacheControlContainsNoCache() const;
  bool CacheControlContainsNoStore() const;
  bool CacheControlContainsMustRevalidate() const;
  bool HasCacheValidatorFields() const;
  base::Optional<base::TimeDelta> CacheControlMaxAge() const;
  base::Optional<base::Time> Date() const;
  base::Optional<base::TimeDelta> Age() const;
  base::Optional<base::Time> Expires() const;
  base::Optional<base::Time> LastModified() const;
  // Will always return values >= 0.
  base::TimeDelta CacheControlStaleWhileRevalidate() const;

  unsigned ConnectionID() const;
  void SetConnectionID(unsigned);

  bool ConnectionReused() const;
  void SetConnectionReused(bool);

  bool WasCached() const;
  void SetWasCached(bool);

  ResourceLoadTiming* GetResourceLoadTiming() const;
  void SetResourceLoadTiming(scoped_refptr<ResourceLoadTiming>);

  scoped_refptr<ResourceLoadInfo> GetResourceLoadInfo() const;
  void SetResourceLoadInfo(scoped_refptr<ResourceLoadInfo>);

  HTTPVersion HttpVersion() const { return http_version_; }
  void SetHttpVersion(HTTPVersion version) { http_version_ = version; }

  int RequestId() const { return request_id_; }
  void SetRequestId(int request_id) { request_id_ = request_id; }

  bool HasMajorCertificateErrors() const {
    return has_major_certificate_errors_;
  }
  void SetHasMajorCertificateErrors(bool has_major_certificate_errors) {
    has_major_certificate_errors_ = has_major_certificate_errors;
  }

  CTPolicyCompliance GetCTPolicyCompliance() const {
    return ct_policy_compliance_;
  }
  void SetCTPolicyCompliance(CTPolicyCompliance);

  bool IsLegacyTLSVersion() const { return is_legacy_tls_version_; }
  void SetIsLegacyTLSVersion(bool value) { is_legacy_tls_version_ = value; }

  SecurityStyle GetSecurityStyle() const { return security_style_; }
  void SetSecurityStyle(SecurityStyle security_style) {
    security_style_ = security_style;
  }

  const base::Optional<SecurityDetails>& GetSecurityDetails() const {
    return security_details_;
  }
  void SetSecurityDetails(const String& protocol,
                          const String& key_exchange,
                          const String& key_exchange_group,
                          const String& cipher,
                          const String& mac,
                          const String& subject_name,
                          const Vector<String>& san_list,
                          const String& issuer,
                          time_t valid_from,
                          time_t valid_to,
                          const Vector<AtomicString>& certificate,
                          const SignedCertificateTimestampList& sct_list);

  int64_t AppCacheID() const { return app_cache_id_; }
  void SetAppCacheID(int64_t id) { app_cache_id_ = id; }

  const KURL& AppCacheManifestURL() const { return app_cache_manifest_url_; }
  void SetAppCacheManifestURL(const KURL& url) {
    app_cache_manifest_url_ = url;
  }

  bool WasFetchedViaSPDY() const { return was_fetched_via_spdy_; }
  void SetWasFetchedViaSPDY(bool value) { was_fetched_via_spdy_ = value; }

  // See network::ResourceResponseInfo::was_fetched_via_service_worker.
  bool WasFetchedViaServiceWorker() const {
    return was_fetched_via_service_worker_;
  }
  void SetWasFetchedViaServiceWorker(bool value) {
    was_fetched_via_service_worker_ = value;
  }

  // See network::ResourceResponseInfo::was_fallback_required_by_service_worker.
  bool WasFallbackRequiredByServiceWorker() const {
    return was_fallback_required_by_service_worker_;
  }
  void SetWasFallbackRequiredByServiceWorker(bool value) {
    was_fallback_required_by_service_worker_ = value;
  }

  network::mojom::FetchResponseType GetType() const { return response_type_; }
  void SetType(network::mojom::FetchResponseType value) {
    response_type_ = value;
  }
  // https://html.spec.whatwg.org/C/#cors-same-origin
  bool IsCorsSameOrigin() const;
  // https://html.spec.whatwg.org/C/#cors-cross-origin
  bool IsCorsCrossOrigin() const;

  // See network::ResourceResponseInfo::url_list_via_service_worker.
  const Vector<KURL>& UrlListViaServiceWorker() const {
    return url_list_via_service_worker_;
  }
  void SetUrlListViaServiceWorker(const Vector<KURL>& url_list) {
    url_list_via_service_worker_ = url_list;
  }

  const String& CacheStorageCacheName() const {
    return cache_storage_cache_name_;
  }
  void SetCacheStorageCacheName(const String& cache_storage_cache_name) {
    cache_storage_cache_name_ = cache_storage_cache_name;
  }

  const Vector<String>& CorsExposedHeaderNames() const {
    return cors_exposed_header_names_;
  }
  void SetCorsExposedHeaderNames(const Vector<String>& header_names) {
    cors_exposed_header_names_ = header_names;
  }

  bool DidServiceWorkerNavigationPreload() const {
    return did_service_worker_navigation_preload_;
  }
  void SetDidServiceWorkerNavigationPreload(bool value) {
    did_service_worker_navigation_preload_ = value;
  }

  base::Time ResponseTime() const { return response_time_; }
  void SetResponseTime(base::Time response_time) {
    response_time_ = response_time;
  }

  const AtomicString& RemoteIPAddress() const { return remote_ip_address_; }
  void SetRemoteIPAddress(const AtomicString& value) {
    remote_ip_address_ = value;
  }

  uint16_t RemotePort() const { return remote_port_; }
  void SetRemotePort(uint16_t value) { remote_port_ = value; }

  bool WasAlpnNegotiated() const { return was_alpn_negotiated_; }
  void SetWasAlpnNegotiated(bool was_alpn_negotiated) {
    was_alpn_negotiated_ = was_alpn_negotiated;
  }

  const AtomicString& AlpnNegotiatedProtocol() const {
    return alpn_negotiated_protocol_;
  }
  void SetAlpnNegotiatedProtocol(const AtomicString& value) {
    alpn_negotiated_protocol_ = value;
  }

  net::HttpResponseInfo::ConnectionInfo ConnectionInfo() const {
    return connection_info_;
  }
  void SetConnectionInfo(net::HttpResponseInfo::ConnectionInfo value) {
    connection_info_ = value;
  }

  AtomicString ConnectionInfoString() const;

  int64_t EncodedDataLength() const { return encoded_data_length_; }
  void SetEncodedDataLength(int64_t value);

  int64_t EncodedBodyLength() const { return encoded_body_length_; }
  void SetEncodedBodyLength(int64_t value);

  int64_t DecodedBodyLength() const { return decoded_body_length_; }
  void SetDecodedBodyLength(int64_t value);

  const base::Optional<base::UnguessableToken>& RecursivePrefetchToken() const {
    return recursive_prefetch_token_;
  }
  void SetRecursivePrefetchToken(
      const base::Optional<base::UnguessableToken>& token) {
    recursive_prefetch_token_ = token;
  }

  unsigned MemoryUsage() const {
    // average size, mostly due to URL and Header Map strings
    return 1280;
  }

  bool AsyncRevalidationRequested() const {
    return async_revalidation_requested_;
  }

  void SetAsyncRevalidationRequested(bool requested) {
    async_revalidation_requested_ = requested;
  }

  bool NetworkAccessed() const { return network_accessed_; }

  void SetNetworkAccessed(bool network_accessed) {
    network_accessed_ = network_accessed;
  }

  bool FromArchive() const { return from_archive_; }

  void SetFromArchive(bool from_archive) { from_archive_ = from_archive; }

  bool WasAlternateProtocolAvailable() const {
    return was_alternate_protocol_available_;
  }

  void SetWasAlternateProtocolAvailable(bool was_alternate_protocol_available) {
    was_alternate_protocol_available_ = was_alternate_protocol_available;
  }

  bool IsSignedExchangeInnerResponse() const {
    return is_signed_exchange_inner_response_;
  }

  void SetIsSignedExchangeInnerResponse(
      bool is_signed_exchange_inner_response) {
    is_signed_exchange_inner_response_ = is_signed_exchange_inner_response;
  }

  bool WasInPrefetchCache() const { return was_in_prefetch_cache_; }

  void SetWasInPrefetchCache(bool was_in_prefetch_cache) {
    was_in_prefetch_cache_ = was_in_prefetch_cache;
  }

 private:
  void UpdateHeaderParsedState(const AtomicString& name);

  KURL current_request_url_;
  AtomicString mime_type_;
  int64_t expected_content_length_ = 0;
  AtomicString text_encoding_name_;

  unsigned connection_id_ = 0;
  int http_status_code_ = 0;
  AtomicString http_status_text_;
  HTTPHeaderMap http_header_fields_;

  // Remote IP address of the socket which fetched this resource.
  AtomicString remote_ip_address_;

  // Remote port number of the socket which fetched this resource.
  uint16_t remote_port_ = 0;

  bool was_cached_ = false;
  bool connection_reused_ = false;
  bool is_null_ = false;
  mutable bool have_parsed_age_header_ = false;
  mutable bool have_parsed_date_header_ = false;
  mutable bool have_parsed_expires_header_ = false;
  mutable bool have_parsed_last_modified_header_ = false;

  // True if the resource was retrieved by the embedder in spite of
  // certificate errors.
  bool has_major_certificate_errors_ = false;

  // The Certificate Transparency policy compliance status of the resource.
  CTPolicyCompliance ct_policy_compliance_ =
      kCTPolicyComplianceDetailsNotAvailable;

  // True if the response was sent over TLS 1.0 or 1.1, which are deprecated and
  // will be removed in the future.
  bool is_legacy_tls_version_ = false;

  // The time at which the resource's certificate expires. Null if there was no
  // certificate.
  base::Time cert_validity_start_;

  // Was the resource fetched over SPDY.  See http://dev.chromium.org/spdy
  bool was_fetched_via_spdy_ = false;

  // Was the resource fetched over a ServiceWorker.
  bool was_fetched_via_service_worker_ = false;

  // Was the fallback request with skip service worker flag required.
  bool was_fallback_required_by_service_worker_ = false;

  // True if service worker navigation preload was performed due to
  // the request for this resource.
  bool did_service_worker_navigation_preload_ = false;

  // True if this resource is stale and needs async revalidation. Will only
  // possibly be set if the load_flags indicated SUPPORT_ASYNC_REVALIDATION.
  bool async_revalidation_requested_ = false;

  // True if this resource is from an inner response of a signed exchange.
  // https://wicg.github.io/webpackage/draft-yasskin-http-origin-signed-responses.html
  bool is_signed_exchange_inner_response_ = false;

  // True if this resource is served from the prefetch cache.
  bool was_in_prefetch_cache_ = false;

  // True if this resource was loaded from the network.
  bool network_accessed_ = false;

  // True if this resource was loaded from a MHTML archive.
  bool from_archive_ = false;

  // True if response could use alternate protocol.
  bool was_alternate_protocol_available_ = false;

  // True if the response was delivered after ALPN is negotiated.
  bool was_alpn_negotiated_ = false;

  // https://fetch.spec.whatwg.org/#concept-response-type
  network::mojom::FetchResponseType response_type_;

  // HTTP version used in the response, if known.
  HTTPVersion http_version_ = kHTTPVersionUnknown;

  // Request id given to the resource by the WebUrlLoader.
  int request_id_ = 0;

  // The security style of the resource.
  // This only contains a valid value when the DevTools Network domain is
  // enabled. (Otherwise, it contains a default value of Unknown.)
  SecurityStyle security_style_ = SecurityStyle::kUnknown;

  // Security details of this request's connection.
  base::Optional<SecurityDetails> security_details_;

  scoped_refptr<ResourceLoadTiming> resource_load_timing_;
  scoped_refptr<ResourceLoadInfo> resource_load_info_;

  mutable CacheControlHeader cache_control_header_;

  mutable base::Optional<base::TimeDelta> age_;
  mutable base::Optional<base::Time> date_;
  mutable base::Optional<base::Time> expires_;
  mutable base::Optional<base::Time> last_modified_;

  // The id of the appcache this response was retrieved from, or zero if
  // the response was not retrieved from an appcache.
  int64_t app_cache_id_ = 0;

  // The manifest url of the appcache this response was retrieved from, if any.
  // Note: only valid for main resource responses.
  KURL app_cache_manifest_url_;

  // The URL list of the response which was fetched by the ServiceWorker.
  // This is empty if the response was created inside the ServiceWorker.
  Vector<KURL> url_list_via_service_worker_;

  // The cache name of the CacheStorage from where the response is served via
  // the ServiceWorker. Null if the response isn't from the CacheStorage.
  String cache_storage_cache_name_;

  // The headers that should be exposed according to CORS. Only guaranteed
  // to be set if the response was fetched by a ServiceWorker.
  Vector<String> cors_exposed_header_names_;

  // The time at which the response headers were received.  For cached
  // responses, this time could be "far" in the past.
  base::Time response_time_;

  // ALPN negotiated protocol of the socket which fetched this resource.
  AtomicString alpn_negotiated_protocol_;

  // Information about the type of connection used to fetch this resource.
  net::HttpResponseInfo::ConnectionInfo connection_info_ =
      net::HttpResponseInfo::ConnectionInfo::CONNECTION_INFO_UNKNOWN;

  // Size of the response in bytes prior to decompression.
  int64_t encoded_data_length_ = 0;

  // Size of the response body in bytes prior to decompression.
  int64_t encoded_body_length_ = 0;

  // Sizes of the response body in bytes after any content-encoding is
  // removed.
  int64_t decoded_body_length_ = 0;

  // This is propagated from the browser process's PrefetchURLLoader on
  // cross-origin prefetch responses. It is used to pass the token along to
  // preload header requests from these responses.
  base::Optional<base::UnguessableToken> recursive_prefetch_token_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_RESOURCE_RESPONSE_H_

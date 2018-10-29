// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_RESPONSE_INFO_H_
#define SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_RESPONSE_INFO_H_

#include <stdint.h>

#include <string>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_timing_info.h"
#include "net/base/proxy_server.h"
#include "net/cert/ct_policy_status.h"
#include "net/cert/signed_certificate_timestamp_and_status.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_response_info.h"
#include "net/nqe/effective_connection_type.h"
#include "services/network/public/cpp/http_raw_request_response_info.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "url/gurl.h"

namespace network {

// NOTE: when modifying this structure, also update ResourceResponse::DeepCopy
// in resource_response.cc.
struct COMPONENT_EXPORT(NETWORK_CPP_BASE) ResourceResponseInfo {
  ResourceResponseInfo();
  ResourceResponseInfo(const ResourceResponseInfo& other);
  ~ResourceResponseInfo();

  // The time at which the request was made that resulted in this response.
  // For cached responses, this time could be "far" in the past.
  base::Time request_time;

  // The time at which the response headers were received.  For cached
  // responses, this time could be "far" in the past.
  base::Time response_time;

  // The response headers or NULL if the URL type does not support headers.
  scoped_refptr<net::HttpResponseHeaders> headers;

  // The mime type of the response.  This may be a derived value.
  std::string mime_type;

  // The character encoding of the response or none if not applicable to the
  // response's mime type.  This may be a derived value.
  std::string charset;

  // The resource's compliance with the Certificate Transparency policy.
  net::ct::CTPolicyCompliance ct_policy_compliance;

  // True if the resource was loaded with an otherwise-valid legacy Symantec
  // certificate which will be distrusted in future.
  bool is_legacy_symantec_cert;

  // Content length if available. -1 if not available
  int64_t content_length;

  // Length of the encoded data transferred over the network. In case there is
  // no data, contains -1.
  int64_t encoded_data_length;

  // Length of the response body data before decompression. -1 unless the body
  // has been read to the end.
  int64_t encoded_body_length;

  // True if the request accessed the network in the process of retrieving data.
  bool network_accessed;

  // The appcache this response was loaded from, or kAppCacheNoCacheId.
  // TODO(rdsmith): Remove conceptual dependence on appcache.
  int64_t appcache_id;

  // The manifest url of the appcache this response was loaded from.
  // Note: this value is only populated for main resource requests.
  GURL appcache_manifest_url;

  // Detailed timing information used by the WebTiming, HAR and Developer
  // Tools.  Includes socket ID and socket reuse information.
  net::LoadTimingInfo load_timing;

  // Actual request and response headers, as obtained from the network stack.
  // Only present if the renderer set report_raw_headers to true and had the
  // CanReadRawCookies permission.
  scoped_refptr<HttpRawRequestResponseInfo> raw_request_response_info;

  // True if the response was delivered using SPDY.
  bool was_fetched_via_spdy;

  // True if the response was delivered after NPN is negotiated.
  bool was_alpn_negotiated;

  // True if response could use alternate protocol. However, browser will
  // ignore the alternate protocol when spdy is not enabled on browser side.
  bool was_alternate_protocol_available;

  // Information about the type of connection used to fetch this response.
  net::HttpResponseInfo::ConnectionInfo connection_info;

  // ALPN protocol negotiated with the server.
  std::string alpn_negotiated_protocol;

  // Remote address of the socket which fetched this resource.
  net::HostPortPair socket_address;

  // True if the response came from cache.
  bool was_fetched_via_cache = false;

  // True if the response was delivered through a proxy.
  bool was_fetched_via_proxy;

  // The proxy server used for this request, if any.
  net::ProxyServer proxy_server;

  // True if the response was fetched by a ServiceWorker.
  bool was_fetched_via_service_worker;

  // True when a request whose mode is |CORS| or |CORS-with-forced-preflight|
  // is sent to a ServiceWorker but FetchEvent.respondWith is not called. So the
  // renderer has to resend the request with skip service worker flag
  // considering the CORS preflight logic.
  bool was_fallback_required_by_service_worker;

  // The URL list of the response which was served by the ServiceWorker. See
  // ServiceWorkerResponseInfo::url_list_via_service_worker().
  std::vector<GURL> url_list_via_service_worker;

  // https://fetch.spec.whatwg.org/#concept-response-type
  mojom::FetchResponseType response_type;

  // The time immediately before starting ServiceWorker. If the response is not
  // provided by the ServiceWorker, kept empty.
  // TODO(ksakamoto): Move this to net::LoadTimingInfo.
  base::TimeTicks service_worker_start_time;

  // The time immediately before dispatching fetch event in ServiceWorker.
  // If the response is not provided by the ServiceWorker, kept empty.
  // TODO(ksakamoto): Move this to net::LoadTimingInfo.
  base::TimeTicks service_worker_ready_time;

  // True when the response is served from the CacheStorage via the
  // ServiceWorker.
  bool is_in_cache_storage = false;

  // The cache name of the CacheStorage from where the response is served via
  // the ServiceWorker. Empty if the response isn't from the CacheStorage.
  std::string cache_storage_cache_name;

  // Effective connection type when the resource was fetched. This is populated
  // only for responses that correspond to main frame requests.
  net::EffectiveConnectionType effective_connection_type;

  // Bitmask of status info of the SSL certificate. See cert_status_flags.h for
  // values.
  net::CertStatus cert_status;

  // Only provided if kURLLoadOptionsSendSSLInfoWithResponse was specified to
  // the URLLoaderFactory::CreateLoaderAndStart option or
  // if ResourceRequest::report_raw_headers is set. When set via
  // |report_raw_headers|, the SSLInfo is not guaranteed to be fully populated
  // and may only contain certain fields of interest (namely, connection
  // parameters and certificate information).
  base::Optional<net::SSLInfo> ssl_info;

  // In case this is a CORS response fetched by a ServiceWorker, this is the
  // set of headers that should be exposed.
  std::vector<std::string> cors_exposed_header_names;

  // True if service worker navigation preload was performed due to the request
  // for this response.
  bool did_service_worker_navigation_preload;

  // Is used to report that a cross-origin response was blocked by Cross-Origin
  // Read Blocking (CORB) from entering renderer. Corresponding message will be
  // generated in devtools console if this flag is set to true.
  bool should_report_corb_blocking;

  // True if this resource is stale and needs async revalidation. Will only
  // possibly be set if the load_flags indicated SUPPORT_ASYNC_REVALIDATION.
  bool async_revalidation_requested;

  // True if mime sniffing has been done. In that case, we don't need to do
  // mime sniffing anymore.
  bool did_mime_sniff;

  // True if the response is an inner response of a signed exchange.
  bool is_signed_exchange_inner_response = false;

  // True if the response was intercepted by a plugin.
  bool intercepted_by_plugin = false;

  // NOTE: When adding or changing fields here, also update
  // ResourceResponse::DeepCopy in resource_response.cc.
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_RESPONSE_INFO_H_

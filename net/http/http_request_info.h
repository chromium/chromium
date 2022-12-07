// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_REQUEST_INFO_H__
#define NET_HTTP_HTTP_REQUEST_INFO_H__

#include <string>

#include "base/memory/raw_ptr.h"
#include "net/base/idempotency.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_request_headers.h"
#include "net/socket/socket_tag.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

class UploadDataStream;

struct NET_EXPORT HttpRequestInfo {
  HttpRequestInfo();
  HttpRequestInfo(const HttpRequestInfo& other);
  ~HttpRequestInfo();

  bool IsConsistent() const;

  // The requested URL.
  GURL url;

  // The method to use (GET, POST, etc.).
  std::string method;

  // This key is used to isolate requests from different contexts in accessing
  // shared cache.
  NetworkIsolationKey network_isolation_key;

  // This key is used to isolate requests from different contexts in accessing
  // shared network resources.

  // TODO @brgoldstein: populate this field from the
  // NetworkContext::PreconnectSockets path. And the HTTPCacheLookupManager
  // path.
  NetworkAnonymizationKey network_anonymization_key;

  // True if it is a subframe's document resource.
  bool is_subframe_document_resource = false;

  // Any extra request headers (including User-Agent).
  HttpRequestHeaders extra_headers;

  // Any upload data.
  raw_ptr<UploadDataStream, DanglingUntriaged> upload_data_stream = nullptr;

  // Any load flags (see load_flags.h).
  int load_flags = 0;

  // If enabled, then request must be sent over connection that cannot be
  // tracked by the server (e.g. without channel id).
  PrivacyMode privacy_mode = PRIVACY_MODE_DISABLED;

  // Secure DNS Tag for the request.
  SecureDnsPolicy secure_dns_policy = SecureDnsPolicy::kAllow;

  // Tag applied to all sockets used to service request.
  SocketTag socket_tag;

  // Network traffic annotation received from URL request.
  net::MutableNetworkTrafficAnnotationTag traffic_annotation;

  // Reporting upload nesting depth of this request.
  //
  // If the request is not a Reporting upload, the depth is 0.
  //
  // If the request is a Reporting upload, the depth is the max of the depth
  // of the requests reported within it plus 1.
  int reporting_upload_depth = 0;

  // This may the top frame origin associated with a request, or it may be the
  // top frame site.  Or it may be nullptr.  Only used for histograms.
  //
  // TODO(https://crbug.com/1136054): Investigate migrating the one consumer of
  // this to NetworkIsolationKey::TopFrameSite().  That gives more consistent
  /// behavior, and may still provide useful metrics.
  absl::optional<url::Origin> possibly_top_frame_origin;

  // Idempotency of the request, which determines that if it is safe to enable
  // 0-RTT for the request. By default, 0-RTT is only enabled for safe
  // HTTP methods, i.e., GET, HEAD, OPTIONS, and TRACE. For other methods,
  // enabling 0-RTT may cause security issues since a network observer can
  // replay the request. If the request has any side effects, those effects can
  // happen multiple times. It is only safe to enable the 0-RTT if it is known
  // that the request is idempotent.
  net::Idempotency idempotency = net::DEFAULT_IDEMPOTENCY;

  // Index of the requested URL in Cache Transparency's pervasive payload list.
  // Only used for logging purposes.
  int pervasive_payloads_index_for_logging = -1;

  // Checksum of the request body and selected headers, in upper-case
  // hexadecimal. Only non-empty if the USE_SINGLE_KEYED_CACHE load flag is set.
  std::string checksum;

  // If not null, the value is used to evaluate whether the cache entry should
  // be bypassed; if is null, that means the request site does not match the
  // filter.
  absl::optional<int64_t> fps_cache_filter;

  // Use as ID to mark the cache entry when persisting. Should be a positive
  // number once set.
  absl::optional<int64_t> browser_run_id;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_REQUEST_INFO_H__

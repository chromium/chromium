// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_REQUEST_INFO_H__
#define NET_HTTP_HTTP_REQUEST_INFO_H__

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "net/base/idempotency.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/network_isolation_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/request_priority.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_request_headers.h"
#include "net/shared_dictionary/shared_dictionary.h"
#include "net/shared_dictionary/shared_dictionary_getter.h"
#include "net/socket/socket_tag.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

class UploadDataStream;

struct NET_EXPORT HttpRequestInfo {
  HttpRequestInfo();

  HttpRequestInfo(const HttpRequestInfo& other);
  HttpRequestInfo& operator=(const HttpRequestInfo& other);
  HttpRequestInfo(HttpRequestInfo&& other);
  HttpRequestInfo& operator=(HttpRequestInfo&& other);

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
  NetworkAnonymizationKey network_anonymization_key;

  // True if it is a subframe's document resource.
  bool is_subframe_document_resource = false;

  // True if it is a main frame navigation.
  bool is_main_frame_navigation = false;

  // Any extra request headers (including User-Agent).
  HttpRequestHeaders extra_headers;

  // Any upload data.
  raw_ptr<UploadDataStream> upload_data_stream = nullptr;

  // Any load flags (see load_flags.h).
  int load_flags = 0;

  // Flag that indicates if the request should be loaded concurrently with
  // other requests of the same priority when using a protocol that supports
  // HTTP extensible priorities (RFC 9218). Currently only HTTP/3.
  bool priority_incremental = kDefaultPriorityIncremental;

  // If enabled, then request must be sent over connection that cannot be
  // tracked by the server (e.g. without channel id).
  PrivacyMode privacy_mode = PRIVACY_MODE_DISABLED;

  // Secure DNS Tag for the request.
  SecureDnsPolicy secure_dns_policy = SecureDnsPolicy::kAllow;

  // Tag applied to all sockets used to service request.
  SocketTag socket_tag;

  // Network traffic annotation received from URL request.
  MutableNetworkTrafficAnnotationTag traffic_annotation;

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
  // TODO(crbug.com/40724003): Investigate migrating the one consumer of
  // this to NetworkIsolationKey::TopFrameSite().  That gives more consistent
  /// behavior, and may still provide useful metrics.
  std::optional<url::Origin> possibly_top_frame_origin;

  // The frame origin associated with a request. This is used to isolate shared
  // dictionaries between different frame origins.
  std::optional<url::Origin> frame_origin;

  // The origin of the context which initiated this request. nullptr for
  // browser-initiated navigations. For more info, see
  // `URLRequest::initiator()`.
  std::optional<url::Origin> initiator;

  // Idempotency of the request, which determines that if it is safe to enable
  // 0-RTT for the request. By default, 0-RTT is only enabled for safe
  // HTTP methods, i.e., GET, HEAD, OPTIONS, and TRACE. For other methods,
  // enabling 0-RTT may cause security issues since a network observer can
  // replay the request. If the request has any side effects, those effects can
  // happen multiple times. It is only safe to enable the 0-RTT if it is known
  // that the request is idempotent.
  Idempotency idempotency = DEFAULT_IDEMPOTENCY;

  // If not null, the value is used to evaluate whether the cache entry should
  // be bypassed; if is null, that means the request site does not match the
  // filter.
  std::optional<int64_t> fps_cache_filter;

  // Use as ID to mark the cache entry when persisting. Should be a positive
  // number once set.
  std::optional<int64_t> browser_run_id;

  // Used to get a shared dictionary for the request. This may be null if the
  // request does not use a shared dictionary.
  SharedDictionaryGetter dictionary_getter;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_REQUEST_INFO_H__

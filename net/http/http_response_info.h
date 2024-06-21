// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_RESPONSE_INFO_H_
#define NET_HTTP_HTTP_RESPONSE_INFO_H_

#include <optional>
#include <set>
#include <string>

#include "base/time/time.h"
#include "net/base/auth.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/proxy_chain.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/alternate_protocol_usage.h"
#include "net/http/http_connection_info.h"
#include "net/http/http_vary_data.h"
#include "net/ssl/ssl_info.h"

namespace base {
class Pickle;
}

namespace net {

class HttpResponseHeaders;
class SSLCertRequestInfo;

class NET_EXPORT HttpResponseInfo {
 public:
  // Used for categorizing transactions for reporting in histograms.
  // CacheEntryStatus covers relatively common use cases being measured and
  // considered for optimization. Many use cases that are more complex or
  // uncommon are binned as OTHER, and details are not reported.
  // NOTE: This enumeration is used in histograms, so please do not add entries
  // in the middle.
  enum CacheEntryStatus {
    ENTRY_UNDEFINED,
    // Complex or uncommon case. E.g., auth (401), partial responses (206), ...
    ENTRY_OTHER,
    // The response was not in the cache. Implies !was_cached &&
    // network_accessed.
    ENTRY_NOT_IN_CACHE,
    // The response was served from the cache and no validation was needed.
    // Implies was_cached && !network_accessed.
    ENTRY_USED,
    // The response was validated and served from the cache. Implies was_cached
    // && network_accessed.
    ENTRY_VALIDATED,
    // There was a stale entry in the cache that was updated. Implies
    // !was_cached && network_accessed.
    ENTRY_UPDATED,
    // The HTTP request didn't allow a conditional request. Implies !was_cached
    // && network_accessed.
    ENTRY_CANT_CONDITIONALIZE,
    ENTRY_MAX,
  };

  HttpResponseInfo();
  HttpResponseInfo(const HttpResponseInfo& rhs);
  ~HttpResponseInfo();
  HttpResponseInfo& operator=(const HttpResponseInfo& rhs);
  // Even though we could get away with the copy ctor and default operator=,
  // that would prevent us from doing a bunch of forward declaration.

  // Initializes from the representation stored in the given pickle.
  bool InitFromPickle(const base::Pickle& pickle, bool* response_truncated);

  // Call this method to persist the response info.
  void Persist(base::Pickle* pickle,
               bool skip_transient_headers,
               bool response_truncated) const;

  // Whether QUIC is used or not.
  bool DidUseQuic() const;

  // The following is only defined if the request_time member is set.
  // If this resource was found in the cache, then this bool is set, and
  // request_time may corresponds to a time "far" in the past.  Note that
  // stale content (perhaps un-cacheable) may be fetched from cache subject to
  // the load flags specified on the request info.  For example, this is done
  // when a user presses the back button to re-render pages, or at startup,
  // when reloading previously visited pages (without going over the network).
  // Note also that under normal circumstances, was_cached is set to the correct
  // value even if the request fails.
  bool was_cached = false;

  // How this response was handled by the HTTP cache.
  CacheEntryStatus cache_entry_status = CacheEntryStatus::ENTRY_UNDEFINED;

  // True if the request accessed the network in the process of retrieving
  // data.
  bool network_accessed = false;

  // True if the request was fetched over a SPDY channel.
  bool was_fetched_via_spdy = false;

  // True if ALPN was negotiated for this request.
  bool was_alpn_negotiated = false;

  // True if the response was fetched via explicit proxying. Any type of
  // proxying may have taken place, HTTP or SOCKS. Note, we do not know if a
  // transparent proxy may have been involved.
  bool WasFetchedViaProxy() const;

  // Information about the proxy chain used to fetch this response, if any.
  ProxyChain proxy_chain;

  // Whether this request was eligible for IP Protection based on the request
  // being a match to the masked domain list, if available.
  // This field is not persisted by `Persist()` and not restored by
  // `InitFromPickle()`.
  bool was_mdl_match = false;

  // Whether the request use http proxy or server authentication.
  bool did_use_http_auth = false;

  // True if the resource was originally fetched for a prefetch and has not been
  // used since.
  bool unused_since_prefetch = false;

  // True if the response is a prefetch whose reuse is "restricted". This means
  // it can only be reused from the cache by requests that are marked as able to
  // use restricted prefetches.
  bool restricted_prefetch = false;

  // True if this resource is stale and needs async revalidation.
  // This value is not persisted by Persist(); it is only ever set when the
  // response is retrieved from the cache.
  bool async_revalidation_requested = false;

  // stale-while-revalidate, if any, will be honored until time given by
  // |stale_revalidate_timeout|. This value is latched the first time
  // stale-while-revalidate is used until the resource is revalidated.
  base::Time stale_revalidate_timeout;

  // Remote address of the socket which fetched this resource.
  //
  // NOTE: If the response was served from the cache (was_cached is true),
  // the socket address will be set to the address that the content came from
  // originally.  This is true even if the response was re-validated using a
  // different remote address, or if some of the content came from a byte-range
  // request to a different address.
  IPEndPoint remote_endpoint;

  // Protocol negotiated with the server.
  std::string alpn_negotiated_protocol;

  // The reason why Chrome uses a specific transport protocol for HTTP
  // semantics.
  AlternateProtocolUsage alternate_protocol_usage =
      AlternateProtocolUsage::ALTERNATE_PROTOCOL_USAGE_UNSPECIFIED_REASON;

  // The type of connection used for this response.
  HttpConnectionInfo connection_info = HttpConnectionInfo::kUNKNOWN;

  // The time at which the request was made that resulted in this response.
  // For cached responses, this is the last time the cache entry was validated.
  base::Time request_time;

  // The time at which the response headers were received.  For cached
  // this is the last time the cache entry was validated.
  base::Time response_time;

  // Host resolution error info.
  ResolveErrorInfo resolve_error_info;

  // If the response headers indicate a 401 or 407 failure, then this structure
  // will contain additional information about the authentication challenge.
  std::optional<AuthChallengeInfo> auth_challenge;

  // The SSL client certificate request info.
  // TODO(wtc): does this really belong in HttpResponseInfo?  I put it here
  // because it is similar to |auth_challenge|, but unlike HTTP authentication
  // challenge, client certificate request is not part of an HTTP response.
  scoped_refptr<SSLCertRequestInfo> cert_request_info;

  // The SSL connection info (if HTTPS). Note that when a response is
  // served from cache, not every field is present. See
  // HttpResponseInfo::InitFromPickle().
  SSLInfo ssl_info;

  // The parsed response headers and status line.
  scoped_refptr<HttpResponseHeaders> headers;

  // The "Vary" header data for this response.
  // Initialized and used by HttpCache::Transaction. May also be passed to an
  // auxiliary in-memory cache in the network service.
  HttpVaryData vary_data;

  // Any DNS aliases for the remote endpoint. Includes all known aliases, e.g.
  // from A, AAAA, or HTTPS, not just from the address used for the connection,
  // in no particular order.
  std::set<std::string> dns_aliases;

  // If not null, this indicates the response is stored during a certain browser
  // session. Used for filtering cache access.
  std::optional<int64_t> browser_run_id;

  // True if the response used a shared dictionary for decoding its body.
  bool did_use_shared_dictionary = false;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_RESPONSE_INFO_H_

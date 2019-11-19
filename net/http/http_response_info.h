// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_RESPONSE_INFO_H_
#define NET_HTTP_HTTP_RESPONSE_INFO_H_

#include <string>

#include "base/optional.h"
#include "base/time/time.h"
#include "net/base/auth.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"
#include "net/base/proxy_server.h"
#include "net/http/http_vary_data.h"
#include "net/ssl/ssl_info.h"

namespace base {
class Pickle;
}

namespace net {

class HttpResponseHeaders;
class IOBufferWithSize;
class SSLCertRequestInfo;

class NET_EXPORT HttpResponseInfo {
 public:
  // Describes the kind of connection used to fetch this response.
  //
  // NOTE: Please keep in sync with ConnectionInfo enum in
  // tools/metrics/histograms/enums.xml.
  // Because of that, and also because these values are persisted to
  // the cache, please make sure not to delete or reorder values.
  enum ConnectionInfo {
    CONNECTION_INFO_UNKNOWN = 0,
    CONNECTION_INFO_HTTP1_1 = 1,
    CONNECTION_INFO_DEPRECATED_SPDY2 = 2,
    CONNECTION_INFO_DEPRECATED_SPDY3 = 3,
    CONNECTION_INFO_HTTP2 = 4,  // HTTP/2.
    CONNECTION_INFO_QUIC_UNKNOWN_VERSION = 5,
    CONNECTION_INFO_DEPRECATED_HTTP2_14 = 6,  // HTTP/2 draft-14.
    CONNECTION_INFO_DEPRECATED_HTTP2_15 = 7,  // HTTP/2 draft-15.
    CONNECTION_INFO_HTTP0_9 = 8,
    CONNECTION_INFO_HTTP1_0 = 9,
    CONNECTION_INFO_QUIC_32 = 10,
    CONNECTION_INFO_QUIC_33 = 11,
    CONNECTION_INFO_QUIC_34 = 12,
    CONNECTION_INFO_QUIC_35 = 13,
    CONNECTION_INFO_QUIC_36 = 14,
    CONNECTION_INFO_QUIC_37 = 15,
    CONNECTION_INFO_QUIC_38 = 16,
    CONNECTION_INFO_QUIC_39 = 17,
    CONNECTION_INFO_QUIC_40 = 18,
    CONNECTION_INFO_QUIC_41 = 19,
    CONNECTION_INFO_QUIC_42 = 20,
    CONNECTION_INFO_QUIC_43 = 21,
    CONNECTION_INFO_QUIC_Q099 = 22,
    CONNECTION_INFO_QUIC_44 = 23,
    CONNECTION_INFO_QUIC_45 = 24,
    CONNECTION_INFO_QUIC_46 = 25,
    CONNECTION_INFO_QUIC_47 = 26,
    CONNECTION_INFO_QUIC_999 = 27,
    CONNECTION_INFO_QUIC_Q048 = 28,
    CONNECTION_INFO_QUIC_Q049 = 29,
    CONNECTION_INFO_QUIC_Q050 = 30,
    CONNECTION_INFO_QUIC_T048 = 31,
    CONNECTION_INFO_QUIC_T049 = 32,
    CONNECTION_INFO_QUIC_T050 = 33,
    CONNECTION_INFO_QUIC_T099 = 34,
    NUM_OF_CONNECTION_INFOS,
  };

  enum ConnectionInfoCoarse {
    CONNECTION_INFO_COARSE_HTTP1,  // HTTP/0.9, 1.0 and 1.1
    CONNECTION_INFO_COARSE_HTTP2,
    CONNECTION_INFO_COARSE_QUIC,
    CONNECTION_INFO_COARSE_OTHER,
  };

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

  // Returns a more coarse-grained description of the protocol used to fetch the
  // response.
  static ConnectionInfoCoarse ConnectionInfoToCoarse(ConnectionInfo info);

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
  bool was_cached;

  // How this response was handled by the HTTP cache.
  CacheEntryStatus cache_entry_status;

  // True if the request was fetched from cache rather than the network
  // because of a LOAD_FROM_CACHE_IF_OFFLINE flag when the system
  // was unable to contact the server.
  bool server_data_unavailable;

  // True if the request accessed the network in the process of retrieving
  // data.
  bool network_accessed;

  // True if the request was fetched over a SPDY channel.
  bool was_fetched_via_spdy;

  // True if ALPN was negotiated for this request.
  bool was_alpn_negotiated;

  // True if the request was fetched via an explicit proxy.  The proxy could
  // be any type of proxy, HTTP or SOCKS.  Note, we do not know if a
  // transparent proxy may have been involved. If true, |proxy_server| contains
  // the proxy server that was used.
  // TODO(tbansal): crbug.com/653354. Remove |was_fetched_via_proxy|.
  bool was_fetched_via_proxy;
  ProxyServer proxy_server;

  // Whether the request use http proxy or server authentication.
  bool did_use_http_auth;

  // True if the resource was originally fetched for a prefetch and has not been
  // used since.
  bool unused_since_prefetch;

  // True if the response is a prefetch whose reuse is "restricted". This means
  // it can only be reused from the cache by requests that are marked as able to
  // use restricted prefetches.
  bool restricted_prefetch;

  // True if this resource is stale and needs async revalidation.
  // This value is not persisted by Persist(); it is only ever set when the
  // response is retrieved from the cache.
  bool async_revalidation_requested;

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

  // The type of connection used for this response.
  ConnectionInfo connection_info;

  // The time at which the request was made that resulted in this response.
  // For cached responses, this is the last time the cache entry was validated.
  base::Time request_time;

  // The time at which the response headers were received.  For cached
  // this is the last time the cache entry was validated.
  base::Time response_time;

  // If the response headers indicate a 401 or 407 failure, then this structure
  // will contain additional information about the authentication challenge.
  base::Optional<AuthChallengeInfo> auth_challenge;

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
  HttpVaryData vary_data;

  // Any metadata associated with this resource's cached data.
  scoped_refptr<IOBufferWithSize> metadata;

  static std::string ConnectionInfoToString(ConnectionInfo connection_info);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_RESPONSE_INFO_H_

// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_H_

#include <stddef.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_states.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/dns/public/secure_dns_policy.h"
#include "net/http/http_request_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/socket/socket_tag.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_session_key.h"
#include "net/ssl/ssl_config.h"
#include "net/websockets/websocket_handshake_stream_base.h"

namespace net {

class HostMappingRules;
class HttpNetworkSession;
class HttpResponseHeaders;

class NET_EXPORT HttpStreamFactory {
 public:
  class NET_EXPORT_PRIVATE Job;
  class NET_EXPORT_PRIVATE JobController;
  class NET_EXPORT_PRIVATE JobFactory;

  enum JobType {
    // Job that will connect via HTTP/1 or HTTP/2. This may be paused for a
    // while when ALTERNATIVE or DNS_ALPN_H3 job was created.
    MAIN,
    // Job that will connect via HTTP/3 iff Chrome has received an Alt-Svc
    // header from the origin.
    ALTERNATIVE,
    // Job that will connect via HTTP/3 iff an "h3" value was found in the ALPN
    // list of an HTTPS DNS record.
    DNS_ALPN_H3,
    // Job that will preconnect. This uses HTTP/3 iff Chrome has received an
    // Alt-Svc header from the origin. Otherwise, it use HTTP/1 or HTTP/2.
    PRECONNECT,
    // Job that will preconnect via HTTP/3 iff an "h3" value was found in the
    // ALPN list of an HTTPS DNS record.
    PRECONNECT_DNS_ALPN_H3,
  };

  // This is the subset of HttpRequestInfo needed by the HttpStreamFactory
  // layer. It's separated out largely to avoid dangling pointers when jobs are
  // orphaned, though it also avoids creating multiple copies of fields that
  // aren't needed, like HttpRequestHeaders.
  //
  // See HttpRequestInfo for description of most fields.
  struct NET_EXPORT StreamRequestInfo {
    StreamRequestInfo();
    explicit StreamRequestInfo(const HttpRequestInfo& http_request_info);

    StreamRequestInfo(const StreamRequestInfo& other);
    StreamRequestInfo& operator=(const StreamRequestInfo& other);
    StreamRequestInfo(StreamRequestInfo&& other);
    StreamRequestInfo& operator=(StreamRequestInfo&& other);

    ~StreamRequestInfo();

    std::string method;
    NetworkAnonymizationKey network_anonymization_key;

    // Whether HTTP/1.x can be used. Extracted from
    // UploadDataStream::AllowHTTP1().
    bool is_http1_allowed = true;

    int load_flags = 0;
    PrivacyMode privacy_mode = PRIVACY_MODE_DISABLED;
    SecureDnsPolicy secure_dns_policy = SecureDnsPolicy::kAllow;
    SocketTag socket_tag;
  };

  // Calculates an appropriate SPDY session key for the given parameters.
  static SpdySessionKey GetSpdySessionKey(
      const ProxyChain& proxy_chain,
      const GURL& origin_url,
      const StreamRequestInfo& request_info);

  // Returns whether an appropriate SPDY session would correspond to either a
  // connection to the last proxy server in the chain (for the traditional HTTP
  // proxying behavior of sending a GET request to the proxy server) or a
  // connection through the entire proxy chain (for tunneled requests). Note
  // that for QUIC proxies we no longer support the former.
  static bool IsGetToProxy(const ProxyChain& proxy_chain,
                           const GURL& origin_url);

  explicit HttpStreamFactory(HttpNetworkSession* session);

  HttpStreamFactory(const HttpStreamFactory&) = delete;
  HttpStreamFactory& operator=(const HttpStreamFactory&) = delete;

  virtual ~HttpStreamFactory();

  void ProcessAlternativeServices(
      HttpNetworkSession* session,
      const NetworkAnonymizationKey& network_anonymization_key,
      const HttpResponseHeaders* headers,
      const url::SchemeHostPort& http_server);

  // Request a stream.
  // Will call delegate->OnStreamReady on successful completion.
  std::unique_ptr<HttpStreamRequest> RequestStream(
      const HttpRequestInfo& info,
      RequestPriority priority,
      const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
      HttpStreamRequest::Delegate* delegate,
      bool enable_ip_based_pooling,
      bool enable_alternative_services,
      const NetLogWithSource& net_log);

  // Request a WebSocket handshake stream.
  // Will call delegate->OnWebSocketHandshakeStreamReady on successful
  // completion.
  std::unique_ptr<HttpStreamRequest> RequestWebSocketHandshakeStream(
      const HttpRequestInfo& info,
      RequestPriority priority,
      const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
      HttpStreamRequest::Delegate* delegate,
      WebSocketHandshakeStreamBase::CreateHelper* create_helper,
      bool enable_ip_based_pooling,
      bool enable_alternative_services,
      const NetLogWithSource& net_log);

  // Request a BidirectionalStreamImpl.
  // Will call delegate->OnBidirectionalStreamImplReady on successful
  // completion.
  // TODO(crbug.com/40573539): This method is virtual to avoid cronet_test
  // failure on iOS that is caused by Network Thread TLS getting the wrong slot.
  virtual std::unique_ptr<HttpStreamRequest> RequestBidirectionalStreamImpl(
      const HttpRequestInfo& info,
      RequestPriority priority,
      const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
      HttpStreamRequest::Delegate* delegate,
      bool enable_ip_based_pooling,
      bool enable_alternative_services,
      const NetLogWithSource& net_log);

  // Requests that enough connections for |num_streams| be opened.
  //
  // TODO: Make this take StreamRequestInfo instead.
  void PreconnectStreams(int num_streams, HttpRequestInfo& info);

  const HostMappingRules* GetHostMappingRules() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(HttpStreamRequestTest, SetPriority);

  friend class HttpStreamFactoryPeer;

  using JobControllerSet =
      std::set<std::unique_ptr<JobController>, base::UniquePtrComparator>;

  url::SchemeHostPort RewriteHost(const url::SchemeHostPort& server);

  // Values must not be changed or reused.  Keep in sync with identically named
  // enum in histograms.xml.
  enum AlternativeServiceType {
    NO_ALTERNATIVE_SERVICE = 0,
    QUIC_SAME_DESTINATION = 1,
    QUIC_DIFFERENT_DESTINATION = 2,
    NOT_QUIC_SAME_DESTINATION = 3,
    NOT_QUIC_DIFFERENT_DESTINATION = 4,
    MAX_ALTERNATIVE_SERVICE_TYPE
  };

  std::unique_ptr<HttpStreamRequest> RequestStreamInternal(
      const HttpRequestInfo& info,
      RequestPriority priority,
      const std::vector<SSLConfig::CertAndStatus>& allowed_bad_certs,
      HttpStreamRequest::Delegate* delegate,
      WebSocketHandshakeStreamBase::CreateHelper* create_helper,
      HttpStreamRequest::StreamType stream_type,
      bool is_websocket,
      bool enable_ip_based_pooling,
      bool enable_alternative_services,
      const NetLogWithSource& net_log);

  // Called when the Preconnect completes. Used for testing.
  virtual void OnPreconnectsCompleteInternal() {}

  // Called when the JobController finishes service. Delete the JobController
  // from |job_controller_set_|.
  void OnJobControllerComplete(JobController* controller);

  const raw_ptr<HttpNetworkSession> session_;

  // Factory used by job controllers for creating jobs.
  std::unique_ptr<JobFactory> job_factory_;

  // All Requests/Preconnects are assigned with a JobController to manage
  // serving Job(s). JobController might outlive Request when Request
  // is served while there's some working Job left. JobController will be
  // deleted from |job_controller_set_| when it determines the completion of
  // its work.
  JobControllerSet job_controller_set_;
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_FACTORY_H_

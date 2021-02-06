// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_H_

#include <stddef.h>

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/containers/unique_ptr_adapters.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_states.h"
#include "net/base/net_export.h"
#include "net/base/privacy_mode.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/http/http_request_info.h"
#include "net/http/http_server_properties.h"
#include "net/http/http_stream_request.h"
#include "net/log/net_log_source.h"
#include "net/log/net_log_with_source.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_session_key.h"
#include "net/ssl/ssl_config.h"
#include "net/websockets/websocket_handshake_stream_base.h"

namespace base {
namespace trace_event {
class ProcessMemoryDump;
}
}

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
    MAIN,
    ALTERNATIVE,
    PRECONNECT,
  };

  explicit HttpStreamFactory(HttpNetworkSession* session);
  virtual ~HttpStreamFactory();

  void ProcessAlternativeServices(
      HttpNetworkSession* session,
      const net::NetworkIsolationKey& network_isolation_key,
      const HttpResponseHeaders* headers,
      const url::SchemeHostPort& http_server);

  // Request a stream.
  // Will call delegate->OnStreamReady on successful completion.
  std::unique_ptr<HttpStreamRequest> RequestStream(
      const HttpRequestInfo& info,
      RequestPriority priority,
      const SSLConfig& server_ssl_config,
      const SSLConfig& proxy_ssl_config,
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
      const SSLConfig& server_ssl_config,
      const SSLConfig& proxy_ssl_config,
      HttpStreamRequest::Delegate* delegate,
      WebSocketHandshakeStreamBase::CreateHelper* create_helper,
      bool enable_ip_based_pooling,
      bool enable_alternative_services,
      const NetLogWithSource& net_log);

  // Request a BidirectionalStreamImpl.
  // Will call delegate->OnBidirectionalStreamImplReady on successful
  // completion.
  // TODO(https://crbug.com/836823): This method is virtual to avoid cronet_test
  // failure on iOS that is caused by Network Thread TLS getting the wrong slot.
  virtual std::unique_ptr<HttpStreamRequest> RequestBidirectionalStreamImpl(
      const HttpRequestInfo& info,
      RequestPriority priority,
      const SSLConfig& server_ssl_config,
      const SSLConfig& proxy_ssl_config,
      HttpStreamRequest::Delegate* delegate,
      bool enable_ip_based_pooling,
      bool enable_alternative_services,
      const NetLogWithSource& net_log);

  // Requests that enough connections for |num_streams| be opened.
  void PreconnectStreams(int num_streams, const HttpRequestInfo& info);

  const HostMappingRules* GetHostMappingRules() const;

  // Dumps memory allocation stats. |parent_dump_absolute_name| is the name
  // used by the parent MemoryAllocatorDump in the memory dump hierarchy.
  void DumpMemoryStats(base::trace_event::ProcessMemoryDump* pmd,
                       const std::string& parent_absolute_name) const;

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
      const SSLConfig& server_ssl_config,
      const SSLConfig& proxy_ssl_config,
      HttpStreamRequest::Delegate* delegate,
      WebSocketHandshakeStreamBase::CreateHelper* create_helper,
      HttpStreamRequest::StreamType stream_type,
      bool is_websocket,
      bool enable_ip_based_pooling,
      bool enable_alternative_services,
      const NetLogWithSource& net_log);

  // Called when the Job detects that the endpoint indicated by the
  // Alternate-Protocol does not work. Lets the factory update
  // HttpAlternateProtocols with the failure and resets the SPDY session key.
  void OnBrokenAlternateProtocol(const Job*, const HostPortPair& origin);

  // Called when the Preconnect completes. Used for testing.
  virtual void OnPreconnectsCompleteInternal() {}

  // Called when the JobController finishes service. Delete the JobController
  // from |job_controller_set_|.
  void OnJobControllerComplete(JobController* controller);

  HttpNetworkSession* const session_;

  // All Requests/Preconnects are assigned with a JobController to manage
  // serving Job(s). JobController might outlive Request when Request
  // is served while there's some working Job left. JobController will be
  // deleted from |job_controller_set_| when it determines the completion of
  // its work.
  JobControllerSet job_controller_set_;

  // Factory used by job controllers for creating jobs.
  std::unique_ptr<JobFactory> job_factory_;

  DISALLOW_COPY_AND_ASSIGN(HttpStreamFactory);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_FACTORY_H_

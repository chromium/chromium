// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_STREAM_FACTORY_JOB_H_
#define NET_HTTP_HTTP_STREAM_FACTORY_JOB_H_

#include <memory>
#include <utility>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/net_export.h"
#include "net/base/proxy_server.h"
#include "net/base/request_priority.h"
#include "net/http/bidirectional_stream_impl.h"
#include "net/http/http_auth.h"
#include "net/http/http_auth_controller.h"
#include "net/http/http_request_info.h"
#include "net/http/http_stream_factory.h"
#include "net/http/http_stream_request.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/quic/quic_stream_factory.h"
#include "net/socket/client_socket_handle.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/socket/next_proto.h"
#include "net/socket/ssl_client_socket.h"
#include "net/spdy/spdy_session_key.h"
#include "net/spdy/spdy_session_pool.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

namespace test {

class HttpStreamFactoryJobPeer;

}  // namespace test

class ClientSocketHandle;
class HttpAuthController;
class HttpNetworkSession;
class HttpStream;
class SpdySessionPool;
class NetLog;
struct SSLConfig;

// An HttpStreamRequest exists for each stream which is in progress of being
// created for the HttpStreamFactory.
class HttpStreamFactory::Job
    : public SpdySessionPool::SpdySessionRequest::Delegate {
 public:
  // For jobs issued simultaneously to an HTTP/2 supported server, a delay is
  // applied to avoid unnecessary socket connection establishments.
  // https://crbug.com/718576
  static const int kHTTP2ThrottleMs = 300;

  // Delegate to report Job's status to HttpStreamRequest and HttpStreamFactory.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    virtual ~Delegate() {}

    // Invoked when |job| has an HttpStream ready.
    virtual void OnStreamReady(Job* job, const SSLConfig& used_ssl_config) = 0;

    // Invoked when |job| has a BidirectionalStream ready.
    virtual void OnBidirectionalStreamImplReady(
        Job* job,
        const SSLConfig& used_ssl_config,
        const ProxyInfo& used_proxy_info) = 0;

    // Invoked when |job| has a WebSocketHandshakeStream ready.
    virtual void OnWebSocketHandshakeStreamReady(
        Job* job,
        const SSLConfig& used_ssl_config,
        const ProxyInfo& used_proxy_info,
        std::unique_ptr<WebSocketHandshakeStreamBase> stream) = 0;

    // Invoked when |job| fails to create a stream.
    virtual void OnStreamFailed(Job* job,
                                int status,
                                const SSLConfig& used_ssl_config) = 0;

    // Invoked when |job| fails on the default network.
    virtual void OnFailedOnDefaultNetwork(Job* job) = 0;

    // Invoked when |job| has a certificate error for the HttpStreamRequest.
    virtual void OnCertificateError(Job* job,
                                    int status,
                                    const SSLConfig& used_ssl_config,
                                    const SSLInfo& ssl_info) = 0;

    // Invoked when |job| raises failure for SSL Client Auth.
    virtual void OnNeedsClientAuth(Job* job,
                                   const SSLConfig& used_ssl_config,
                                   SSLCertRequestInfo* cert_info) = 0;

    // Invoked when |job| needs proxy authentication.
    virtual void OnNeedsProxyAuth(Job* job,
                                  const HttpResponseInfo& proxy_response,
                                  const SSLConfig& used_ssl_config,
                                  const ProxyInfo& used_proxy_info,
                                  HttpAuthController* auth_controller) = 0;

    // Invoked when the |job| finishes pre-connecting sockets.
    virtual void OnPreconnectsComplete(Job* job) = 0;

    // Invoked to record connection attempts made by the socket layer to
    // HttpStreamRequest if |job| is associated with HttpStreamRequest.
    virtual void AddConnectionAttemptsToRequest(
        Job* job,
        const ConnectionAttempts& attempts) = 0;

    // Invoked when |job| finishes initiating a connection.
    virtual void OnConnectionInitialized(Job* job, int rv) = 0;

    // Return false if |job| can advance to the next state. Otherwise, |job|
    // will wait for Job::Resume() to be called before advancing.
    virtual bool ShouldWait(Job* job) = 0;

    virtual const NetLogWithSource* GetNetLog() const = 0;

    virtual WebSocketHandshakeStreamBase::CreateHelper*
    websocket_handshake_stream_create_helper() = 0;

    virtual void MaybeSetWaitTimeForMainJob(const base::TimeDelta& delay) = 0;
  };

  // Job is owned by |delegate|, hence |delegate| is valid for the lifetime of
  // the Job.
  //
  // |alternative_protocol| is the protocol required by Alternative Service, if
  // any:
  // * |alternative_protocol == kProtoUnknown| means that the Job can pool to an
  //   existing SpdySession, or bind to a idle TCP socket that might use either
  //   HTTP/1.1 or HTTP/2.
  // * |alternative_protocol == kProtoHTTP2| means that the Job can pool to an
  //   existing SpdySession, or bind to a idle TCP socket.  In the latter case,
  //   if the socket does not use HTTP/2, then the Job fails.
  // * |alternative_protocol == kProtoQUIC| means that the Job can pool to an
  //   existing QUIC connection or open a new one.
  // Note that this can be overwritten by specifying a QUIC proxy in
  // |proxy_info|, or by setting
  // HttpNetworkSession::Params::origins_to_force_quic_on.
  //
  // If |alternative_proxy_server| is a valid proxy server, then the Job will
  // use that instead of using ProxyResolutionService for proxy resolution.
  // Further, if |alternative_proxy_server| is a valid but bad proxy, then
  // fallback proxies are not used. It is illegal to call this constructor with
  // a valid |alternative_proxy_server| and an |alternate_protocol| different
  // from kProtoUnknown.
  Job(Delegate* delegate,
      JobType job_type,
      HttpNetworkSession* session,
      const HttpRequestInfo& request_info,
      RequestPriority priority,
      const ProxyInfo& proxy_info,
      const SSLConfig& server_ssl_config,
      const SSLConfig& proxy_ssl_config,
      HostPortPair destination,
      GURL origin_url,
      NextProto alternative_protocol,
      quic::ParsedQuicVersion quic_version,
      const ProxyServer& alternative_proxy_server,
      bool is_websocket,
      bool enable_ip_based_pooling,
      NetLog* net_log);
  ~Job() override;

  // Start initiates the process of creating a new HttpStream.
  // |delegate_| will be notified upon completion.
  void Start(HttpStreamRequest::StreamType stream_type);

  // Preconnect will attempt to request |num_streams| sockets from the
  // appropriate ClientSocketPool.
  int Preconnect(int num_streams);

  int RestartTunnelWithProxyAuth();
  LoadState GetLoadState() const;

  // Tells |this| that |delegate_| has determined it still needs to continue
  // connecting.
  virtual void Resume();

  // Called when |this| is orphaned by Delegate. This is valid for
  // ALTERNATIVE job only.
  void Orphan();

  void SetPriority(RequestPriority priority);

  RequestPriority priority() const { return priority_; }
  bool was_alpn_negotiated() const;
  NextProto negotiated_protocol() const;
  bool using_spdy() const;
  const NetLogWithSource& net_log() const { return net_log_; }
  HttpStreamRequest::StreamType stream_type() const { return stream_type_; }

  std::unique_ptr<HttpStream> ReleaseStream() { return std::move(stream_); }

  std::unique_ptr<BidirectionalStreamImpl> ReleaseBidirectionalStream() {
    return std::move(bidirectional_stream_impl_);
  }

  // Returns the estimated memory usage in bytes.
  size_t EstimateMemoryUsage() const;

  bool is_waiting() const { return next_state_ == STATE_WAIT_COMPLETE; }
  const SSLConfig& server_ssl_config() const;
  const SSLConfig& proxy_ssl_config() const;
  const ProxyInfo& proxy_info() const;

  JobType job_type() const { return job_type_; }

  const ProxyServer alternative_proxy_server() const {
    return alternative_proxy_server_;
  }

  bool using_existing_quic_session() const {
    return using_existing_quic_session_;
  }

  bool using_quic() const { return using_quic_; }

  bool should_reconsider_proxy() const { return should_reconsider_proxy_; }

  NetErrorDetails* net_error_details() { return &net_error_details_; }

 private:
  friend class test::HttpStreamFactoryJobPeer;

  enum State {
    STATE_START,
    // The main and alternative jobs are started in parallel.  The main job
    // can wait if it's paused. The alternative job never waits.
    //
    // An HTTP/2 alternative job notifies the JobController in DoInitConnection
    // unless it can pool to an existing SpdySession.  JobController, in turn,
    // resumes the main job.
    //
    // A QUIC alternative job notifies the JobController in DoInitConnection
    // regardless of whether it pools to an existing QUIC session, but the main
    // job is only resumed after some delay.
    //
    // If the main job is resumed, then it races the alternative job.
    STATE_WAIT,
    STATE_WAIT_COMPLETE,

    STATE_INIT_CONNECTION,
    STATE_INIT_CONNECTION_COMPLETE,
    STATE_WAITING_USER_ACTION,
    STATE_CREATE_STREAM,
    STATE_CREATE_STREAM_COMPLETE,
    STATE_DRAIN_BODY_FOR_AUTH_RESTART,
    STATE_DRAIN_BODY_FOR_AUTH_RESTART_COMPLETE,
    STATE_DONE,
    STATE_NONE,
    // Used for UMA.
    STATE_MAX,
  };

  void OnStreamReadyCallback();
  void OnBidirectionalStreamImplReadyCallback();
  void OnWebSocketHandshakeStreamReadyCallback();
  // This callback function is called when a new SPDY session is created.
  void OnNewSpdySessionReadyCallback();
  void OnStreamFailedCallback(int result);
  void OnCertificateErrorCallback(int result, const SSLInfo& ssl_info);
  void OnNeedsProxyAuthCallback(const HttpResponseInfo& response_info,
                                HttpAuthController* auth_controller,
                                base::OnceClosure restart_with_auth_callback);
  void OnNeedsClientAuthCallback(SSLCertRequestInfo* cert_info);
  void OnPreconnectsComplete();

  void OnIOComplete(int result);
  // RunLoop() finishes asynchronously and invokes one of the On* methods (see
  // above) when done.
  void RunLoop(int result);
  int DoLoop(int result);
  int StartInternal();
  int DoInitConnectionImpl();
  int DoInitConnectionImplQuic();

  // If this is a QUIC alt job, then this function is called when host
  // resolution completes. It's called with the next result after host
  // resolution, not the result of host resolution itself.
  void OnQuicHostResolution(int result);

  // Invoked when the underlying connection fails on the default network.
  void OnFailedOnDefaultNetwork(int result);

  // Each of these methods corresponds to a State value.  Those with an input
  // argument receive the result from the previous state.  If a method returns
  // ERR_IO_PENDING, then the result from OnIOComplete will be passed to the
  // next state method as the result arg.
  int DoStart();
  int DoWait();
  int DoWaitComplete(int result);
  int DoInitConnection();
  int DoInitConnectionComplete(int result);
  int DoWaitingUserAction(int result);
  int DoCreateStream();
  int DoCreateStreamComplete(int result);

  void ResumeInitConnection();

  // Creates a SpdyHttpStream or a BidirectionalStreamImpl from the given values
  // and sets to |stream_| or |bidirectional_stream_impl_| respectively. Does
  // nothing if |stream_factory_| is for WebSocket.
  int SetSpdyHttpStreamOrBidirectionalStreamImpl(
      base::WeakPtr<SpdySession> session);

  // SpdySessionPool::SpdySessionRequest::Delegate implementation:
  void OnSpdySessionAvailable(base::WeakPtr<SpdySession> spdy_session) override;

  // Retrieve SSLInfo from our SSL Socket.
  // This must only be called when we are using an SSLSocket.
  void GetSSLInfo(SSLInfo* ssl_info);

  // Called in Job constructor: should Job be forced to use QUIC.
  static bool ShouldForceQuic(HttpNetworkSession* session,
                              const HostPortPair& destination,
                              const GURL& origin_url,
                              const ProxyInfo& proxy_info,
                              bool using_ssl);

  // Called in Job constructor. Use |spdy_session_key_| after construction.
  static SpdySessionKey GetSpdySessionKey(
      bool spdy_session_direct,
      const ProxyServer& proxy_server,
      const GURL& origin_url,
      PrivacyMode privacy_mode,
      const SocketTag& socket_tag,
      const NetworkIsolationKey& network_isolation_key,
      bool disable_secure_dns);

  // Returns true if the current request can use an existing spdy session.
  bool CanUseExistingSpdySession() const;

  // Called when we encounter a network error that could be resolved by trying
  // a new proxy configuration.  If there is another proxy configuration to try
  // then this method sets next_state_ appropriately and returns either OK or
  // ERR_IO_PENDING depending on whether or not the new proxy configuration is
  // available synchronously or asynchronously.  Otherwise, the given error
  // code is simply returned.
  int ReconsiderProxyAfterError(int error);

  ClientSocketPoolManager::SocketGroupType GetSocketGroup() const;

  void MaybeCopyConnectionAttemptsFromSocketOrHandle();

  // Returns true if the request should be throttled to allow for only one
  // connection attempt to be made to an H2 server at a time.
  bool ShouldThrottleConnectForSpdy() const;

  const HttpRequestInfo request_info_;
  RequestPriority priority_;
  const ProxyInfo proxy_info_;
  SSLConfig server_ssl_config_;
  SSLConfig proxy_ssl_config_;
  const NetLogWithSource net_log_;

  const CompletionRepeatingCallback io_callback_;
  std::unique_ptr<ClientSocketHandle> connection_;
  HttpNetworkSession* const session_;

  State next_state_;

  // The server we are trying to reach, could be that of the origin or of the
  // alternative service (after applying host mapping rules).
  const HostPortPair destination_;

  // The origin url we're trying to reach. This url may be different from the
  // original request when host mapping rules are set-up.
  const GURL origin_url_;

  // Alternative proxy server that should be used by |this| to fetch the
  // request.
  const ProxyServer alternative_proxy_server_;

  // True if request is for Websocket.
  const bool is_websocket_;

  // True if WebSocket request is allowed to use a WebSocket-capable existing
  // HTTP/2 connection.  In this case FindAvailableSession() must be called with
  // |enable_websocket = true|.
  const bool try_websocket_over_http2_;

  // Enable pooling to a SpdySession with matching IP and certificate
  // even if the SpdySessionKey is different.
  const bool enable_ip_based_pooling_;

  // Unowned. |this| job is owned by |delegate_|.
  Delegate* const delegate_;

  const JobType job_type_;

  // True if handling a HTTPS request.
  const bool using_ssl_;

  // True if Job uses QUIC.
  const bool using_quic_;

  // quic::ParsedQuicVersion that should be used to connect to the QUIC
  // server if Job uses QUIC.
  quic::ParsedQuicVersion quic_version_;

  // True if Alternative Service protocol field requires that HTTP/2 is used.
  // In this case, Job fails if it cannot pool to an existing SpdySession and
  // the server does not negotiate HTTP/2 on a new socket.
  const bool expect_spdy_;

  // True if Job actually uses HTTP/2.
  bool using_spdy_;

  // True if this job might succeed with a different proxy config.
  bool should_reconsider_proxy_;

  QuicStreamRequest quic_request_;

  // Only valid for a QUIC job. Set when a QUIC connection is started. If true,
  // then OnQuicHostResolution() is expected to be called in the future.
  bool expect_on_quic_host_resolution_;

  // True if this job used an existing QUIC session.
  bool using_existing_quic_session_;

  // True when the tunnel is in the process of being established - we can't
  // read from the socket until the tunnel is done.
  bool establishing_tunnel_;

  std::unique_ptr<HttpStream> stream_;
  std::unique_ptr<WebSocketHandshakeStreamBase> websocket_stream_;
  std::unique_ptr<BidirectionalStreamImpl> bidirectional_stream_impl_;

  // True if we negotiated ALPN.
  bool was_alpn_negotiated_;

  // Protocol negotiated with the server.
  NextProto negotiated_protocol_;

  // 0 if we're not preconnecting. Otherwise, the number of streams to
  // preconnect.
  int num_streams_;

  // Initialized when we have an existing SpdySession.
  base::WeakPtr<SpdySession> existing_spdy_session_;

  // Once Job claims a pushed stream on a SpdySession, |pushed_stream_id_| is
  // the ID of the claimed stream, and |existing_spdy_session_| points to that
  // SpdySession.  Otherwise |pushed_stream_id_| is set to kNoPushedStreamFound
  // (but |existing_spdy_session_| can still be non-null).
  spdy::SpdyStreamId pushed_stream_id_;

  // True if not connecting to an Https proxy for an Http url.
  const bool spdy_session_direct_;

  const SpdySessionKey spdy_session_key_;

  // Type of stream that is requested.
  HttpStreamRequest::StreamType stream_type_;

  // Whether Job has continued to DoInitConnection().
  bool init_connection_already_resumed_;

  base::OnceClosure restart_with_auth_callback_;

  NetErrorDetails net_error_details_;

  std::unique_ptr<SpdySessionPool::SpdySessionRequest> spdy_session_request_;

  base::WeakPtrFactory<Job> ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(Job);
};

// Factory for creating Jobs.
class HttpStreamFactory::JobFactory {
 public:
  JobFactory();

  virtual ~JobFactory();

  virtual std::unique_ptr<HttpStreamFactory::Job> CreateMainJob(
      HttpStreamFactory::Job::Delegate* delegate,
      HttpStreamFactory::JobType job_type,
      HttpNetworkSession* session,
      const HttpRequestInfo& request_info,
      RequestPriority priority,
      const ProxyInfo& proxy_info,
      const SSLConfig& server_ssl_config,
      const SSLConfig& proxy_ssl_config,
      HostPortPair destination,
      GURL origin_url,
      bool is_websocket,
      bool enable_ip_based_pooling,
      NetLog* net_log);

  virtual std::unique_ptr<HttpStreamFactory::Job> CreateAltSvcJob(
      HttpStreamFactory::Job::Delegate* delegate,
      HttpStreamFactory::JobType job_type,
      HttpNetworkSession* session,
      const HttpRequestInfo& request_info,
      RequestPriority priority,
      const ProxyInfo& proxy_info,
      const SSLConfig& server_ssl_config,
      const SSLConfig& proxy_ssl_config,
      HostPortPair destination,
      GURL origin_url,
      NextProto alternative_protocol,
      quic::ParsedQuicVersion quic_version,
      bool is_websocket,
      bool enable_ip_based_pooling,
      NetLog* net_log);

  virtual std::unique_ptr<HttpStreamFactory::Job> CreateAltProxyJob(
      HttpStreamFactory::Job::Delegate* delegate,
      HttpStreamFactory::JobType job_type,
      HttpNetworkSession* session,
      const HttpRequestInfo& request_info,
      RequestPriority priority,
      const ProxyInfo& proxy_info,
      const SSLConfig& server_ssl_config,
      const SSLConfig& proxy_ssl_config,
      HostPortPair destination,
      GURL origin_url,
      const ProxyServer& alternative_proxy_server,
      bool is_websocket,
      bool enable_ip_based_pooling,
      NetLog* net_log);
};

}  // namespace net

#endif  // NET_HTTP_HTTP_STREAM_FACTORY_JOB_H_

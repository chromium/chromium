// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_CONNECT_JOB_H_
#define NET_SOCKET_CONNECT_JOB_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/load_states.h"
#include "net/base/load_timing_info.h"
#include "net/base/net_export.h"
#include "net/base/request_priority.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/http/http_server_properties.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/next_proto.h"
#include "net/socket/socket_tag.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_config.h"
#include "net/third_party/quiche/src/quiche/quic/core/quic_versions.h"

namespace net {

class ClientSocketFactory;
class HostPortPair;
class HostResolver;
struct HostResolverEndpointResult;
class HttpAuthCache;
class HttpAuthController;
class HttpAuthHandlerFactory;
class HttpResponseInfo;
class HttpUserAgentSettings;
class NetLog;
class NetLogWithSource;
class NetworkQualityEstimator;
class ProxyDelegate;
class QuicSessionPool;
class SocketPerformanceWatcherFactory;
class SocketTag;
class SpdySessionPool;
class SSLCertRequestInfo;
class StreamSocket;
class WebSocketEndpointLockManager;

// Immutable socket parameters intended for shared use by all ConnectJob types.
// Excludes priority because it can be modified over the lifetime of a
// ConnectJob. Excludes connection timeout and NetLogWithSource because
// ConnectJobs that wrap other ConnectJobs typically have different values for
// those.
struct NET_EXPORT_PRIVATE CommonConnectJobParams {
  // TODO(crbug.com/40946406): Look into passing in HttpNetworkSession
  // instead.
  CommonConnectJobParams(
      ClientSocketFactory* client_socket_factory,
      HostResolver* host_resolver,
      HttpAuthCache* http_auth_cache,
      HttpAuthHandlerFactory* http_auth_handler_factory,
      SpdySessionPool* spdy_session_pool,
      const quic::ParsedQuicVersionVector* quic_supported_versions,
      QuicSessionPool* quic_session_pool,
      ProxyDelegate* proxy_delegate,
      const HttpUserAgentSettings* http_user_agent_settings,
      SSLClientContext* ssl_client_context,
      SocketPerformanceWatcherFactory* socket_performance_watcher_factory,
      NetworkQualityEstimator* network_quality_estimator,
      NetLog* net_log,
      WebSocketEndpointLockManager* websocket_endpoint_lock_manager,
      HttpServerProperties* http_server_properties,
      const NextProtoVector* alpn_protos,
      const SSLConfig::ApplicationSettings* application_settings,
      const bool* ignore_certificate_errors,
      const bool* enable_early_data);
  CommonConnectJobParams(const CommonConnectJobParams& other);
  ~CommonConnectJobParams();

  CommonConnectJobParams& operator=(const CommonConnectJobParams& other);

  raw_ptr<ClientSocketFactory> client_socket_factory;
  raw_ptr<HostResolver> host_resolver;
  raw_ptr<HttpAuthCache> http_auth_cache;
  raw_ptr<HttpAuthHandlerFactory> http_auth_handler_factory;
  raw_ptr<SpdySessionPool> spdy_session_pool;
  raw_ptr<const quic::ParsedQuicVersionVector> quic_supported_versions;
  raw_ptr<QuicSessionPool> quic_session_pool;
  raw_ptr<ProxyDelegate> proxy_delegate;
  raw_ptr<const HttpUserAgentSettings> http_user_agent_settings;
  raw_ptr<SSLClientContext> ssl_client_context;
  raw_ptr<SocketPerformanceWatcherFactory> socket_performance_watcher_factory;
  raw_ptr<NetworkQualityEstimator> network_quality_estimator;
  raw_ptr<NetLog> net_log;

  // This must only be non-null for WebSockets.
  raw_ptr<WebSocketEndpointLockManager> websocket_endpoint_lock_manager;

  raw_ptr<HttpServerProperties> http_server_properties;

  raw_ptr<const NextProtoVector> alpn_protos;
  raw_ptr<const SSLConfig::ApplicationSettings> application_settings;
  raw_ptr<const bool> ignore_certificate_errors;
  raw_ptr<const bool> enable_early_data;
};

// When a host resolution completes, OnHostResolutionCallback() is invoked. If
// it returns |kContinue|, the ConnectJob can continue immediately. If it
// returns |kMayBeDeletedAsync|, the ConnectJob may be slated for asychronous
// destruction, so should post a task before continuing, in case it will be
// deleted. The purpose of kMayBeDeletedAsync is to avoid needlessly creating
// and connecting a socket when it might not be needed.
enum class OnHostResolutionCallbackResult {
  kContinue,
  kMayBeDeletedAsync,
};

// If non-null, invoked when host resolution completes. May not destroy the
// ConnectJob synchronously, but may signal the ConnectJob may be destroyed
// asynchronously. See OnHostResolutionCallbackResult above.
//
// `endpoint_results` is the list of endpoints the host being connected to was
// resolved to, with the port fields populated to the port being connected to.
using OnHostResolutionCallback =
    base::RepeatingCallback<OnHostResolutionCallbackResult(
        const HostPortPair& host_port_pair,
        const std::vector<HostResolverEndpointResult>& endpoint_results,
        const std::set<std::string>& aliases)>;

// ConnectJob provides an abstract interface for "connecting" a socket.
// The connection may involve host resolution, tcp connection, ssl connection,
// etc.
class NET_EXPORT_PRIVATE ConnectJob {
 public:
  // Alerts the delegate that the connection completed. |job| must be destroyed
  // by the delegate. A std::unique_ptr<> isn't used because the caller of this
  // function doesn't own |job|.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    Delegate() = default;

    Delegate(const Delegate&) = delete;
    Delegate& operator=(const Delegate&) = delete;

    virtual ~Delegate() = default;

    // Alerts the delegate that the connection completed. |job| must be
    // destroyed by the delegate. A std::unique_ptr<> isn't used because the
    // caller of this function doesn't own |job|.
    virtual void OnConnectJobComplete(int result, ConnectJob* job) = 0;

    // Invoked when an HTTP proxy returns an HTTP auth challenge during tunnel
    // establishment. Always invoked asynchronously. The caller should use
    // |auth_controller| to set challenge response information and then invoke
    // |restart_with_auth_callback| to continue establishing a connection, or
    // delete the ConnectJob if it doesn't want to respond to the challenge.
    //
    // Will only be called once at a time. Neither OnConnectJobComplete() nor
    // OnNeedsProxyAuth() will be called synchronously when
    // |restart_with_auth_callback| is invoked. Will not be called after
    // OnConnectJobComplete() has been invoked.
    virtual void OnNeedsProxyAuth(const HttpResponseInfo& response,
                                  HttpAuthController* auth_controller,
                                  base::OnceClosure restart_with_auth_callback,
                                  ConnectJob* job) = 0;
  };

  // A |timeout_duration| of 0 corresponds to no timeout.
  //
  // If |net_log| is non-NULL, the ConnectJob will use it for logging.
  // Otherwise, a new one will be created of type |net_log_source_type|.
  //
  // |net_log_connect_event_type| is the NetLog event type logged on Connect()
  // and connect completion.
  ConnectJob(RequestPriority priority,
             const SocketTag& socket_tag,
             base::TimeDelta timeout_duration,
             const CommonConnectJobParams* common_connect_job_params,
             Delegate* delegate,
             const NetLogWithSource* net_log,
             NetLogSourceType net_log_source_type,
             NetLogEventType net_log_connect_event_type);

  ConnectJob(const ConnectJob&) = delete;
  ConnectJob& operator=(const ConnectJob&) = delete;

  virtual ~ConnectJob();

  // Accessors
  const NetLogWithSource& net_log() { return net_log_; }
  RequestPriority priority() const { return priority_; }

  // Releases ownership of the underlying socket to the caller. Returns the
  // released socket, or nullptr if there was a connection error.
  std::unique_ptr<StreamSocket> PassSocket();

  // Returns the connected socket, or nullptr if PassSocket() has already been
  // called. Used to query the socket state. May only be called after the
  // ConnectJob completes.
  StreamSocket* socket() { return socket_.get(); }

  void ChangePriority(RequestPriority priority);

  // Begins connecting the socket.  Returns OK on success, ERR_IO_PENDING if it
  // cannot complete synchronously without blocking, or another net error code
  // on error.  In asynchronous completion, the ConnectJob will notify
  // |delegate_| via OnConnectJobComplete.  In both asynchronous and synchronous
  // completion, ReleaseSocket() can be called to acquire the connected socket
  // if it succeeded.
  //
  // On completion, the ConnectJob must be destroyed synchronously, since it
  // doesn't bother to stop its timer when complete.
  // TODO(mmenke): Can that be fixed?
  int Connect();

  // Returns the current LoadState of the ConnectJob. Each ConnectJob class must
  // start (optionally) with a LOAD_STATE_RESOLVING_HOST followed by
  // LOAD_STATE_CONNECTING, and never return to LOAD_STATE_CONNECTING. This
  // behavior is needed for backup ConnectJobs to function correctly.
  //
  // TODO(mmenke): Can something better be done here?
  virtual LoadState GetLoadState() const = 0;

  // Returns true if the ConnectJob has ever successfully established a TCP
  // connection. Used solely for deciding if a backup job is needed. Once it
  // starts returning true, must always return true when called in the future.
  // Not safe to call after NotifyComplete() is invoked.
  virtual bool HasEstablishedConnection() const = 0;

  // Returns a list of failed attempts to connect to the destination server.
  // Returns an empty list if connecting to a proxy.
  virtual ConnectionAttempts GetConnectionAttempts() const;

  // Returns error information about any host resolution attempt.
  virtual ResolveErrorInfo GetResolveErrorInfo() const = 0;

  // If the ConnectJob failed, returns true if the failure occurred after SSL
  // negotiation started. If the ConnectJob succeeded, the returned value is
  // undefined.
  virtual bool IsSSLError() const;

  // If the ConnectJob failed with ERR_SSL_CLIENT_AUTH_CERT_NEEDED, returns the
  // SSLCertRequestInfo received. Otherwise, returns nullptr.
  virtual scoped_refptr<SSLCertRequestInfo> GetCertRequestInfo();

  // Returns the `HostResolverEndpointResult` structure corresponding to the
  // chosen route. Should only be called on a successful connect. If the
  // `ConnectJob` does not make DNS queries, or does not use the SVCB/HTTPS
  // record, it may return `std::nullopt`, to avoid callers getting confused by
  // an empty `IPEndPoint` list.
  virtual std::optional<HostResolverEndpointResult>
  GetHostResolverEndpointResult() const;

  const LoadTimingInfo::ConnectTiming& connect_timing() const {
    return connect_timing_;
  }

  // Sets |done_closure_| which will be called when |this| is deleted.
  void set_done_closure(base::OnceClosure done_closure);

  const NetLogWithSource& net_log() const { return net_log_; }

 protected:
  const SocketTag& socket_tag() const { return socket_tag_; }
  ClientSocketFactory* client_socket_factory() {
    return common_connect_job_params_->client_socket_factory;
  }
  HostResolver* host_resolver() {
    return common_connect_job_params_->host_resolver;
  }
  const HttpUserAgentSettings* http_user_agent_settings() const {
    return common_connect_job_params_->http_user_agent_settings;
  }
  SSLClientContext* ssl_client_context() {
    return common_connect_job_params_->ssl_client_context;
  }
  SocketPerformanceWatcherFactory* socket_performance_watcher_factory() {
    return common_connect_job_params_->socket_performance_watcher_factory;
  }
  NetworkQualityEstimator* network_quality_estimator() {
    return common_connect_job_params_->network_quality_estimator;
  }
  WebSocketEndpointLockManager* websocket_endpoint_lock_manager() {
    return common_connect_job_params_->websocket_endpoint_lock_manager;
  }
  HttpServerProperties* http_server_properties() {
    return common_connect_job_params_->http_server_properties;
  }
  const CommonConnectJobParams* common_connect_job_params() const {
    return common_connect_job_params_;
  }

  void SetSocket(std::unique_ptr<StreamSocket> socket,
                 std::optional<std::set<std::string>> dns_aliases);
  void NotifyDelegateOfCompletion(int rv);
  void NotifyDelegateOfProxyAuth(const HttpResponseInfo& response,
                                 HttpAuthController* auth_controller,
                                 base::OnceClosure restart_with_auth_callback);

  // If |remaining_time| is base::TimeDelta(), stops the timeout timer, if it's
  // running. Otherwise, Starts / restarts the timeout timer to trigger in the
  // specified amount of time.
  void ResetTimer(base::TimeDelta remaining_time);

  // Returns whether or not the timeout timer is running. Only intended for use
  // by DCHECKs.
  bool TimerIsRunning() const;

  // Connection establishment timing information.
  // TODO(mmenke): This should be private.
  LoadTimingInfo::ConnectTiming connect_timing_;

 private:
  virtual int ConnectInternal() = 0;

  virtual void ChangePriorityInternal(RequestPriority priority) = 0;

  void LogConnectStart();
  void LogConnectCompletion(int net_error);

  // Alerts the delegate that the ConnectJob has timed out.
  void OnTimeout();

  // Invoked to notify subclasses that the has request timed out.
  virtual void OnTimedOutInternal();

  const base::TimeDelta timeout_duration_;
  RequestPriority priority_;
  const SocketTag socket_tag_;
  raw_ptr<const CommonConnectJobParams> common_connect_job_params_;
  // Timer to abort jobs that take too long.
  base::OneShotTimer timer_;
  raw_ptr<Delegate> delegate_;
  std::unique_ptr<StreamSocket> socket_;
  // Indicates if this is the topmost ConnectJob. The topmost ConnectJob logs an
  // extra begin and end event, to allow callers to log extra data before the
  // ConnectJob has started / after it has completed.
  const bool top_level_job_;
  NetLogWithSource net_log_;
  // This is called when |this| is deleted.
  base::ScopedClosureRunner done_closure_;
  const NetLogEventType net_log_connect_event_type_;
};

}  // namespace net

#endif  // NET_SOCKET_CONNECT_JOB_H_

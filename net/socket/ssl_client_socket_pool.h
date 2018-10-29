// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CLIENT_SOCKET_POOL_H_
#define NET_SOCKET_SSL_CLIENT_SOCKET_POOL_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/net_export.h"
#include "net/base/privacy_mode.h"
#include "net/http/http_response_info.h"
#include "net/socket/client_socket_pool.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

class CTPolicyEnforcer;
class CertVerifier;
class ClientSocketFactory;
class ConnectJobFactory;
class CTVerifier;
class HostPortPair;
class HttpProxyClientSocketPool;
class HttpProxySocketParams;
class SOCKSClientSocketPool;
class SOCKSSocketParams;
class SSLClientSocket;
class TransportClientSocketPool;
class TransportSecurityState;
class TransportSocketParams;

class NET_EXPORT_PRIVATE SSLSocketParams
    : public base::RefCounted<SSLSocketParams> {
 public:
  enum ConnectionType { DIRECT, SOCKS_PROXY, HTTP_PROXY };

  // Exactly one of |direct_params|, |socks_proxy_params|, and
  // |http_proxy_params| must be non-NULL.
  SSLSocketParams(const scoped_refptr<TransportSocketParams>& direct_params,
                  const scoped_refptr<SOCKSSocketParams>& socks_proxy_params,
                  const scoped_refptr<HttpProxySocketParams>& http_proxy_params,
                  const HostPortPair& host_and_port,
                  const SSLConfig& ssl_config,
                  PrivacyMode privacy_mode,
                  bool ignore_certificate_errors);

  // Returns the type of the underlying connection.
  ConnectionType GetConnectionType() const;

  // Must be called only when GetConnectionType() returns DIRECT.
  const scoped_refptr<TransportSocketParams>&
      GetDirectConnectionParams() const;

  // Must be called only when GetConnectionType() returns SOCKS_PROXY.
  const scoped_refptr<SOCKSSocketParams>&
      GetSocksProxyConnectionParams() const;

  // Must be called only when GetConnectionType() returns HTTP_PROXY.
  const scoped_refptr<HttpProxySocketParams>&
      GetHttpProxyConnectionParams() const;

  const HostPortPair& host_and_port() const { return host_and_port_; }
  const SSLConfig& ssl_config() const { return ssl_config_; }
  PrivacyMode privacy_mode() const { return privacy_mode_; }
  bool ignore_certificate_errors() const { return ignore_certificate_errors_; }

 private:
  friend class base::RefCounted<SSLSocketParams>;
  ~SSLSocketParams();

  const scoped_refptr<TransportSocketParams> direct_params_;
  const scoped_refptr<SOCKSSocketParams> socks_proxy_params_;
  const scoped_refptr<HttpProxySocketParams> http_proxy_params_;
  const HostPortPair host_and_port_;
  const SSLConfig ssl_config_;
  const PrivacyMode privacy_mode_;
  const bool ignore_certificate_errors_;

  DISALLOW_COPY_AND_ASSIGN(SSLSocketParams);
};

// SSLConnectJob handles the SSL handshake after setting up the underlying
// connection as specified in the params.
class SSLConnectJob : public ConnectJob {
 public:
  // Note: the SSLConnectJob does not own |messenger| so it must outlive the
  // job.
  SSLConnectJob(const std::string& group_name,
                RequestPriority priority,
                const SocketTag& socket_tag,
                ClientSocketPool::RespectLimits respect_limits,
                const scoped_refptr<SSLSocketParams>& params,
                const base::TimeDelta& timeout_duration,
                TransportClientSocketPool* transport_pool,
                SOCKSClientSocketPool* socks_pool,
                HttpProxyClientSocketPool* http_proxy_pool,
                ClientSocketFactory* client_socket_factory,
                const SSLClientSocketContext& context,
                Delegate* delegate,
                NetLog* net_log);
  ~SSLConnectJob() override;

  // ConnectJob methods.
  LoadState GetLoadState() const override;

  void GetAdditionalErrorState(ClientSocketHandle* handle) override;

 private:
  enum State {
    STATE_TRANSPORT_CONNECT,
    STATE_TRANSPORT_CONNECT_COMPLETE,
    STATE_SOCKS_CONNECT,
    STATE_SOCKS_CONNECT_COMPLETE,
    STATE_TUNNEL_CONNECT,
    STATE_TUNNEL_CONNECT_COMPLETE,
    STATE_SSL_CONNECT,
    STATE_SSL_CONNECT_COMPLETE,
    STATE_NONE,
  };

  void OnIOComplete(int result);

  // Runs the state transition loop.
  int DoLoop(int result);

  int DoTransportConnect();
  int DoTransportConnectComplete(int result);
  int DoSOCKSConnect();
  int DoSOCKSConnectComplete(int result);
  int DoTunnelConnect();
  int DoTunnelConnectComplete(int result);
  int DoSSLConnect();
  int DoSSLConnectComplete(int result);

  // Returns the initial state for the state machine based on the
  // |connection_type|.
  static State GetInitialState(SSLSocketParams::ConnectionType connection_type);

  // Starts the SSL connection process.  Returns OK on success and
  // ERR_IO_PENDING if it cannot immediately service the request.
  // Otherwise, it returns a net error code.
  int ConnectInternal() override;

  scoped_refptr<SSLSocketParams> params_;
  TransportClientSocketPool* const transport_pool_;
  SOCKSClientSocketPool* const socks_pool_;
  HttpProxyClientSocketPool* const http_proxy_pool_;
  ClientSocketFactory* const client_socket_factory_;

  const SSLClientSocketContext context_;

  State next_state_;
  CompletionRepeatingCallback callback_;
  std::unique_ptr<ClientSocketHandle> transport_socket_handle_;
  std::unique_ptr<SSLClientSocket> ssl_socket_;

  HttpResponseInfo error_response_info_;

  ConnectionAttempts connection_attempts_;
  // The address of the server the connect job is connected to. Populated if
  // and only if the connect job is connected *directly* to the server (not
  // through an HTTPS CONNECT request or a SOCKS proxy).
  IPEndPoint server_address_;

  DISALLOW_COPY_AND_ASSIGN(SSLConnectJob);
};

class NET_EXPORT_PRIVATE SSLClientSocketPool
    : public ClientSocketPool,
      public HigherLayeredPool,
      public SSLConfigService::Observer {
 public:
  typedef SSLSocketParams SocketParams;

  // Only the pools that will be used are required. i.e. if you never
  // try to create an SSL over SOCKS socket, |socks_pool| may be NULL.
  SSLClientSocketPool(int max_sockets,
                      int max_sockets_per_group,
                      CertVerifier* cert_verifier,
                      ChannelIDService* channel_id_service,
                      TransportSecurityState* transport_security_state,
                      CTVerifier* cert_transparency_verifier,
                      CTPolicyEnforcer* ct_policy_enforcer,
                      const std::string& ssl_session_cache_shard,
                      ClientSocketFactory* client_socket_factory,
                      TransportClientSocketPool* transport_pool,
                      SOCKSClientSocketPool* socks_pool,
                      HttpProxyClientSocketPool* http_proxy_pool,
                      SSLConfigService* ssl_config_service,
                      NetLog* net_log);

  ~SSLClientSocketPool() override;

  // ClientSocketPool implementation.
  int RequestSocket(const std::string& group_name,
                    const void* connect_params,
                    RequestPriority priority,
                    const SocketTag& socket_tag,
                    RespectLimits respect_limits,
                    ClientSocketHandle* handle,
                    CompletionOnceCallback callback,
                    const NetLogWithSource& net_log) override;

  void RequestSockets(const std::string& group_name,
                      const void* params,
                      int num_sockets,
                      const NetLogWithSource& net_log) override;

  void SetPriority(const std::string& group_name,
                   ClientSocketHandle* handle,
                   RequestPriority priority) override;

  void CancelRequest(const std::string& group_name,
                     ClientSocketHandle* handle) override;

  void ReleaseSocket(const std::string& group_name,
                     std::unique_ptr<StreamSocket> socket,
                     int id) override;

  void FlushWithError(int error) override;

  void CloseIdleSockets() override;

  void CloseIdleSocketsInGroup(const std::string& group_name) override;

  int IdleSocketCount() const override;

  int IdleSocketCountInGroup(const std::string& group_name) const override;

  LoadState GetLoadState(const std::string& group_name,
                         const ClientSocketHandle* handle) const override;

  // Dumps memory allocation stats. |parent_dump_absolute_name| is the name
  // used by the parent MemoryAllocatorDump in the memory dump hierarchy.
  void DumpMemoryStats(base::trace_event::ProcessMemoryDump* pmd,
                       const std::string& parent_dump_absolute_name) const;

  std::unique_ptr<base::DictionaryValue> GetInfoAsValue(
      const std::string& name,
      const std::string& type,
      bool include_nested_pools) const override;

  base::TimeDelta ConnectionTimeout() const override;

  // LowerLayeredPool implementation.
  bool IsStalled() const override;

  void AddHigherLayeredPool(HigherLayeredPool* higher_pool) override;

  void RemoveHigherLayeredPool(HigherLayeredPool* higher_pool) override;

  // HigherLayeredPool implementation.
  bool CloseOneIdleConnection() override;

 private:
  typedef ClientSocketPoolBase<SSLSocketParams> PoolBase;

  // SSLConfigService::Observer implementation.

  // When the user changes the SSL config, we flush all idle sockets so they
  // won't get re-used.
  void OnSSLConfigChanged() override;

  class SSLConnectJobFactory : public PoolBase::ConnectJobFactory {
   public:
    SSLConnectJobFactory(
        TransportClientSocketPool* transport_pool,
        SOCKSClientSocketPool* socks_pool,
        HttpProxyClientSocketPool* http_proxy_pool,
        ClientSocketFactory* client_socket_factory,
        const SSLClientSocketContext& context,
        NetLog* net_log);

    ~SSLConnectJobFactory() override;

    // ClientSocketPoolBase::ConnectJobFactory methods.
    std::unique_ptr<ConnectJob> NewConnectJob(
        const std::string& group_name,
        const PoolBase::Request& request,
        ConnectJob::Delegate* delegate) const override;

    base::TimeDelta ConnectionTimeout() const override;

   private:
    TransportClientSocketPool* const transport_pool_;
    SOCKSClientSocketPool* const socks_pool_;
    HttpProxyClientSocketPool* const http_proxy_pool_;
    ClientSocketFactory* const client_socket_factory_;
    const SSLClientSocketContext context_;
    base::TimeDelta timeout_;
    NetLog* net_log_;

    DISALLOW_COPY_AND_ASSIGN(SSLConnectJobFactory);
  };

  TransportClientSocketPool* const transport_pool_;
  SOCKSClientSocketPool* const socks_pool_;
  HttpProxyClientSocketPool* const http_proxy_pool_;
  PoolBase base_;
  SSLConfigService* const ssl_config_service_;

  DISALLOW_COPY_AND_ASSIGN(SSLClientSocketPool);
};

}  // namespace net

#endif  // NET_SOCKET_SSL_CLIENT_SOCKET_POOL_H_

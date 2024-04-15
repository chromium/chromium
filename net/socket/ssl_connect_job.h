// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SSL_CONNECT_JOB_H_
#define NET_SOCKET_SSL_CONNECT_JOB_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/completion_repeating_callback.h"
#include "net/base/net_export.h"
#include "net/base/network_anonymization_key.h"
#include "net/dns/public/host_resolver_results.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/socket/connect_job.h"
#include "net/socket/connect_job_params.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/ssl_client_socket.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_config_service.h"

namespace net {

class HostPortPair;
class HttpProxySocketParams;
class SocketTag;
class SOCKSSocketParams;
class TransportSocketParams;

class NET_EXPORT_PRIVATE SSLSocketParams
    : public base::RefCounted<SSLSocketParams> {
 public:
  enum ConnectionType { DIRECT, SOCKS_PROXY, HTTP_PROXY };

  // Exactly one of |direct_params|, |socks_proxy_params|, and
  // |http_proxy_params| must be non-NULL.
  SSLSocketParams(ConnectJobParams params,
                  const HostPortPair& host_and_port,
                  const SSLConfig& ssl_config,
                  NetworkAnonymizationKey network_anonymization_key);

  SSLSocketParams(const SSLSocketParams&) = delete;
  SSLSocketParams& operator=(const SSLSocketParams&) = delete;

  // Returns the type of the underlying connection.
  ConnectionType GetConnectionType() const;

  // Must be called only when GetConnectionType() returns DIRECT.
  const scoped_refptr<TransportSocketParams>& GetDirectConnectionParams()
      const {
    return nested_params_.transport();
  }

  // Must be called only when GetConnectionType() returns SOCKS_PROXY.
  const scoped_refptr<SOCKSSocketParams>& GetSocksProxyConnectionParams()
      const {
    return nested_params_.socks();
  }

  // Must be called only when GetConnectionType() returns HTTP_PROXY.
  const scoped_refptr<HttpProxySocketParams>& GetHttpProxyConnectionParams()
      const {
    return nested_params_.http_proxy();
  }

  const HostPortPair& host_and_port() const { return host_and_port_; }
  const SSLConfig& ssl_config() const { return ssl_config_; }
  const NetworkAnonymizationKey& network_anonymization_key() const {
    return network_anonymization_key_;
  }

 private:
  friend class base::RefCounted<SSLSocketParams>;
  ~SSLSocketParams();

  const ConnectJobParams nested_params_;
  const HostPortPair host_and_port_;
  const SSLConfig ssl_config_;
  const NetworkAnonymizationKey network_anonymization_key_;
};

// SSLConnectJob establishes a connection, through a proxy if needed, and then
// handles the SSL handshake. It returns an SSLClientSocket on success.
class NET_EXPORT_PRIVATE SSLConnectJob : public ConnectJob,
                                         public ConnectJob::Delegate {
 public:
  class NET_EXPORT_PRIVATE Factory {
   public:
    Factory() = default;
    virtual ~Factory() = default;

    virtual std::unique_ptr<SSLConnectJob> Create(
        RequestPriority priority,
        const SocketTag& socket_tag,
        const CommonConnectJobParams* common_connect_job_params,
        scoped_refptr<SSLSocketParams> params,
        ConnectJob::Delegate* delegate,
        const NetLogWithSource* net_log);
  };

  SSLConnectJob(RequestPriority priority,
                const SocketTag& socket_tag,
                const CommonConnectJobParams* common_connect_job_params,
                scoped_refptr<SSLSocketParams> params,
                ConnectJob::Delegate* delegate,
                const NetLogWithSource* net_log);

  SSLConnectJob(const SSLConnectJob&) = delete;
  SSLConnectJob& operator=(const SSLConnectJob&) = delete;

  ~SSLConnectJob() override;

  // ConnectJob methods.
  LoadState GetLoadState() const override;
  bool HasEstablishedConnection() const override;

  // ConnectJob::Delegate methods.
  void OnConnectJobComplete(int result, ConnectJob* job) override;
  void OnNeedsProxyAuth(const HttpResponseInfo& response,
                        HttpAuthController* auth_controller,
                        base::OnceClosure restart_with_auth_callback,
                        ConnectJob* job) override;
  ConnectionAttempts GetConnectionAttempts() const override;
  ResolveErrorInfo GetResolveErrorInfo() const override;
  bool IsSSLError() const override;
  scoped_refptr<SSLCertRequestInfo> GetCertRequestInfo() override;

  // Returns the timeout for the SSL handshake. This is the same for all
  // connections regardless of whether or not there is a proxy in use.
  static base::TimeDelta HandshakeTimeoutForTesting();

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

  void ResetStateForRestart();

  void ChangePriorityInternal(RequestPriority priority) override;

  scoped_refptr<SSLSocketParams> params_;

  State next_state_;
  CompletionRepeatingCallback callback_;
  std::unique_ptr<ConnectJob> nested_connect_job_;
  std::unique_ptr<StreamSocket> nested_socket_;
  std::unique_ptr<SSLClientSocket> ssl_socket_;

  // True once SSL negotiation has started.
  bool ssl_negotiation_started_ = false;

  // True if legacy crypto should be disabled for the job's current connection
  // attempt. On error, the connection will be retried with legacy crypto
  // enabled.
  bool disable_legacy_crypto_with_fallback_ = true;

  scoped_refptr<SSLCertRequestInfo> ssl_cert_request_info_;

  ConnectionAttempts connection_attempts_;
  ResolveErrorInfo resolve_error_info_;
  // The address of the server the connect job is connected to. Populated if
  // and only if the connect job is connected *directly* to the server (not
  // through an HTTPS CONNECT request or a SOCKS proxy).
  IPEndPoint server_address_;

  // Any DNS aliases for the remote endpoint. Includes all known aliases, e.g.
  // from A, AAAA, or HTTPS, not just from the address used for the connection,
  // in no particular order. Stored because `nested_connect_job_` has a limited
  // lifetime and the aliases can no longer be retrieved from there by by the
  // time that the aliases are needed to be passed in SetSocket.
  std::set<std::string> dns_aliases_;

  // The endpoint result used by `nested_connect_job_`. Stored because
  // `nested_connect_job_` has a limited lifetime.
  std::optional<HostResolverEndpointResult> endpoint_result_;

  // If not `std::nullopt`, the ECH retry configs to use in the ECH recovery
  // flow. `endpoint_result_` will then contain the endpoint to reconnect to.
  std::optional<std::vector<uint8_t>> ech_retry_configs_;
};

}  // namespace net

#endif  // NET_SOCKET_SSL_CONNECT_JOB_H_

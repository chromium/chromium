// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKS_CONNECT_JOB_H_
#define NET_SOCKET_SOCKS_CONNECT_JOB_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"
#include "net/base/completion_once_callback.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_export.h"
#include "net/base/network_isolation_key.h"
#include "net/base/request_priority.h"
#include "net/dns/public/resolve_error_info.h"
#include "net/socket/connect_job.h"
#include "net/socket/socks_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class SocketTag;
class StreamSocket;
class TransportSocketParams;

class NET_EXPORT_PRIVATE SOCKSSocketParams
    : public base::RefCounted<SOCKSSocketParams> {
 public:
  SOCKSSocketParams(scoped_refptr<TransportSocketParams> proxy_server_params,
                    bool socks_v5,
                    const HostPortPair& host_port_pair,
                    const NetworkIsolationKey& network_isolation_key,
                    const NetworkTrafficAnnotationTag& traffic_annotation);

  const scoped_refptr<TransportSocketParams>& transport_params() const {
    return transport_params_;
  }
  const HostPortPair& destination() const { return destination_; }
  bool is_socks_v5() const { return socks_v5_; }
  const NetworkIsolationKey& network_isolation_key() {
    return network_isolation_key_;
  }

  const NetworkTrafficAnnotationTag traffic_annotation() {
    return traffic_annotation_;
  }

 private:
  friend class base::RefCounted<SOCKSSocketParams>;
  ~SOCKSSocketParams();

  // The transport (likely TCP) connection must point toward the proxy server.
  const scoped_refptr<TransportSocketParams> transport_params_;
  // This is the HTTP destination.
  const HostPortPair destination_;
  const bool socks_v5_;
  const NetworkIsolationKey network_isolation_key_;

  NetworkTrafficAnnotationTag traffic_annotation_;

  DISALLOW_COPY_AND_ASSIGN(SOCKSSocketParams);
};

// SOCKSConnectJob handles establishing a connection to a SOCKS4 or SOCKS5 proxy
// and then sending a handshake to establish a tunnel.
class NET_EXPORT_PRIVATE SOCKSConnectJob : public ConnectJob,
                                           public ConnectJob::Delegate {
 public:
  SOCKSConnectJob(RequestPriority priority,
                  const SocketTag& socket_tag,
                  const CommonConnectJobParams* common_connect_job_params,
                  scoped_refptr<SOCKSSocketParams> socks_params,
                  ConnectJob::Delegate* delegate,
                  const NetLogWithSource* net_log);
  ~SOCKSConnectJob() override;

  // ConnectJob methods.
  LoadState GetLoadState() const override;
  bool HasEstablishedConnection() const override;
  ResolveErrorInfo GetResolveErrorInfo() const override;

  // Returns the handshake timeout used by SOCKSConnectJobs.
  static base::TimeDelta HandshakeTimeoutForTesting();

 private:
  enum State {
    STATE_TRANSPORT_CONNECT,
    STATE_TRANSPORT_CONNECT_COMPLETE,
    STATE_SOCKS_CONNECT,
    STATE_SOCKS_CONNECT_COMPLETE,
    STATE_NONE,
  };

  void OnIOComplete(int result);

  // ConnectJob::Delegate methods.
  void OnConnectJobComplete(int result, ConnectJob* job) override;
  void OnNeedsProxyAuth(const HttpResponseInfo& response,
                        HttpAuthController* auth_controller,
                        base::OnceClosure restart_with_auth_callback,
                        ConnectJob* job) override;

  // Runs the state transition loop.
  int DoLoop(int result);

  int DoTransportConnect();
  int DoTransportConnectComplete(int result);
  int DoSOCKSConnect();
  int DoSOCKSConnectComplete(int result);

  // Begins the transport connection and the SOCKS handshake.  Returns OK on
  // success and ERR_IO_PENDING if it cannot immediately service the request.
  // Otherwise, it returns a net error code.
  int ConnectInternal() override;

  void ChangePriorityInternal(RequestPriority priority) override;

  scoped_refptr<SOCKSSocketParams> socks_params_;

  State next_state_;
  std::unique_ptr<ConnectJob> transport_connect_job_;
  std::unique_ptr<StreamSocket> socket_;
  SOCKSClientSocket* socks_socket_ptr_;

  ResolveErrorInfo resolve_error_info_;

  DISALLOW_COPY_AND_ASSIGN(SOCKSConnectJob);
};

}  // namespace net

#endif  // NET_SOCKET_SOCKS_CONNECT_JOB_H_

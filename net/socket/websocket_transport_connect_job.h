// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_WEBSOCKET_TRANSPORT_CONNECT_JOB_H_
#define NET_SOCKET_WEBSOCKET_TRANSPORT_CONNECT_JOB_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/net_export.h"
#include "net/dns/host_resolver.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/transport_connect_job.h"

namespace net {

class SocketTag;
class WebSocketTransportConnectSubJob;

// WebSocketTransportConnectJob handles the host resolution necessary for socket
// creation and the TCP connect. WebSocketTransportConnectJob also has fallback
// logic for IPv6 connect() timeouts (which may happen due to networks / routers
// with broken IPv6 support). Those timeouts take 20s, so rather than make the
// user wait 20s for the timeout to fire, we use a fallback timer
// (kIPv6FallbackTimerInMs) and start a connect() to an IPv4 address if the
// timer fires. Then we race the IPv4 connect(s) against the IPv6 connect(s) and
// use the socket that completes successfully first or fails last.
//
// TODO(mmenke): Look into merging this with TransportConnectJob. That would
// bring all the features supported by TransportConnectJob to WebSockets:
// Happy eyeballs, socket tagging, error reporting (Used by network error
// logging), and provide performance information to SocketPerformanceWatcher.
class NET_EXPORT_PRIVATE WebSocketTransportConnectJob : public ConnectJob {
 public:
  WebSocketTransportConnectJob(
      RequestPriority priority,
      const SocketTag& socket_tag,
      const CommonConnectJobParams* common_connect_job_params,
      const scoped_refptr<TransportSocketParams>& params,
      Delegate* delegate,
      const NetLogWithSource* net_log);
  ~WebSocketTransportConnectJob() override;

  // ConnectJob methods.
  LoadState GetLoadState() const override;
  bool HasEstablishedConnection() const override;

 private:
  friend class WebSocketTransportConnectSubJob;

  enum State {
    STATE_RESOLVE_HOST,
    STATE_RESOLVE_HOST_COMPLETE,
    STATE_TRANSPORT_CONNECT,
    STATE_TRANSPORT_CONNECT_COMPLETE,
    STATE_NONE,
  };

  // Although it is not strictly necessary, it makes the code simpler if each
  // subjob knows what type it is.
  enum SubJobType { SUB_JOB_IPV4, SUB_JOB_IPV6 };

  void OnIOComplete(int result);
  int DoLoop(int result);

  int DoResolveHost();
  int DoResolveHostComplete(int result);
  int DoTransportConnect();
  int DoTransportConnectComplete(int result);

  // Called back from a SubJob when it completes.
  void OnSubJobComplete(int result, WebSocketTransportConnectSubJob* job);

  // Called from |fallback_timer_|.
  void StartIPv4JobAsync();

  // Begins the host resolution and the TCP connect.  Returns OK on success
  // and ERR_IO_PENDING if it cannot immediately service the request.
  // Otherwise, it returns a net error code.
  int ConnectInternal() override;

  void ChangePriorityInternal(RequestPriority priority) override;

  scoped_refptr<TransportSocketParams> params_;
  std::unique_ptr<HostResolver::ResolveHostRequest> request_;

  State next_state_;

  // The addresses are divided into IPv4 and IPv6, which are performed partially
  // in parallel. If the list of IPv6 addresses is non-empty, then the IPv6 jobs
  // go first, followed after |kIPv6FallbackTimerInMs| by the IPv4
  // addresses. First sub-job to establish a connection wins.
  std::unique_ptr<WebSocketTransportConnectSubJob> ipv4_job_;
  std::unique_ptr<WebSocketTransportConnectSubJob> ipv6_job_;

  base::OneShotTimer fallback_timer_;
  TransportConnectJob::RaceResult race_result_;

  bool had_ipv4_;
  bool had_ipv6_;

  base::WeakPtrFactory<WebSocketTransportConnectJob> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebSocketTransportConnectJob);
};

}  // namespace net

#endif  // NET_SOCKET_WEBSOCKET_TRANSPORT_CONNECT_JOB_H_

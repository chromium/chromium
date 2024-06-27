// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_stream_attempt.h"

#include <memory>

#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"
#include "net/socket/transport_client_socket.h"

namespace net {

TcpStreamAttempt::TcpStreamAttempt(const StreamAttemptParams* params,
                                   IPEndPoint ip_endpoint)
    : StreamAttempt(params,
                    ip_endpoint,
                    NetLogSourceType::TCP_STREAM_ATTEMPT,
                    NetLogEventType::TCP_STREAM_ATTEMPT_ALIVE) {}

TcpStreamAttempt::~TcpStreamAttempt() = default;

int TcpStreamAttempt::StartInternal() {
  std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher;
  if (params().socket_performance_watcher_factory) {
    socket_performance_watcher =
        params()
            .socket_performance_watcher_factory->CreateSocketPerformanceWatcher(
                SocketPerformanceWatcherFactory::PROTOCOL_TCP,
                ip_endpoint().address());
  }

  std::unique_ptr<TransportClientSocket> stream_socket =
      params().client_socket_factory->CreateTransportClientSocket(
          AddressList(ip_endpoint()), std::move(socket_performance_watcher),
          params().network_quality_estimator, net_log().net_log(),
          net_log().source());

  TransportClientSocket* socket_ptr = stream_socket.get();
  SetStreamSocket(std::move(stream_socket));
  return socket_ptr->Connect(
      base::BindOnce(&TcpStreamAttempt::OnIOComplete, base::Unretained(this)));
}

void TcpStreamAttempt::OnIOComplete(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  NotifyOfCompletion(rv);
}

}  // namespace net

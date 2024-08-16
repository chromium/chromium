// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_stream_attempt.h"

#include <memory>

#include "base/time/time.h"
#include "base/timer/timer.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"
#include "net/socket/transport_client_socket.h"

namespace net {

TcpStreamAttempt::TcpStreamAttempt(const StreamAttemptParams* params,
                                   IPEndPoint ip_endpoint,
                                   const NetLogWithSource* net_log)
    : StreamAttempt(params,
                    ip_endpoint,
                    NetLogSourceType::TCP_STREAM_ATTEMPT,
                    NetLogEventType::TCP_STREAM_ATTEMPT_ALIVE,
                    net_log) {}

TcpStreamAttempt::~TcpStreamAttempt() = default;

LoadState TcpStreamAttempt::GetLoadState() const {
  switch (next_state_) {
    case State::kNone:
      return LOAD_STATE_IDLE;
    case State::kConnecting:
      return LOAD_STATE_CONNECTING;
  }
}

int TcpStreamAttempt::StartInternal() {
  next_state_ = State::kConnecting;

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

  mutable_connect_timing().connect_start = base::TimeTicks::Now();
  CHECK(!timeout_timer_.IsRunning());
  timeout_timer_.Start(
      FROM_HERE, kTcpHandshakeTimeout,
      base::BindOnce(&TcpStreamAttempt::OnTimeout, base::Unretained(this)));

  int rv = socket_ptr->Connect(
      base::BindOnce(&TcpStreamAttempt::OnIOComplete, base::Unretained(this)));
  if (rv != ERR_IO_PENDING) {
    HandleCompletion();
  }
  return rv;
}

base::Value::Dict TcpStreamAttempt::GetNetLogStartParams() {
  base::Value::Dict dict;
  dict.Set("ip_endpoint", ip_endpoint().ToString());
  return dict;
}

void TcpStreamAttempt::HandleCompletion() {
  next_state_ = State::kNone;
  timeout_timer_.Stop();
  mutable_connect_timing().connect_end = base::TimeTicks::Now();
}

void TcpStreamAttempt::OnIOComplete(int rv) {
  CHECK_NE(rv, ERR_IO_PENDING);
  HandleCompletion();
  NotifyOfCompletion(rv);
}

void TcpStreamAttempt::OnTimeout() {
  SetStreamSocket(nullptr);
  // TODO(bashi): The error code should be ERR_CONNECTION_TIMED_OUT but use
  // ERR_TIMED_OUT for consistency with ConnectJobs.
  OnIOComplete(ERR_TIMED_OUT);
}

}  // namespace net

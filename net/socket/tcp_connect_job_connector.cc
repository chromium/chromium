// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_connect_job_connector.h"

#include <memory>
#include <optional>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"

namespace net {

TcpConnectJob::Connector::Connector(TcpConnectJob* parent) : parent_(*parent) {}

TcpConnectJob::Connector::~Connector() = default;

int TcpConnectJob::Connector::OnEndpointDataAvailable() {
  DCHECK(!is_done());

  if (next_state_ == State::kWaitForIPEndPoint) {
    next_state_ = State::kObtainIPEndPoint;
  } else if (next_state_ == State::kWaitForCryptoReady) {
    next_state_ = State::kVerifyIPEndPointUsable;
  } else {
    // Not waiting on DNS request, but busy, so return ERR_IO_PENDING;
    return ERR_IO_PENDING;
  }

  return DoLoop(OK);
}

LoadState TcpConnectJob::Connector::GetLoadState() const {
  switch (next_state_) {
    case State::kWaitForIPEndPoint:
    case State::kObtainIPEndPoint:
      // The next two states come after after some connection states, but does
      // technically mean we're back to waiting on DNS again.
    case State::kWaitForCryptoReady:
    case State::kVerifyIPEndPointUsable:
      return LOAD_STATE_RESOLVING_HOST;
    case State::kTcpConnect:
    case State::kTcpConnectComplete:
    case State::kDone:
      return LOAD_STATE_CONNECTING;
    case State::kNone:
      // This shouldn't happen.
      NOTREACHED();
  }
  NOTREACHED();
}

std::unique_ptr<StreamSocket> TcpConnectJob::Connector::PassSocket() {
  DCHECK_EQ(next_state_, State::kDone);
  DCHECK(transport_socket_);
  return std::move(transport_socket_);
}

const IPEndPoint& TcpConnectJob::Connector::CurrentAddress() const {
  DCHECK(current_address_);
  return *current_address_;
}

void TcpConnectJob::Connector::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    parent_->OnConnectorComplete(rv);
    // `this` may be deleted here.
  }
}

int TcpConnectJob::Connector::DoLoop(int result) {
  int rv = result;
  do {
    DCHECK_NE(rv, ERR_IO_PENDING);
    DCHECK_NE(next_state_, State::kNone);
    DCHECK_NE(next_state_, State::kDone);

    State state = next_state_;
    next_state_ = State::kNone;
    switch (state) {
      case State::kObtainIPEndPoint:
        DCHECK_EQ(rv, OK);
        rv = DoObtainIPEndPoint();
        break;
      case State::kTcpConnect:
        DCHECK_EQ(rv, OK);
        rv = DoTcpConnect();
        break;
      case State::kTcpConnectComplete:
        rv = DoTcpConnectComplete(rv);
        break;
      case State::kVerifyIPEndPointUsable:
        DCHECK_EQ(rv, OK);
        rv = DoVerifyIPEndPointUsable();
        break;
      default:
        NOTREACHED();
    }
  } while (rv != ERR_IO_PENDING && next_state_ != State::kDone);

  DCHECK_NE(next_state_, State::kNone);
  return rv;
}

int TcpConnectJob::Connector::DoObtainIPEndPoint() {
  DCHECK(!current_address_);

  TcpConnectJob::IPEndPointInfo endpoint_info = parent_->GetNextIPEndPoint();

  if (!endpoint_info.has_value()) {
    if (endpoint_info.error() == ERR_IO_PENDING) {
      // No IPEndPoints available, but the DNS request is not done yet. Need to
      // wait for more data.
      next_state_ = State::kWaitForIPEndPoint;
      return ERR_IO_PENDING;
    }

    // We failed to connect to any IP, and no more are coming.
    next_state_ = State::kDone;

    // The parent class handles DNS errors itself, so this currently can only be
    // ERR_IO_PENDING or ERR_NAME_NOT_RESOLVED. ERR_NAME_NOT_RESOLVED means no
    // more IPs are incoming. Return the last connection error, if available, or
    // ERR_NAME_NOT_RESOLVED / the error from GetNextIPEndPoint(), otherwise.
    CHECK_EQ(endpoint_info.error(), ERR_NAME_NOT_RESOLVED);
    return last_error_.value_or(endpoint_info.error());
  }

  current_address_ = std::move(endpoint_info).value();

  // Get lock if needed.
  next_state_ = State::kTcpConnect;
  return OK;
}

int TcpConnectJob::Connector::DoTcpConnect() {
  DCHECK(current_address_);

  next_state_ = State::kTcpConnectComplete;
  AddressList one_address(*current_address_);

  // Create a `SocketPerformanceWatcher`, and pass the ownership.
  std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher;
  if (auto* factory = parent_->socket_performance_watcher_factory();
      factory != nullptr) {
    socket_performance_watcher = factory->CreateSocketPerformanceWatcher(
        SocketPerformanceWatcherFactory::PROTOCOL_TCP,
        current_address_->address());
  }

  const NetLogWithSource& net_log = parent_->net_log();
  transport_socket_ =
      parent_->client_socket_factory()->CreateTransportClientSocket(
          one_address, std::move(socket_performance_watcher),
          parent_->network_quality_estimator(), net_log.net_log(),
          net_log.source());

  transport_socket_->ApplySocketTag(parent_->socket_tag());

  // TODO(https://crbug.com/484073410): Wire up websocket lock here, if needed.

  return transport_socket_->Connect(base::BindOnce(
      &TcpConnectJob::Connector::OnIOComplete, base::Unretained(this)));
}

int TcpConnectJob::Connector::DoTcpConnectComplete(int result) {
  // The connection attempt failed, no need to wait for crypto ready before
  // trying the next IP, if appropriate.
  if (result != OK) {
    return OnEndpointFailed(result);
  }

  // The connection attempt succeeded, so can set this to true. This will
  // prevent any new backup ConnectJobs from being made.
  parent_->has_established_connection_ = true;

  next_state_ = State::kVerifyIPEndPointUsable;
  return OK;
}

int TcpConnectJob::Connector::DoVerifyIPEndPointUsable() {
  // If still not crypto ready, need to wait before can do final verification of
  // the destination address we're connected to.
  if (!parent_->EndpointsCryptoReady()) {
    next_state_ = State::kWaitForCryptoReady;
    return ERR_IO_PENDING;
  }

  // If the address is not usable, treat it as a failure, and let
  // DoConnectAndVerifyComplete() handle the error.
  //
  // We could more proactively probe for this situation when we receive DNS
  // results and notice they're now cypto ready, to avoid waiting for connect
  // complete before checking again, and weren't before. Unclear if this case
  // is common enough to be worth the more complicated state transitions,
  // particularly in the WebSocket case.
  if (!parent_->IsIPEndPointUsable(CurrentAddress())) {
    return OnEndpointFailed(ERR_NAME_NOT_RESOLVED);
  }

  next_state_ = State::kDone;
  return OK;
}

int TcpConnectJob::Connector::OnEndpointFailed(int error) {
  DCHECK_NE(error, OK);
  DCHECK_NE(error, ERR_IO_PENDING);
  DCHECK(current_address_);

  // If there's only one connector, this makes the next attempt prefer the
  // address family other than that of the request that just failed.
  parent_->prefer_ipv6_ =
      (current_address_->GetFamily() != ADDRESS_FAMILY_IPV6);

  // Drop the socket to release the endpoint lock, if there is one.
  transport_socket_.reset();
  current_address_.reset();

  // Don't try the next address if entering suspend mode.
  if (error == ERR_NETWORK_IO_SUSPENDED) {
    next_state_ = State::kDone;
    return error;
  }

  // Try falling back to the next address in the list.
  next_state_ = State::kObtainIPEndPoint;
  // Don't overwrite uninteresting errors - this is mostly so failures in
  // IsIPEndPointUsable() can reuse this code without overwriting old results.
  if (error != ERR_NAME_NOT_RESOLVED) {
    last_error_ = error;
  }
  return OK;
}

}  // namespace net

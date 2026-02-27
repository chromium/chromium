// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_connect_job_connector.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ref.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"
#include "net/socket/websocket_stream_socket.h"

namespace net {

TcpConnectJob::Connector::Connector(TcpConnectJob* parent,
                                    std::string_view name)
    : parent_(*parent), name_(name) {}

TcpConnectJob::Connector::~Connector() = default;

int TcpConnectJob::Connector::TryAdvanceState() {
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

ServiceEndpoint TcpConnectJob::Connector::PassFinalServiceEndpoint() {
  DCHECK_EQ(next_state_, State::kDone);
  DCHECK(final_service_endpoint_);
  return std::move(final_service_endpoint_).value();
}

void TcpConnectJob::Connector::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING) {
    parent_->OnConnectorComplete(rv, *this);
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

  TcpConnectJob::IPEndPointInfo endpoint_info =
      parent_->GetNextIPEndPoint(*this);

  if (!endpoint_info.has_value()) {
    if (endpoint_info.error() == ERR_IO_PENDING) {
      // No IPEndPoints available, but the DNS request is not done yet. Need to
      // wait for more data.
      next_state_ = State::kWaitForIPEndPoint;
      return ERR_IO_PENDING;
    }

    // The parent class handles DNS errors itself, so this currently can only be
    // ERR_IO_PENDING or ERR_NAME_NOT_RESOLVED. ERR_NAME_NOT_RESOLVED means no
    // more IPs are incoming.
    CHECK_EQ(endpoint_info.error(), ERR_NAME_NOT_RESOLVED);

    // We failed to connect to any IP, and no more are coming.
    OnDone(endpoint_info.error());

    return endpoint_info.error();
  }

  current_address_ = std::move(endpoint_info).value();

  // Get lock if needed.
  next_state_ = State::kTcpConnect;
  return OK;
}

int TcpConnectJob::Connector::DoTcpConnect() {
  DCHECK(current_address_);

  next_state_ = State::kTcpConnectComplete;

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
          AddressList(*current_address_), std::move(socket_performance_watcher),
          parent_->network_quality_estimator(), net_log.net_log(),
          net_log.source());

  transport_socket_->ApplySocketTag(parent_->socket_tag());

  // If there's a `websocket_endpoint_lock_manager`, then this is a WebSocket
  // connection attempt, and a lock must be obtained on the destination endpoint
  // before connecting. Wrap `socket` in a `WebSocketStreamSocket`, which will
  // wait for the lock before connecting, and then release it on destruction.
  if (parent_->websocket_endpoint_lock_manager()) {
    transport_socket_ = std::make_unique<WebSocketStreamSocket>(
        *parent_->websocket_endpoint_lock_manager(), *current_address_,
        std::move(transport_socket_));
  }

  parent_->net_log().AddEvent(
      NetLogEventType::TCP_CONNECT_JOB_CONNECTOR_CONNECT_START, [&] {
        base::DictValue dict;
        dict.Set("address", current_address_->ToString());
        dict.Set("connector", name_);
        transport_socket_->NetLog().source().AddToEventParameters(dict);
        return dict;
      });

  return transport_socket_->Connect(base::BindOnce(
      &TcpConnectJob::Connector::OnIOComplete, base::Unretained(this)));
}

int TcpConnectJob::Connector::DoTcpConnectComplete(int result) {
  parent_->net_log().AddEvent(
      NetLogEventType::TCP_CONNECT_JOB_CONNECTOR_CONNECT_COMPLETE, [&] {
        base::DictValue dict = NetLogDict();
        if (result) {
          dict.Set("net_error", result);
        }
        return dict;
      });

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

  // Search for the corresponding ServiceEndpoint. If not found, the address is
  // not usable.
  const ServiceEndpoint* service_endpoint =
      parent_->FindServiceEndpoint(*current_address_);

  parent_->net_log().AddEvent(
      NetLogEventType::TCP_CONNECT_JOB_VERIFY_IP_ENDPOINT_USABLE, [&] {
        return NetLogDict().Set("is_usable", service_endpoint != nullptr);
      });

  if (!service_endpoint) {
    // If the address is not usable, treat it as a failure of the current IP.
    //
    // We could more proactively probe for this situation when we receive DNS
    // results and notice they're now cypto ready, to avoid waiting for connect
    // complete before checking again, and weren't before. Unclear if this case
    // is common enough to be worth the more complicated state transitions.
    return OnEndpointFailed(ERR_NAME_NOT_RESOLVED);
  }

  final_service_endpoint_ = *service_endpoint;
  OnDone(OK);
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

  // If this isn't an IsIPEndPointUsable() error, record the failed connection
  // attempt. IsIPEndPointUsable() aren't connection errors, and are potentially
  // less interesting than connection errors, though we don't actually know if
  // the older error is from a usable endpoint or not.
  if (error != ERR_NAME_NOT_RESOLVED) {
    parent_->connection_attempts_.emplace_back(*current_address_, error);
  }

  // Drop the socket to release the endpoint lock, if there is one.
  transport_socket_.reset();
  current_address_.reset();

  // Don't try the next address if entering suspend mode.
  if (error == ERR_NETWORK_IO_SUSPENDED) {
    OnDone(error);
    return error;
  }

  // Try falling back to the next address in the list.
  next_state_ = State::kObtainIPEndPoint;
  return OK;
}

void TcpConnectJob::Connector::OnDone(int result) {
  DCHECK_NE(next_state_, State::kDone);
  next_state_ = State::kDone;

  parent_->net_log().AddEvent(NetLogEventType::TCP_CONNECT_JOB_CONNECTOR_DONE,
                              [&] {
                                base::DictValue dict = NetLogDict();
                                if (result) {
                                  dict.Set("net_error", result);
                                }
                                return dict;
                              });
}

base::DictValue TcpConnectJob::Connector::NetLogDict() const {
  return base::DictValue().Set("connector", name_);
}

}  // namespace net

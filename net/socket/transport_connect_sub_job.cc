// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_connect_sub_job.h"

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/socket/socket_performance_watcher_factory.h"
#include "net/socket/websocket_stream_socket.h"

namespace net {

TransportConnectSubJob::TransportConnectSubJob(
    std::vector<IPEndPoint> addresses,
    TransportConnectJob* parent_job,
    SubJobType type)
    : parent_job_(parent_job), addresses_(std::move(addresses)), type_(type) {}

TransportConnectSubJob::~TransportConnectSubJob() = default;

// Start connecting.
int TransportConnectSubJob::Start() {
  DCHECK_EQ(STATE_NONE, next_state_);
  next_state_ = STATE_TRANSPORT_CONNECT;
  return DoLoop(OK);
}

LoadState TransportConnectSubJob::GetLoadState() const {
  switch (next_state_) {
    case STATE_TRANSPORT_CONNECT:
    case STATE_TRANSPORT_CONNECT_COMPLETE:
    case STATE_DONE:
      return LOAD_STATE_CONNECTING;
    case STATE_NONE:
      return LOAD_STATE_IDLE;
  }
  NOTREACHED();
}

const IPEndPoint& TransportConnectSubJob::CurrentAddress() const {
  DCHECK_LT(current_address_index_, addresses_.size());
  return addresses_[current_address_index_];
}

void TransportConnectSubJob::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    parent_job_->OnSubJobComplete(rv, this);  // |this| deleted
}

int TransportConnectSubJob::DoLoop(int result) {
  DCHECK_NE(next_state_, STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_TRANSPORT_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoTransportConnect();
        break;
      case STATE_TRANSPORT_CONNECT_COMPLETE:
        rv = DoTransportConnectComplete(rv);
        break;
      default:
        NOTREACHED();
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE &&
           next_state_ != STATE_DONE);

  return rv;
}

int TransportConnectSubJob::DoTransportConnect() {
  next_state_ = STATE_TRANSPORT_CONNECT_COMPLETE;
  AddressList one_address(CurrentAddress());

  // Create a `SocketPerformanceWatcher`, and pass the ownership.
  std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher;
  if (auto* factory = parent_job_->socket_performance_watcher_factory();
      factory != nullptr) {
    socket_performance_watcher = factory->CreateSocketPerformanceWatcher(
        SocketPerformanceWatcherFactory::PROTOCOL_TCP,
        CurrentAddress().address());
  }

  const NetLogWithSource& net_log = parent_job_->net_log();
  transport_socket_ =
      parent_job_->client_socket_factory()->CreateTransportClientSocket(
          one_address, std::move(socket_performance_watcher),
          parent_job_->network_quality_estimator(), net_log.net_log(),
          net_log.source());

  net_log.AddEvent(NetLogEventType::TRANSPORT_CONNECT_JOB_CONNECT_ATTEMPT, [&] {
    auto dict = base::DictValue().Set("address", CurrentAddress().ToString());
    transport_socket_->NetLog().source().AddToEventParameters(dict);
    return dict;
  });

  transport_socket_->ApplySocketTag(parent_job_->socket_tag());

  // If there's a `websocket_endpoint_lock_manager`, then this is a WebSocket
  // connection attempt, and a lock must be obtained on the destination endpoint
  // before connecting. Wrap `socket` in a `WebSocketStreamSocket`, which will
  // wait for the lock before connecting, and then release it on destruction.
  if (parent_job_->websocket_endpoint_lock_manager()) {
    transport_socket_ = std::make_unique<WebSocketStreamSocket>(
        *parent_job_->websocket_endpoint_lock_manager(), CurrentAddress(),
        std::move(transport_socket_));
  }

  // This use of base::Unretained() is safe because transport_socket_ is
  // destroyed in the destructor.
  return transport_socket_->Connect(base::BindOnce(
      &TransportConnectSubJob::OnIOComplete, base::Unretained(this)));
}

int TransportConnectSubJob::DoTransportConnectComplete(int result) {
  next_state_ = STATE_DONE;
  if (result != OK) {
    // Drop the socket to release the endpoint lock, if any.
    transport_socket_.reset();

    parent_job_->connection_attempts_.emplace_back(CurrentAddress(), result);

    // Don't try the next address if entering suspend mode.
    if (result != ERR_NETWORK_IO_SUSPENDED &&
        current_address_index_ + 1 < addresses_.size()) {
      // Try falling back to the next address in the list.
      next_state_ = STATE_TRANSPORT_CONNECT;
      ++current_address_index_;
      result = OK;
    }

    return result;
  }

  return result;
}

}  // namespace net

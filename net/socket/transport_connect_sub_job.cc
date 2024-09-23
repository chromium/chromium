// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/transport_connect_sub_job.h"

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
#include "net/socket/websocket_endpoint_lock_manager.h"

namespace net {

namespace {

// StreamSocket wrapper that registers/unregisters the wrapped StreamSocket with
// a WebSocketEndpointLockManager on creation/destruction.
class WebSocketStreamSocket final : public StreamSocket {
 public:
  WebSocketStreamSocket(
      std::unique_ptr<StreamSocket> wrapped_socket,
      WebSocketEndpointLockManager* websocket_endpoint_lock_manager,
      const IPEndPoint& address)
      : wrapped_socket_(std::move(wrapped_socket)),
        lock_releaser_(websocket_endpoint_lock_manager, address) {}

  WebSocketStreamSocket(const WebSocketStreamSocket&) = delete;
  WebSocketStreamSocket& operator=(const WebSocketStreamSocket&) = delete;

  ~WebSocketStreamSocket() override = default;

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           CompletionOnceCallback callback) override {
    return wrapped_socket_->Read(buf, buf_len, std::move(callback));
  }
  int ReadIfReady(IOBuffer* buf,
                  int buf_len,
                  CompletionOnceCallback callback) override {
    return wrapped_socket_->ReadIfReady(buf, buf_len, std::move(callback));
  }
  int CancelReadIfReady() override {
    return wrapped_socket_->CancelReadIfReady();
  }
  int Write(IOBuffer* buf,
            int buf_len,
            CompletionOnceCallback callback,
            const NetworkTrafficAnnotationTag& traffic_annotation) override {
    return wrapped_socket_->Write(buf, buf_len, std::move(callback),
                                  traffic_annotation);
  }
  int SetReceiveBufferSize(int32_t size) override {
    return wrapped_socket_->SetReceiveBufferSize(size);
  }
  int SetSendBufferSize(int32_t size) override {
    return wrapped_socket_->SetSendBufferSize(size);
  }
  void SetDnsAliases(std::set<std::string> aliases) override {
    wrapped_socket_->SetDnsAliases(std::move(aliases));
  }
  const std::set<std::string>& GetDnsAliases() const override {
    return wrapped_socket_->GetDnsAliases();
  }

  // StreamSocket implementation:
  int Connect(CompletionOnceCallback callback) override {
    return wrapped_socket_->Connect(std::move(callback));
  }
  void Disconnect() override { wrapped_socket_->Disconnect(); }
  bool IsConnected() const override { return wrapped_socket_->IsConnected(); }
  bool IsConnectedAndIdle() const override {
    return wrapped_socket_->IsConnectedAndIdle();
  }
  int GetPeerAddress(IPEndPoint* address) const override {
    return wrapped_socket_->GetPeerAddress(address);
  }
  int GetLocalAddress(IPEndPoint* address) const override {
    return wrapped_socket_->GetLocalAddress(address);
  }
  const NetLogWithSource& NetLog() const override {
    return wrapped_socket_->NetLog();
  }
  bool WasEverUsed() const override { return wrapped_socket_->WasEverUsed(); }
  NextProto GetNegotiatedProtocol() const override {
    return wrapped_socket_->GetNegotiatedProtocol();
  }
  bool GetSSLInfo(SSLInfo* ssl_info) override {
    return wrapped_socket_->GetSSLInfo(ssl_info);
  }
  int64_t GetTotalReceivedBytes() const override {
    return wrapped_socket_->GetTotalReceivedBytes();
  }
  void ApplySocketTag(const SocketTag& tag) override {
    wrapped_socket_->ApplySocketTag(tag);
  }

 private:
  std::unique_ptr<StreamSocket> wrapped_socket_;
  WebSocketEndpointLockManager::LockReleaser lock_releaser_;
};

}  // namespace

TransportConnectSubJob::TransportConnectSubJob(
    std::vector<IPEndPoint> addresses,
    TransportConnectJob* parent_job,
    SubJobType type)
    : parent_job_(parent_job), addresses_(std::move(addresses)), type_(type) {}

TransportConnectSubJob::~TransportConnectSubJob() = default;

// Start connecting.
int TransportConnectSubJob::Start() {
  DCHECK_EQ(STATE_NONE, next_state_);
  next_state_ = STATE_OBTAIN_LOCK;
  return DoLoop(OK);
}

// Called by WebSocketEndpointLockManager when the lock becomes available.
void TransportConnectSubJob::GotEndpointLock() {
  DCHECK_EQ(STATE_OBTAIN_LOCK_COMPLETE, next_state_);
  OnIOComplete(OK);
}

LoadState TransportConnectSubJob::GetLoadState() const {
  switch (next_state_) {
    case STATE_OBTAIN_LOCK:
    case STATE_OBTAIN_LOCK_COMPLETE:
      // TODO(ricea): Add a WebSocket-specific LOAD_STATE ?
      return LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET;
    case STATE_TRANSPORT_CONNECT_COMPLETE:
    case STATE_DONE:
      return LOAD_STATE_CONNECTING;
    case STATE_NONE:
      return LOAD_STATE_IDLE;
  }
  NOTREACHED_IN_MIGRATION();
  return LOAD_STATE_IDLE;
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
      case STATE_OBTAIN_LOCK:
        DCHECK_EQ(OK, rv);
        rv = DoEndpointLock();
        break;
      case STATE_OBTAIN_LOCK_COMPLETE:
        DCHECK_EQ(OK, rv);
        rv = DoEndpointLockComplete();
        break;
      case STATE_TRANSPORT_CONNECT_COMPLETE:
        rv = DoTransportConnectComplete(rv);
        break;
      default:
        NOTREACHED_IN_MIGRATION();
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE &&
           next_state_ != STATE_DONE);

  return rv;
}

int TransportConnectSubJob::DoEndpointLock() {
  next_state_ = STATE_OBTAIN_LOCK_COMPLETE;
  if (!parent_job_->websocket_endpoint_lock_manager()) {
    return OK;
  }
  return parent_job_->websocket_endpoint_lock_manager()->LockEndpoint(
      CurrentAddress(), this);
}

int TransportConnectSubJob::DoEndpointLockComplete() {
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
    auto dict = base::Value::Dict().Set("address", CurrentAddress().ToString());
    transport_socket_->NetLog().source().AddToEventParameters(dict);
    return dict;
  });

  // If `websocket_endpoint_lock_manager_` is non-null, this class now owns an
  // endpoint lock. Wrap `socket` in a `WebSocketStreamSocket` to take ownership
  // of the lock and release it when the socket goes out of scope. This must
  // happen before any early returns in this method.
  if (parent_job_->websocket_endpoint_lock_manager()) {
    transport_socket_ = std::make_unique<WebSocketStreamSocket>(
        std::move(transport_socket_),
        parent_job_->websocket_endpoint_lock_manager(), CurrentAddress());
  }

  transport_socket_->ApplySocketTag(parent_job_->socket_tag());

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

    parent_job_->connection_attempts_.push_back(
        ConnectionAttempt(CurrentAddress(), result));

    // Don't try the next address if entering suspend mode.
    if (result != ERR_NETWORK_IO_SUSPENDED &&
        current_address_index_ + 1 < addresses_.size()) {
      // Try falling back to the next address in the list.
      next_state_ = STATE_OBTAIN_LOCK;
      ++current_address_index_;
      result = OK;
    }

    return result;
  }

  return result;
}

}  // namespace net

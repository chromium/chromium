// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/websocket_transport_connect_sub_job.h"

#include "base/bind.h"
#include "base/logging.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/client_socket_factory.h"
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
  bool WasAlpnNegotiated() const override {
    return wrapped_socket_->WasAlpnNegotiated();
  }
  NextProto GetNegotiatedProtocol() const override {
    return wrapped_socket_->GetNegotiatedProtocol();
  }
  bool GetSSLInfo(SSLInfo* ssl_info) override {
    return wrapped_socket_->GetSSLInfo(ssl_info);
  }
  void GetConnectionAttempts(ConnectionAttempts* out) const override {
    wrapped_socket_->GetConnectionAttempts(out);
  }
  void ClearConnectionAttempts() override {
    wrapped_socket_->ClearConnectionAttempts();
  }
  void AddConnectionAttempts(const ConnectionAttempts& attempts) override {
    wrapped_socket_->AddConnectionAttempts(attempts);
  }
  int64_t GetTotalReceivedBytes() const override {
    return wrapped_socket_->GetTotalReceivedBytes();
  }
  void DumpMemoryStats(SocketMemoryStats* stats) const override {
    wrapped_socket_->DumpMemoryStats(stats);
  }
  void ApplySocketTag(const SocketTag& tag) override {
    wrapped_socket_->ApplySocketTag(tag);
  }

 private:
  std::unique_ptr<StreamSocket> wrapped_socket_;
  WebSocketEndpointLockManager::LockReleaser lock_releaser_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketStreamSocket);
};

}  // namespace

WebSocketTransportConnectSubJob::WebSocketTransportConnectSubJob(
    const AddressList& addresses,
    WebSocketTransportConnectJob* parent_job,
    SubJobType type,
    WebSocketEndpointLockManager* websocket_endpoint_lock_manager)
    : parent_job_(parent_job),
      addresses_(addresses),
      current_address_index_(0),
      next_state_(STATE_NONE),
      type_(type),
      websocket_endpoint_lock_manager_(websocket_endpoint_lock_manager) {}

WebSocketTransportConnectSubJob::~WebSocketTransportConnectSubJob() {
  // We don't worry about cancelling the TCP connect, since ~StreamSocket will
  // take care of it.
  if (next()) {
    DCHECK_EQ(STATE_OBTAIN_LOCK_COMPLETE, next_state_);
    // The ~Waiter destructor will remove this object from the waiting list.
  } else if (next_state_ == STATE_TRANSPORT_CONNECT_COMPLETE) {
    websocket_endpoint_lock_manager_->UnlockEndpoint(CurrentAddress());
  }
}

// Start connecting.
int WebSocketTransportConnectSubJob::Start() {
  DCHECK_EQ(STATE_NONE, next_state_);
  next_state_ = STATE_OBTAIN_LOCK;
  return DoLoop(OK);
}

// Called by WebSocketEndpointLockManager when the lock becomes available.
void WebSocketTransportConnectSubJob::GotEndpointLock() {
  DCHECK_EQ(STATE_OBTAIN_LOCK_COMPLETE, next_state_);
  OnIOComplete(OK);
}

LoadState WebSocketTransportConnectSubJob::GetLoadState() const {
  switch (next_state_) {
    case STATE_OBTAIN_LOCK:
    case STATE_OBTAIN_LOCK_COMPLETE:
      // TODO(ricea): Add a WebSocket-specific LOAD_STATE ?
      return LOAD_STATE_WAITING_FOR_AVAILABLE_SOCKET;
    case STATE_TRANSPORT_CONNECT:
    case STATE_TRANSPORT_CONNECT_COMPLETE:
    case STATE_DONE:
      return LOAD_STATE_CONNECTING;
    case STATE_NONE:
      return LOAD_STATE_IDLE;
  }
  NOTREACHED();
  return LOAD_STATE_IDLE;
}

ClientSocketFactory* WebSocketTransportConnectSubJob::client_socket_factory()
    const {
  return parent_job_->client_socket_factory();
}

const NetLogWithSource& WebSocketTransportConnectSubJob::net_log() const {
  return parent_job_->net_log();
}

const IPEndPoint& WebSocketTransportConnectSubJob::CurrentAddress() const {
  DCHECK_LT(current_address_index_, addresses_.size());
  return addresses_[current_address_index_];
}

void WebSocketTransportConnectSubJob::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    parent_job_->OnSubJobComplete(rv, this);  // |this| deleted
}

int WebSocketTransportConnectSubJob::DoLoop(int result) {
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
      case STATE_TRANSPORT_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoTransportConnect();
        break;
      case STATE_TRANSPORT_CONNECT_COMPLETE:
        rv = DoTransportConnectComplete(rv);
        break;
      default:
        NOTREACHED();
        rv = ERR_FAILED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE &&
           next_state_ != STATE_DONE);

  return rv;
}

int WebSocketTransportConnectSubJob::DoEndpointLock() {
  int rv =
      websocket_endpoint_lock_manager_->LockEndpoint(CurrentAddress(), this);
  next_state_ = STATE_OBTAIN_LOCK_COMPLETE;
  return rv;
}

int WebSocketTransportConnectSubJob::DoEndpointLockComplete() {
  next_state_ = STATE_TRANSPORT_CONNECT;
  return OK;
}

int WebSocketTransportConnectSubJob::DoTransportConnect() {
  // TODO(ricea): Update global g_last_connect_time and report
  // ConnectInterval.
  next_state_ = STATE_TRANSPORT_CONNECT_COMPLETE;
  AddressList one_address(CurrentAddress());
  transport_socket_ = client_socket_factory()->CreateTransportClientSocket(
      one_address, nullptr, net_log().net_log(), net_log().source());
  // This use of base::Unretained() is safe because transport_socket_ is
  // destroyed in the destructor.
  return transport_socket_->Connect(base::BindOnce(
      &WebSocketTransportConnectSubJob::OnIOComplete, base::Unretained(this)));
}

int WebSocketTransportConnectSubJob::DoTransportConnectComplete(int result) {
  next_state_ = STATE_DONE;
  if (result != OK) {
    websocket_endpoint_lock_manager_->UnlockEndpoint(CurrentAddress());

    if (current_address_index_ + 1 < addresses_.size()) {
      // Try falling back to the next address in the list.
      next_state_ = STATE_OBTAIN_LOCK;
      ++current_address_index_;
      result = OK;
    }

    return result;
  }

  // On success, need to register the socket with the
  // WebSocketEndpointLockManager.
  transport_socket_ = std::make_unique<WebSocketStreamSocket>(
      std::move(transport_socket_), websocket_endpoint_lock_manager_,
      CurrentAddress());

  return result;
}

}  // namespace net

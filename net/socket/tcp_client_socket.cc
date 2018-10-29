// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/tcp_client_socket.h"

#include <utility>

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/socket/socket_performance_watcher.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace net {

class NetLogWithSource;

TCPClientSocket::TCPClientSocket(
    const AddressList& addresses,
    std::unique_ptr<SocketPerformanceWatcher> socket_performance_watcher,
    net::NetLog* net_log,
    const net::NetLogSource& source)
    : TCPClientSocket(
          std::make_unique<TCPSocket>(std::move(socket_performance_watcher),
                                      net_log,
                                      source),
          addresses,
          -1 /* current_address_index */,
          nullptr /* bind_address */) {}

TCPClientSocket::TCPClientSocket(std::unique_ptr<TCPSocket> connected_socket,
                                 const IPEndPoint& peer_address)
    : TCPClientSocket(std::move(connected_socket),
                      AddressList(peer_address),
                      0 /* current_address_index */,
                      nullptr /* bind_address */) {}

TCPClientSocket::~TCPClientSocket() {
  Disconnect();
}

std::unique_ptr<TCPClientSocket> TCPClientSocket::CreateFromBoundSocket(
    std::unique_ptr<TCPSocket> bound_socket,
    const AddressList& addresses,
    const IPEndPoint& bound_address) {
  return base::WrapUnique(new TCPClientSocket(
      std::move(bound_socket), addresses, -1 /* current_address_index */,
      std::make_unique<IPEndPoint>(bound_address)));
}

int TCPClientSocket::Bind(const IPEndPoint& address) {
  if (current_address_index_ >= 0 || bind_address_) {
    // Cannot bind the socket if we are already connected or connecting.
    NOTREACHED();
    return ERR_UNEXPECTED;
  }

  int result = OK;
  if (!socket_->IsValid()) {
    result = OpenSocket(address.GetFamily());
    if (result != OK)
      return result;
  }

  result = socket_->Bind(address);
  if (result != OK)
    return result;

  bind_address_.reset(new IPEndPoint(address));
  return OK;
}

bool TCPClientSocket::SetKeepAlive(bool enable, int delay) {
  return socket_->SetKeepAlive(enable, delay);
}

bool TCPClientSocket::SetNoDelay(bool no_delay) {
  return socket_->SetNoDelay(no_delay);
}

void TCPClientSocket::SetBeforeConnectCallback(
    const BeforeConnectCallback& before_connect_callback) {
  DCHECK_EQ(CONNECT_STATE_NONE, next_connect_state_);
  before_connect_callback_ = before_connect_callback;
}

int TCPClientSocket::Connect(CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());

  // If connecting or already connected, then just return OK.
  if (socket_->IsValid() && current_address_index_ >= 0)
    return OK;

  socket_->StartLoggingMultipleConnectAttempts(addresses_);

  // We will try to connect to each address in addresses_. Start with the
  // first one in the list.
  next_connect_state_ = CONNECT_STATE_CONNECT;
  current_address_index_ = 0;

  int rv = DoConnectLoop(OK);
  if (rv == ERR_IO_PENDING) {
    connect_callback_ = std::move(callback);
  } else {
    socket_->EndLoggingMultipleConnectAttempts(rv);
  }

  return rv;
}

TCPClientSocket::TCPClientSocket(std::unique_ptr<TCPSocket> socket,
                                 const AddressList& addresses,
                                 int current_address_index,
                                 std::unique_ptr<IPEndPoint> bind_address)
    : socket_(std::move(socket)),
      bind_address_(std::move(bind_address)),
      addresses_(addresses),
      current_address_index_(-1),
      next_connect_state_(CONNECT_STATE_NONE),
      previously_disconnected_(false),
      total_received_bytes_(0),
      was_ever_used_(false) {
  DCHECK(socket_);
  if (socket_->IsValid())
    socket_->SetDefaultOptionsForClient();
}

int TCPClientSocket::ReadCommon(IOBuffer* buf,
                                int buf_len,
                                CompletionOnceCallback callback,
                                bool read_if_ready) {
  DCHECK(!callback.is_null());

  // |socket_| is owned by |this| and the callback won't be run once |socket_|
  // is gone/closed. Therefore, it is safe to use base::Unretained() here.
  CompletionOnceCallback read_callback =
      base::BindOnce(&TCPClientSocket::DidCompleteRead, base::Unretained(this),
                     std::move(callback));
  int result =
      read_if_ready
          ? socket_->ReadIfReady(buf, buf_len, std::move(read_callback))
          : socket_->Read(buf, buf_len, std::move(read_callback));
  if (result > 0) {
    was_ever_used_ = true;
    total_received_bytes_ += result;
  }

  return result;
}

int TCPClientSocket::DoConnectLoop(int result) {
  DCHECK_NE(next_connect_state_, CONNECT_STATE_NONE);

  int rv = result;
  do {
    ConnectState state = next_connect_state_;
    next_connect_state_ = CONNECT_STATE_NONE;
    switch (state) {
      case CONNECT_STATE_CONNECT:
        DCHECK_EQ(OK, rv);
        rv = DoConnect();
        break;
      case CONNECT_STATE_CONNECT_COMPLETE:
        rv = DoConnectComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state " << state;
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_connect_state_ != CONNECT_STATE_NONE);

  return rv;
}

int TCPClientSocket::DoConnect() {
  DCHECK_GE(current_address_index_, 0);
  DCHECK_LT(current_address_index_, static_cast<int>(addresses_.size()));

  const IPEndPoint& endpoint = addresses_[current_address_index_];

  if (previously_disconnected_) {
    was_ever_used_ = false;
    connection_attempts_.clear();
    previously_disconnected_ = false;
  }

  next_connect_state_ = CONNECT_STATE_CONNECT_COMPLETE;

  if (socket_->IsValid()) {
    DCHECK(bind_address_);
  } else {
    int result = OpenSocket(endpoint.GetFamily());
    if (result != OK)
      return result;

    if (bind_address_) {
      result = socket_->Bind(*bind_address_);
      if (result != OK) {
        socket_->Close();
        return result;
      }
    }
  }

  if (before_connect_callback_) {
    int result = before_connect_callback_.Run();
    DCHECK_NE(ERR_IO_PENDING, result);
    if (result != net::OK)
      return result;
  }

  // Notify |socket_performance_watcher_| only if the |socket_| is reused to
  // connect to a different IP Address.
  if (socket_->socket_performance_watcher() && current_address_index_ != 0)
    socket_->socket_performance_watcher()->OnConnectionChanged();

  // |socket_| is owned by this class and the callback won't be run once
  // |socket_| is gone. Therefore, it is safe to use base::Unretained() here.
  return socket_->Connect(endpoint,
                          base::Bind(&TCPClientSocket::DidCompleteConnect,
                                     base::Unretained(this)));
}

int TCPClientSocket::DoConnectComplete(int result) {
  if (result == OK)
    return OK;  // Done!

  connection_attempts_.push_back(
      ConnectionAttempt(addresses_[current_address_index_], result));

  // Close whatever partially connected socket we currently have.
  DoDisconnect();

  // Try to fall back to the next address in the list.
  if (current_address_index_ + 1 < static_cast<int>(addresses_.size())) {
    next_connect_state_ = CONNECT_STATE_CONNECT;
    ++current_address_index_;
    return OK;
  }

  // Otherwise there is nothing to fall back to, so give up.
  return result;
}

void TCPClientSocket::Disconnect() {
  DoDisconnect();
  current_address_index_ = -1;
  bind_address_.reset();
}

void TCPClientSocket::DoDisconnect() {
  total_received_bytes_ = 0;
  EmitTCPMetricsHistogramsOnDisconnect();
  // If connecting or already connected, record that the socket has been
  // disconnected.
  previously_disconnected_ = socket_->IsValid() && current_address_index_ >= 0;
  socket_->Close();
}

bool TCPClientSocket::IsConnected() const {
  return socket_->IsConnected();
}

bool TCPClientSocket::IsConnectedAndIdle() const {
  return socket_->IsConnectedAndIdle();
}

int TCPClientSocket::GetPeerAddress(IPEndPoint* address) const {
  return socket_->GetPeerAddress(address);
}

int TCPClientSocket::GetLocalAddress(IPEndPoint* address) const {
  DCHECK(address);

  if (!socket_->IsValid()) {
    if (bind_address_) {
      *address = *bind_address_;
      return OK;
    }
    return ERR_SOCKET_NOT_CONNECTED;
  }

  return socket_->GetLocalAddress(address);
}

const NetLogWithSource& TCPClientSocket::NetLog() const {
  return socket_->net_log();
}

bool TCPClientSocket::WasEverUsed() const {
  return was_ever_used_;
}

void TCPClientSocket::EnableTCPFastOpenIfSupported() {
  socket_->EnableTCPFastOpenIfSupported();
}

bool TCPClientSocket::WasAlpnNegotiated() const {
  return false;
}

NextProto TCPClientSocket::GetNegotiatedProtocol() const {
  return kProtoUnknown;
}

bool TCPClientSocket::GetSSLInfo(SSLInfo* ssl_info) {
  return false;
}

int TCPClientSocket::Read(IOBuffer* buf,
                          int buf_len,
                          CompletionOnceCallback callback) {
  return ReadCommon(buf, buf_len, std::move(callback), /*read_if_ready=*/false);
}

int TCPClientSocket::ReadIfReady(IOBuffer* buf,
                                 int buf_len,
                                 CompletionOnceCallback callback) {
  return ReadCommon(buf, buf_len, std::move(callback), /*read_if_ready=*/true);
}

int TCPClientSocket::CancelReadIfReady() {
  return socket_->CancelReadIfReady();
}

int TCPClientSocket::Write(
    IOBuffer* buf,
    int buf_len,
    CompletionOnceCallback callback,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK(!callback.is_null());

  // |socket_| is owned by this class and the callback won't be run once
  // |socket_| is gone. Therefore, it is safe to use base::Unretained() here.
  CompletionOnceCallback write_callback =
      base::BindOnce(&TCPClientSocket::DidCompleteWrite, base::Unretained(this),
                     std::move(callback));
  int result = socket_->Write(buf, buf_len, std::move(write_callback),
                              traffic_annotation);
  if (result > 0)
    was_ever_used_ = true;

  return result;
}

int TCPClientSocket::SetReceiveBufferSize(int32_t size) {
  return socket_->SetReceiveBufferSize(size);
}

int TCPClientSocket::SetSendBufferSize(int32_t size) {
    return socket_->SetSendBufferSize(size);
}

SocketDescriptor TCPClientSocket::SocketDescriptorForTesting() const {
  return socket_->SocketDescriptorForTesting();
}

void TCPClientSocket::GetConnectionAttempts(ConnectionAttempts* out) const {
  *out = connection_attempts_;
}

void TCPClientSocket::ClearConnectionAttempts() {
  connection_attempts_.clear();
}

void TCPClientSocket::AddConnectionAttempts(
    const ConnectionAttempts& attempts) {
  connection_attempts_.insert(connection_attempts_.begin(), attempts.begin(),
                              attempts.end());
}

int64_t TCPClientSocket::GetTotalReceivedBytes() const {
  return total_received_bytes_;
}

void TCPClientSocket::ApplySocketTag(const SocketTag& tag) {
  socket_->ApplySocketTag(tag);
}

void TCPClientSocket::DidCompleteConnect(int result) {
  DCHECK_EQ(next_connect_state_, CONNECT_STATE_CONNECT_COMPLETE);
  DCHECK_NE(result, ERR_IO_PENDING);
  DCHECK(!connect_callback_.is_null());

  result = DoConnectLoop(result);
  if (result != ERR_IO_PENDING) {
    socket_->EndLoggingMultipleConnectAttempts(result);
    std::move(connect_callback_).Run(result);
  }
}

void TCPClientSocket::DidCompleteRead(CompletionOnceCallback callback,
                                      int result) {
  if (result > 0)
    total_received_bytes_ += result;

  DidCompleteReadWrite(std::move(callback), result);
}

void TCPClientSocket::DidCompleteWrite(CompletionOnceCallback callback,
                                       int result) {
  DidCompleteReadWrite(std::move(callback), result);
}

void TCPClientSocket::DidCompleteReadWrite(CompletionOnceCallback callback,
                                           int result) {
  if (result > 0)
    was_ever_used_ = true;
  std::move(callback).Run(result);
}

int TCPClientSocket::OpenSocket(AddressFamily family) {
  DCHECK(!socket_->IsValid());

  int result = socket_->Open(family);
  if (result != OK)
    return result;

  socket_->SetDefaultOptionsForClient();

  return OK;
}

void TCPClientSocket::EmitTCPMetricsHistogramsOnDisconnect() {
  base::TimeDelta rtt;
  if (socket_->GetEstimatedRoundTripTime(&rtt)) {
    UMA_HISTOGRAM_CUSTOM_TIMES("Net.TcpRtt.AtDisconnect", rtt,
                               base::TimeDelta::FromMilliseconds(1),
                               base::TimeDelta::FromMinutes(10), 100);
  }
}

}  // namespace net

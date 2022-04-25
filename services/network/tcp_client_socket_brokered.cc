// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/tcp_client_socket_brokered.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/socket/connection_attempts.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace network {

TCPClientSocketBrokered::TCPClientSocketBrokered(
    const net::AddressList& addresses,
    std::unique_ptr<net::SocketPerformanceWatcher> socket_performance_watcher,
    net::NetworkQualityEstimator* network_quality_estimator,
    net::NetLog* net_log,
    const net::NetLogSource& source)
    : addresses_(addresses),
      socket_performance_watcher_(std::move(socket_performance_watcher)),
      network_quality_estimator_(network_quality_estimator),
      net_log_(net_log),
      source_(source) {}

TCPClientSocketBrokered::~TCPClientSocketBrokered() {
  Disconnect();
}

int TCPClientSocketBrokered::Bind(const net::IPEndPoint& address) {
  // TODO(liza): Implement this.
  NOTREACHED();
  return net::OK;
}

bool TCPClientSocketBrokered::SetKeepAlive(bool enable, int delay) {
  if (!brokered_socket_) {
    return false;
  }
  return brokered_socket_->SetKeepAlive(enable, delay);
}

bool TCPClientSocketBrokered::SetNoDelay(bool no_delay) {
  if (!brokered_socket_) {
    return false;
  }
  return brokered_socket_->SetNoDelay(no_delay);
}

void TCPClientSocketBrokered::SetSocketCreatorForTesting(
    base::RepeatingCallback<std::unique_ptr<net::TransportClientSocket>(void)>
        socket_creator) {
  socket_creator_for_testing_ = std::move(socket_creator);
}

net::ConnectionAttempts TCPClientSocketBrokered::GetConnectionAttempts() const {
  if (!brokered_socket_) {
    return {};
  }
  return brokered_socket_->GetConnectionAttempts();
}

void TCPClientSocketBrokered::SetBeforeConnectCallback(
    const BeforeConnectCallback& before_connect_callback) {
  // TODO(liza): Implement this.
  NOTREACHED();
}

int TCPClientSocketBrokered::Connect(net::CompletionOnceCallback callback) {
  // TODO(liza): add support for reconnecting disconnected socket, or look into
  // removing support for reconnection from TCPClientSocket if it's not needed.
  DCHECK(!callback.is_null());
  // If connecting or already connected, then just return OK.
  if (IsConnected() || is_connect_in_progress_)
    return net::OK;

  is_connect_in_progress_ = true;
  // TODO(liza): Add a mojo call that creates a TCPClientSocket and calls
  // Connect.
  if (socket_creator_for_testing_) {
    brokered_socket_ = socket_creator_for_testing_.Run();
  } else {
    brokered_socket_ = std::make_unique<net::TCPClientSocket>(
        addresses_, std::move(socket_performance_watcher_),
        network_quality_estimator_, net_log_, source_);
  }
  brokered_socket_->ApplySocketTag(tag_);
  return brokered_socket_->Connect(base::BindOnce(
      &TCPClientSocketBrokered::DidCompleteConnect,
      brokered_weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

int TCPClientSocketBrokered::OpenSocketForBind(const net::IPEndPoint& address) {
  // TODO(liza): Implement this.
  NOTREACHED();
  return net::OK;
}

void TCPClientSocketBrokered::DidCompleteOpenForBind(
    const net::IPEndPoint& address,
    std::unique_ptr<net::TCPSocket> new_socket,
    net::Error result) {
  // TODO(liza): Implement this.
  NOTREACHED();
}

void TCPClientSocketBrokered::DidCompleteConnect(
    net::CompletionOnceCallback callback,
    int result) {
  DCHECK_NE(result, net::ERR_IO_PENDING);

  std::move(callback).Run(result);
  is_connect_in_progress_ = false;
}

void TCPClientSocketBrokered::Disconnect() {
  if (brokered_socket_) {
    brokered_socket_->Disconnect();
  }
  is_connect_in_progress_ = false;
}

bool TCPClientSocketBrokered::IsConnected() const {
  if (!brokered_socket_) {
    return false;
  }

  return brokered_socket_->IsConnected();
}

bool TCPClientSocketBrokered::IsConnectedAndIdle() const {
  if (!brokered_socket_) {
    return false;
  }
  return brokered_socket_->IsConnectedAndIdle();
}

int TCPClientSocketBrokered::GetPeerAddress(net::IPEndPoint* address) const {
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->GetPeerAddress(std::move(address));
}

int TCPClientSocketBrokered::GetLocalAddress(net::IPEndPoint* address) const {
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->GetLocalAddress(std::move(address));
}

const net::NetLogWithSource& TCPClientSocketBrokered::NetLog() const {
  if (!brokered_socket_) {
    return NetLog();
  }
  return brokered_socket_->NetLog();
}

bool TCPClientSocketBrokered::WasEverUsed() const {
  if (!brokered_socket_) {
    return false;
  }
  return brokered_socket_->WasEverUsed();
}

bool TCPClientSocketBrokered::WasAlpnNegotiated() const {
  return false;
}

net::NextProto TCPClientSocketBrokered::GetNegotiatedProtocol() const {
  return net::kProtoUnknown;
}

bool TCPClientSocketBrokered::GetSSLInfo(net::SSLInfo* ssl_info) {
  return false;
}

int64_t TCPClientSocketBrokered::GetTotalReceivedBytes() const {
  if (!brokered_socket_) {
    return 0;
  }
  return brokered_socket_->GetTotalReceivedBytes();
}

void TCPClientSocketBrokered::ApplySocketTag(const net::SocketTag& tag) {
  if (!brokered_socket_) {
    tag_ = tag;
  } else {
    brokered_socket_->ApplySocketTag(tag);
  }
}

int TCPClientSocketBrokered::Read(net::IOBuffer* buf,
                                  int buf_len,
                                  net::CompletionOnceCallback callback) {
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->Read(buf, buf_len, std::move(callback));
}

int TCPClientSocketBrokered::ReadIfReady(net::IOBuffer* buf,
                                         int buf_len,
                                         net::CompletionOnceCallback callback) {
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->ReadIfReady(buf, buf_len, std::move(callback));
}

int TCPClientSocketBrokered::CancelReadIfReady() {
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->CancelReadIfReady();
}

int TCPClientSocketBrokered::Write(
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->Write(std::move(buf), buf_len, std::move(callback),
                                 traffic_annotation);
}

int TCPClientSocketBrokered::SetReceiveBufferSize(int32_t size) {
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->SetReceiveBufferSize(size);
}

int TCPClientSocketBrokered::SetSendBufferSize(int32_t size) {
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->SetSendBufferSize(size);
}

}  // namespace network

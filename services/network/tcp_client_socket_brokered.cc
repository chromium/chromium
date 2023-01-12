// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/tcp_client_socket_brokered.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/brokered_client_socket_factory.h"
#include "services/network/public/cpp/transferable_socket.h"

namespace network {

TCPClientSocketBrokered::TCPClientSocketBrokered(
    const net::AddressList& addresses,
    std::unique_ptr<net::SocketPerformanceWatcher> socket_performance_watcher,
    net::NetworkQualityEstimator* network_quality_estimator,
    net::NetLog* net_log,
    const net::NetLogSource& source,
    BrokeredClientSocketFactory* client_socket_factory)
    : addresses_(addresses),
      socket_performance_watcher_(std::move(socket_performance_watcher)),
      network_quality_estimator_(network_quality_estimator),
      net_log_(net_log),
      source_(source),
      client_socket_factory_(client_socket_factory) {}

TCPClientSocketBrokered::~TCPClientSocketBrokered() {
  Disconnect();
}

int TCPClientSocketBrokered::Bind(const net::IPEndPoint& address) {
  // TODO(liza): Implement this.
  NOTREACHED();
  return net::OK;
}

bool TCPClientSocketBrokered::SetKeepAlive(bool enable, int delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return false;
  }
  return brokered_socket_->SetKeepAlive(enable, delay);
}

bool TCPClientSocketBrokered::SetNoDelay(bool no_delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return false;
  }
  return brokered_socket_->SetNoDelay(no_delay);
}

void TCPClientSocketBrokered::SetBeforeConnectCallback(
    const BeforeConnectCallback& before_connect_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!before_connect_callback_);
  DCHECK(!IsConnected());
  DCHECK(!is_connect_in_progress_);

  before_connect_callback_ = before_connect_callback;
}

int TCPClientSocketBrokered::Connect(net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(liza): add support for reconnecting disconnected socket, or look into
  // removing support for reconnection from TCPClientSocket if it's not needed.
  DCHECK(!callback.is_null());
  // If connecting or already connected, then just return OK.
  if (IsConnected() || is_connect_in_progress_)
    return net::OK;

  is_connect_in_progress_ = true;

  // TODO(https://crbug.com/1321274): Pass in AddressFamily of single IPEndPoint
  client_socket_factory_->BrokerCreateTcpSocket(
      addresses_.begin()->GetFamily(),
      base::BindOnce(&TCPClientSocketBrokered::DidCompleteCreate,
                     brokered_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));

  return net::ERR_IO_PENDING;
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(result, net::ERR_IO_PENDING);

  is_connect_in_progress_ = false;
  // The callback may delete {this}.
  std::move(callback).Run(result);
}

void TCPClientSocketBrokered ::DidCompleteCreate(
    net::CompletionOnceCallback callback,
    network::TransferableSocket socket,
    int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (result != net::OK) {
    std::move(callback).Run(result);
    return;
  }

  // Create an unconnected TCPSocket with the socket fd that was opened in the
  // browser process.
  std::unique_ptr<net::TCPSocket> tcp_socket = std::make_unique<net::TCPSocket>(
      std::move(socket_performance_watcher_), net_log_, source_);
  tcp_socket->AdoptUnconnectedSocket(socket.TakeSocket());

  // TODO(liza): Pass through the NetworkHandle.
  brokered_socket_ = std::make_unique<net::TCPClientSocket>(
      std::move(tcp_socket), addresses_, network_quality_estimator_);
  brokered_socket_->ApplySocketTag(tag_);

  if (before_connect_callback_) {
    int callback_result = before_connect_callback_.Run();
    DCHECK_NE(net::ERR_IO_PENDING, callback_result);
    if (callback_result != net::OK) {
      std::move(callback).Run(callback_result);
      return;
    }
  }

  brokered_socket_->Connect(base::BindOnce(
      &TCPClientSocketBrokered::DidCompleteConnect,
      brokered_weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void TCPClientSocketBrokered::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (brokered_socket_) {
    brokered_socket_->Disconnect();
  }
  is_connect_in_progress_ = false;
}

bool TCPClientSocketBrokered::IsConnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return false;
  }

  return brokered_socket_->IsConnected();
}

bool TCPClientSocketBrokered::IsConnectedAndIdle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return false;
  }
  return brokered_socket_->IsConnectedAndIdle();
}

int TCPClientSocketBrokered::GetPeerAddress(net::IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->GetPeerAddress(std::move(address));
}

int TCPClientSocketBrokered::GetLocalAddress(net::IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->GetLocalAddress(std::move(address));
}

const net::NetLogWithSource& TCPClientSocketBrokered::NetLog() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return NetLog();
  }
  return brokered_socket_->NetLog();
}

bool TCPClientSocketBrokered::WasEverUsed() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return 0;
  }
  return brokered_socket_->GetTotalReceivedBytes();
}

void TCPClientSocketBrokered::ApplySocketTag(const net::SocketTag& tag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    tag_ = tag;
  } else {
    brokered_socket_->ApplySocketTag(tag);
  }
}

int TCPClientSocketBrokered::Read(net::IOBuffer* buf,
                                  int buf_len,
                                  net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->Read(buf, buf_len, std::move(callback));
}

int TCPClientSocketBrokered::ReadIfReady(net::IOBuffer* buf,
                                         int buf_len,
                                         net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->ReadIfReady(buf, buf_len, std::move(callback));
}

int TCPClientSocketBrokered::CancelReadIfReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->Write(std::move(buf), buf_len, std::move(callback),
                                 traffic_annotation);
}

int TCPClientSocketBrokered::SetReceiveBufferSize(int32_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->SetReceiveBufferSize(size);
}

int TCPClientSocketBrokered::SetSendBufferSize(int32_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->SetSendBufferSize(size);
}

}  // namespace network

// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/brokered_tcp_client_socket.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/socket/tcp_client_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/brokered_client_socket_factory.h"
#include "services/network/public/cpp/transferable_socket.h"

namespace network {

BrokeredTcpClientSocket::BrokeredTcpClientSocket(
    const net::AddressList& addresses,
    std::unique_ptr<net::SocketPerformanceWatcher> socket_performance_watcher,
    net::NetworkQualityEstimator* network_quality_estimator,
    net::NetLog* net_log,
    const net::NetLogSource& source,
    BrokeredClientSocketFactory* client_socket_factory)
    : addresses_(addresses),
      socket_performance_watcher_(std::move(socket_performance_watcher)),
      network_quality_estimator_(network_quality_estimator),
      net_log_source_(
          net::NetLogWithSource::Make(net_log, net::NetLogSourceType::SOCKET)),
      client_socket_factory_(client_socket_factory) {
  net_log_source_.BeginEventReferencingSource(
      net::NetLogEventType::BROKERED_SOCKET_ALIVE, source);
}

BrokeredTcpClientSocket::~BrokeredTcpClientSocket() {
  net_log_source_.EndEvent(net::NetLogEventType::BROKERED_SOCKET_ALIVE);
  Disconnect();
}

int BrokeredTcpClientSocket::Bind(const net::IPEndPoint& address) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (IsConnected() || is_connect_in_progress_) {
    // Cannot bind the socket if we are already connected or connecting.
    NOTREACHED_IN_MIGRATION();
    return net::ERR_UNEXPECTED;
  }
  // Since opening a socket must be done via an asynchronous IPC, we will store
  // the bind address and attempt to bind when Connect() is called. Bind() will
  // be done after opening a socket but before actually connecting.
  bind_address_ = std::make_unique<net::IPEndPoint>(address);
  return net::OK;
}

bool BrokeredTcpClientSocket::SetKeepAlive(bool enable, int delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return false;
  }
  return brokered_socket_->SetKeepAlive(enable, delay);
}

bool BrokeredTcpClientSocket::SetNoDelay(bool no_delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return false;
  }
  return brokered_socket_->SetNoDelay(no_delay);
}

void BrokeredTcpClientSocket::SetBeforeConnectCallback(
    const BeforeConnectCallback& before_connect_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!before_connect_callback_);
  DCHECK(!IsConnected());
  DCHECK(!is_connect_in_progress_);

  before_connect_callback_ = before_connect_callback;
}

int BrokeredTcpClientSocket::Connect(net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // TODO(liza): add support for reconnecting disconnected socket, or look into
  // removing support for reconnection from TCPClientSocket if it's not needed.
  DCHECK(!callback.is_null());
  // If connecting or already connected, then just return OK.
  if (IsConnected() || is_connect_in_progress_) {
    return net::OK;
  }

  is_connect_in_progress_ = true;

  net_log_source_.BeginEvent(net::NetLogEventType::BROKERED_CREATE_SOCKET);

  // TODO(crbug.com/40223835): Pass in AddressFamily of single IPEndPoint
  client_socket_factory_->BrokerCreateTcpSocket(
      addresses_.begin()->GetFamily(),
      base::BindOnce(&BrokeredTcpClientSocket::DidCompleteCreate,
                     brokered_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));

  return net::ERR_IO_PENDING;
}

void BrokeredTcpClientSocket::DidCompleteConnect(
    net::CompletionOnceCallback callback,
    int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(result, net::ERR_IO_PENDING);

  is_connect_in_progress_ = false;
  // The callback may delete {this}.
  std::move(callback).Run(result);
}

void BrokeredTcpClientSocket ::DidCompleteCreate(
    net::CompletionOnceCallback callback,
    network::TransferableSocket socket,
    int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  net_log_source_.EndEventWithNetErrorCode(
      net::NetLogEventType::BROKERED_CREATE_SOCKET, result);
  if (result != net::OK) {
    std::move(callback).Run(result);
    return;
  }

  // Create an unconnected TCPSocket with the socket fd that was opened in the
  // browser process.
  std::unique_ptr<net::TCPSocket> tcp_socket = net::TCPSocket::Create(
      std::move(socket_performance_watcher_), net_log_source_);
  tcp_socket->AdoptUnconnectedSocket(socket.TakeSocket());

  // If Bind() was called prior to connecting, attempt to bind now that a socket
  // has been opened.
  if (bind_address_) {
    int bind_result = tcp_socket->Bind(*bind_address_);
    if (bind_result != net::OK) {
      tcp_socket->Close();
      std::move(callback).Run(bind_result);
      return;
    }
  }
  // TODO(liza): Pass through the NetworkHandle.
  brokered_socket_ = std::make_unique<net::TCPClientSocket>(
      std::move(tcp_socket), addresses_, std::move(bind_address_),
      network_quality_estimator_);

  brokered_socket_->ApplySocketTag(tag_);
  if (before_connect_callback_) {
    int callback_result = before_connect_callback_.Run();
    DCHECK_NE(net::ERR_IO_PENDING, callback_result);
    if (callback_result != net::OK) {
      net_log_source_.AddEventWithNetErrorCode(net::NetLogEventType::FAILED,
                                               callback_result);
      std::move(callback).Run(callback_result);
      return;
    }
  }

  auto split_connect_callback = base::SplitOnceCallback(std::move(callback));
  int connect_result = brokered_socket_->Connect(
      base::BindOnce(&BrokeredTcpClientSocket::DidCompleteConnect,
                     brokered_weak_ptr_factory_.GetWeakPtr(),
                     std::move(split_connect_callback.first)));
  if (connect_result != net::ERR_IO_PENDING) {
    DidCompleteConnect(std::move(split_connect_callback.second),
                       connect_result);
  }
}

void BrokeredTcpClientSocket::Disconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (brokered_socket_) {
    brokered_socket_->Disconnect();
  }
  bind_address_.reset();
  is_connect_in_progress_ = false;
}

bool BrokeredTcpClientSocket::IsConnected() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return false;
  }

  return brokered_socket_->IsConnected();
}

bool BrokeredTcpClientSocket::IsConnectedAndIdle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return false;
  }
  return brokered_socket_->IsConnectedAndIdle();
}

int BrokeredTcpClientSocket::GetPeerAddress(net::IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->GetPeerAddress(std::move(address));
}

int BrokeredTcpClientSocket::GetLocalAddress(net::IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->GetLocalAddress(std::move(address));
}

const net::NetLogWithSource& BrokeredTcpClientSocket::NetLog() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return net_log_source_;
}

bool BrokeredTcpClientSocket::WasEverUsed() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return false;
  }
  return brokered_socket_->WasEverUsed();
}

net::NextProto BrokeredTcpClientSocket::GetNegotiatedProtocol() const {
  return net::kProtoUnknown;
}

bool BrokeredTcpClientSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  return false;
}

int64_t BrokeredTcpClientSocket::GetTotalReceivedBytes() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return 0;
  }
  return brokered_socket_->GetTotalReceivedBytes();
}

void BrokeredTcpClientSocket::ApplySocketTag(const net::SocketTag& tag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    tag_ = tag;
  } else {
    brokered_socket_->ApplySocketTag(tag);
  }
}

int BrokeredTcpClientSocket::Read(net::IOBuffer* buf,
                                  int buf_len,
                                  net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->Read(buf, buf_len, std::move(callback));
}

int BrokeredTcpClientSocket::ReadIfReady(net::IOBuffer* buf,
                                         int buf_len,
                                         net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->ReadIfReady(buf, buf_len, std::move(callback));
}

int BrokeredTcpClientSocket::CancelReadIfReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->CancelReadIfReady();
}

int BrokeredTcpClientSocket::Write(
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

int BrokeredTcpClientSocket::SetReceiveBufferSize(int32_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->SetReceiveBufferSize(size);
}

int BrokeredTcpClientSocket::SetSendBufferSize(int32_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!brokered_socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return brokered_socket_->SetSendBufferSize(size);
}

}  // namespace network

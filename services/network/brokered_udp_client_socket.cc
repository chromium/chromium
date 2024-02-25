// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/brokered_udp_client_socket.h"

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "mojo/public/cpp/platform/platform_handle.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_handle.h"
#include "net/log/net_log_source.h"
#include "net/nqe/network_quality_estimator.h"
#include "net/socket/datagram_client_socket.h"
#include "net/socket/datagram_socket.h"
#include "net/socket/socket_tag.h"
#include "net/socket/stream_socket.h"
#include "net/socket/udp_client_socket.h"
#include "net/socket/udp_socket.h"
#include "net/socket/udp_socket_global_limits.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/brokered_client_socket_factory.h"
#include "services/network/public/cpp/transferable_socket.h"

namespace network {

BrokeredUdpClientSocket::BrokeredUdpClientSocket(
    net::DatagramSocket::BindType bind_type,
    net::NetLog* net_log,
    const net::NetLogSource& source,
    BrokeredClientSocketFactory* client_socket_factory,
    net::handles::NetworkHandle network)
    : bind_type_(bind_type),
      network_(network),
      net_log_source_(
          net::NetLogWithSource::Make(net_log, net::NetLogSourceType::SOCKET)),
      client_socket_factory_(client_socket_factory) {
  net_log_source_.BeginEventReferencingSource(
      net::NetLogEventType::BROKERED_SOCKET_ALIVE, source);
}

BrokeredUdpClientSocket::~BrokeredUdpClientSocket() {
  net_log_source_.EndEvent(net::NetLogEventType::BROKERED_SOCKET_ALIVE);
}

int BrokeredUdpClientSocket::Connect(const net::IPEndPoint& address) {
  if (!broker_helper_.ShouldBroker(address.address())) {
    return ConnectInternal(address);
  }
  // Brokered sockets can only support asynchronous connections so this does not
  // need to be implemented. However, this path can still be hit if the sandbox
  // is enabled and a caller attempts to call a synchronous Connect. Callers are
  // expected to handle Connect failures themselves so we just return
  // ERR_NOT_IMPLEMENTED.
  return net::ERR_NOT_IMPLEMENTED;
}

int BrokeredUdpClientSocket::ConnectUsingNetwork(
    net::handles::NetworkHandle network,
    const net::IPEndPoint& address) {
  // NetworkHandles are not supported on Windows, so this method and the
  // following Connect*Network() methods don't need to return anything.
  return net::ERR_NOT_IMPLEMENTED;
}

int BrokeredUdpClientSocket::ConnectUsingDefaultNetwork(
    const net::IPEndPoint& address) {
  return net::ERR_NOT_IMPLEMENTED;
}

int BrokeredUdpClientSocket::ConnectAsync(
    const net::IPEndPoint& address,
    net::CompletionOnceCallback callback) {
  return ConnectAsyncInternal(address, std::move(callback));
}

int BrokeredUdpClientSocket::ConnectUsingNetworkAsync(
    net::handles::NetworkHandle network,
    const net::IPEndPoint& address,
    net::CompletionOnceCallback callback) {
  return net::ERR_NOT_IMPLEMENTED;
}

int BrokeredUdpClientSocket::ConnectUsingDefaultNetworkAsync(
    const net::IPEndPoint& address,
    net::CompletionOnceCallback callback) {
  return net::ERR_NOT_IMPLEMENTED;
}

int BrokeredUdpClientSocket::ConnectAsyncInternal(
    const net::IPEndPoint& address,
    net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(callback);
  DCHECK(!socket_);
  CHECK(!connect_called_);
  connect_called_ = true;
  if (!broker_helper_.ShouldBroker(address.address())) {
    return DidCompleteCreate(/*should_broker=*/false, address,
                             std::move(callback), network::TransferableSocket(),
                             net::OK);
  }
  net_log_source_.BeginEvent(net::NetLogEventType::BROKERED_CREATE_SOCKET);
  client_socket_factory_->BrokerCreateUdpSocket(
      address.GetFamily(),
      base::BindOnce(
          base::IgnoreResult(&BrokeredUdpClientSocket::DidCompleteCreate),
          brokered_weak_ptr_factory_.GetWeakPtr(), /*should_broker=*/true,
          address, std::move(callback)));
  return net::ERR_IO_PENDING;
}

int BrokeredUdpClientSocket::ConnectInternal(const net::IPEndPoint& address) {
  socket_ = std::make_unique<net::UDPClientSocket>(bind_type_, net_log_source_,
                                                   network_);

  // These options must be set before opening a socket or adopting an opened
  // socket.
  if (use_non_blocking_io_) {
    socket_->UseNonBlockingIO();
  }
  if (recv_optimization_) {
    socket_->EnableRecvOptimization();
  }

  int set_multicast_rv = socket_->SetMulticastInterface(interface_index_);
  if (set_multicast_rv != net::OK) {
    return set_multicast_rv;
  }
  socket_->ApplySocketTag(tag_);
  socket_->SetMsgConfirm(set_msg_confirm_);

  int connect_rv = socket_->Connect(address);
  return connect_rv;
}

int BrokeredUdpClientSocket::DidCompleteCreate(
    bool should_broker,
    const net::IPEndPoint& address,
    net::CompletionOnceCallback callback,
    network::TransferableSocket socket,
    int result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (should_broker) {
    net_log_source_.EndEventWithNetErrorCode(
        net::NetLogEventType::BROKERED_CREATE_SOCKET, result);
    if (result != net::OK) {
      std::move(callback).Run(result);
      return result;
    }
  }
  socket_ = std::make_unique<net::UDPClientSocket>(bind_type_, net_log_source_,
                                                   network_);
  // These options must be set before opening a socket or adopting an opened
  // socket.
  if (use_non_blocking_io_) {
    socket_->UseNonBlockingIO();
  }
  if (recv_optimization_) {
    socket_->EnableRecvOptimization();
  }

  if (should_broker) {
    int adopt_socket_rv =
        socket_->AdoptOpenedSocket(address.GetFamily(), socket.TakeSocket());
    if (adopt_socket_rv != net::OK) {
      Close();
      std::move(callback).Run(adopt_socket_rv);
      return adopt_socket_rv;
    }
  }

  int set_multicast_rv = socket_->SetMulticastInterface(interface_index_);
  if (set_multicast_rv != net::OK) {
    if (should_broker) {
      std::move(callback).Run(set_multicast_rv);
    }
    return set_multicast_rv;
  }
  socket_->ApplySocketTag(tag_);
  socket_->SetMsgConfirm(set_msg_confirm_);

  auto split_callback = base::SplitOnceCallback(std::move(callback));
  const int connect_rv =
      socket_->ConnectAsync(address, std::move(split_callback.first));
  if (should_broker && connect_rv != net::ERR_IO_PENDING) {
    std::move(split_callback.second).Run(connect_rv);
  }
  return connect_rv;
}

net::handles::NetworkHandle BrokeredUdpClientSocket::GetBoundNetwork() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!socket_) {
    return net::handles::kInvalidNetworkHandle;
  }
  return socket_->GetBoundNetwork();
}

void BrokeredUdpClientSocket::ApplySocketTag(const net::SocketTag& tag) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (socket_) {
    socket_->ApplySocketTag(tag);
  }
  tag_ = tag;
}

int BrokeredUdpClientSocket::Read(net::IOBuffer* buf,
                                  int buf_len,
                                  net::CompletionOnceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return socket_->Read(buf, buf_len, std::move(callback));
}

int BrokeredUdpClientSocket::Write(
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback callback,
    const net::NetworkTrafficAnnotationTag& traffic_annotation) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return socket_->Write(buf, buf_len, std::move(callback), traffic_annotation);
}

void BrokeredUdpClientSocket::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  socket_.reset();
  brokered_weak_ptr_factory_.InvalidateWeakPtrs();
}

int BrokeredUdpClientSocket::GetPeerAddress(net::IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return socket_->GetPeerAddress(address);
}

int BrokeredUdpClientSocket::GetLocalAddress(net::IPEndPoint* address) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!socket_) {
    return net::ERR_SOCKET_NOT_CONNECTED;
  }
  return socket_->GetLocalAddress(address);
}

int BrokeredUdpClientSocket::SetReceiveBufferSize(int32_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(socket_);
  return socket_->SetReceiveBufferSize(size);
}

int BrokeredUdpClientSocket::SetSendBufferSize(int32_t size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(socket_);
  return socket_->SetSendBufferSize(size);
}

int BrokeredUdpClientSocket::SetDoNotFragment() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(socket_);
  return socket_->SetDoNotFragment();
}

int BrokeredUdpClientSocket::SetRecvTos() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(socket_);
  return socket_->SetRecvTos();
}

int BrokeredUdpClientSocket::SetTos(net::DiffServCodePoint dscp,
                                    net::EcnCodePoint ecn) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(socket_);
  return socket_->SetTos(dscp, ecn);
}

void BrokeredUdpClientSocket::SetMsgConfirm(bool confirm) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  set_msg_confirm_ = confirm;
}

const net::NetLogWithSource& BrokeredUdpClientSocket::NetLog() const {
  return net_log_source_;
}

void BrokeredUdpClientSocket::UseNonBlockingIO() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  use_non_blocking_io_ = true;
}

int BrokeredUdpClientSocket::SetMulticastInterface(uint32_t interface_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!socket_) {
    interface_index_ = interface_index;
    return net::OK;
  }
  return socket_->SetMulticastInterface(interface_index);
}

void BrokeredUdpClientSocket::EnableRecvOptimization() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  recv_optimization_ = true;
}

void BrokeredUdpClientSocket::SetIOSNetworkServiceType(
    int ios_network_service_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  socket_->SetIOSNetworkServiceType(ios_network_service_type);
}

net::DscpAndEcn BrokeredUdpClientSocket::GetLastTos() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return socket_->GetLastTos();
}

}  // namespace network

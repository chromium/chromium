// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/api/socket/udp_socket.h"

#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/ranges/algorithm.h"
#include "extensions/browser/api/api_resource.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/datagram_socket.h"
#include "net/socket/udp_client_socket.h"

namespace extensions {

static base::LazyInstance<BrowserContextKeyedAPIFactory<
    ApiResourceManager<ResumableUDPSocket>>>::DestructorAtExit g_factory =
    LAZY_INSTANCE_INITIALIZER;

// static
template <>
BrowserContextKeyedAPIFactory<ApiResourceManager<ResumableUDPSocket> >*
ApiResourceManager<ResumableUDPSocket>::GetFactoryInstance() {
  return g_factory.Pointer();
}

UDPSocket::UDPSocket(
    mojo::PendingRemote<network::mojom::UDPSocket> socket,
    mojo::PendingReceiver<network::mojom::UDPSocketListener> listener_receiver,
    const std::string& owner_extension_id)
    : Socket(owner_extension_id),
      socket_(std::move(socket)),
      socket_options_(network::mojom::UDPSocketOptions::New()),
      is_bound_(false) {
  listener_receiver_.Bind(std::move(listener_receiver));
}

UDPSocket::~UDPSocket() {
  Disconnect(true /* socket_destroying */);
}

void UDPSocket::Connect(const net::AddressList& address,
                        net::CompletionOnceCallback callback) {
  if (IsConnectedOrBound()) {
    std::move(callback).Run(net::ERR_CONNECTION_FAILED);
    return;
  }
  // UDP API only connects to the first address received from DNS so
  // connection may not work even if other addresses are reachable.
  const net::IPEndPoint& ip_end_point = address.front();
  socket_->Connect(
      ip_end_point, std::move(socket_options_),
      base::BindOnce(&UDPSocket::OnConnectCompleted, base::Unretained(this),
                     std::move(callback), ip_end_point));
}

void UDPSocket::Bind(const std::string& address,
                     uint16_t port,
                     net::CompletionOnceCallback callback) {
  if (IsConnectedOrBound()) {
    std::move(callback).Run(net::ERR_CONNECTION_FAILED);
    return;
  }

  net::IPEndPoint ip_end_point;
  if (!StringAndPortToIPEndPoint(address, port, &ip_end_point)) {
    std::move(callback).Run(net::ERR_INVALID_ARGUMENT);
    return;
  }
  socket_->Bind(ip_end_point, std::move(socket_options_),
                base::BindOnce(&UDPSocket::OnBindCompleted,
                               base::Unretained(this), std::move(callback)));
}

void UDPSocket::Disconnect(bool socket_destroying) {
  is_connected_ = false;
  is_bound_ = false;
  socket_->Close();
  local_addr_ = std::nullopt;
  peer_addr_ = std::nullopt;
  read_callback_.Reset();
  // TODO(devlin): Should we do this for all callbacks?
  if (!recv_from_callback_.is_null()) {
    std::move(recv_from_callback_)
        .Run(net::ERR_CONNECTION_CLOSED, nullptr, true /* socket_destroying */,
             std::string(), 0);
  }
  multicast_groups_.clear();
}

void UDPSocket::Read(int count, ReadCompletionCallback callback) {
  DCHECK(!callback.is_null());

  if (!read_callback_.is_null()) {
    std::move(callback).Run(net::ERR_IO_PENDING, nullptr,
                            false /* socket_destroying */);
    return;
  }

  if (count < 0) {
    std::move(callback).Run(net::ERR_INVALID_ARGUMENT, nullptr,
                            false /* socket_destroying */);
    return;
  }

  if (!IsConnected()) {
    std::move(callback).Run(net::ERR_SOCKET_NOT_CONNECTED, nullptr,
                            false /* socket_destroying */);
    return;
  }

  read_callback_ = std::move(callback);
  socket_->ReceiveMoreWithBufferSize(1, count);
  return;
}

int UDPSocket::WriteImpl(net::IOBuffer* io_buffer,
                         int io_buffer_size,
                         net::CompletionOnceCallback callback) {
  if (!IsConnected())
    return net::ERR_SOCKET_NOT_CONNECTED;
  base::span<const uint8_t> data(
      reinterpret_cast<const uint8_t*>(io_buffer->data()),
      static_cast<size_t>(io_buffer_size));
  socket_->Send(
      data,
      net::MutableNetworkTrafficAnnotationTag(
          Socket::GetNetworkTrafficAnnotationTag()),
      base::BindOnce(&UDPSocket::OnWriteCompleted, base::Unretained(this),
                     std::move(callback), data.size()));
  return net::ERR_IO_PENDING;
}

void UDPSocket::RecvFrom(int count, RecvFromCompletionCallback callback) {
  DCHECK(!callback.is_null());

  if (!recv_from_callback_.is_null()) {
    std::move(callback).Run(net::ERR_IO_PENDING, nullptr,
                            false /* socket_destroying */, std::string(), 0);
    return;
  }

  if (count < 0) {
    std::move(callback).Run(net::ERR_INVALID_ARGUMENT, nullptr,
                            false /* socket_destroying */, std::string(), 0);
    return;
  }

  if (!is_bound_) {
    std::move(callback).Run(net::ERR_SOCKET_NOT_CONNECTED, nullptr,
                            false /* socket_destroying */, std::string(), 0);
    return;
  }

  recv_from_callback_ = std::move(callback);
  socket_->ReceiveMoreWithBufferSize(1, count);
}

void UDPSocket::SendTo(scoped_refptr<net::IOBuffer> io_buffer,
                       int byte_count,
                       const net::IPEndPoint& address,
                       net::CompletionOnceCallback callback) {
  DCHECK(!callback.is_null());

  if (!is_bound_) {
    std::move(callback).Run(net::ERR_SOCKET_NOT_CONNECTED);
    return;
  }

  base::span<const uint8_t> data(
      reinterpret_cast<const uint8_t*>(io_buffer->data()),
      static_cast<size_t>(byte_count));
  socket_->SendTo(
      address, data,
      net::MutableNetworkTrafficAnnotationTag(
          Socket::GetNetworkTrafficAnnotationTag()),
      base::BindOnce(&UDPSocket::OnSendToCompleted, base::Unretained(this),
                     std::move(callback), data.size()));
}

bool UDPSocket::IsConnected() {
  return is_connected_;
}

bool UDPSocket::GetPeerAddress(net::IPEndPoint* address) {
  if (!IsConnected())
    return false;
  if (!peer_addr_)
    return false;
  *address = peer_addr_.value();
  return true;
}

bool UDPSocket::GetLocalAddress(net::IPEndPoint* address) {
  if (!IsConnectedOrBound())
    return false;
  if (!local_addr_)
    return false;
  *address = local_addr_.value();
  return true;
}

Socket::SocketType UDPSocket::GetSocketType() const { return Socket::TYPE_UDP; }

bool UDPSocket::IsConnectedOrBound() const {
  return is_connected_ || is_bound_;
}

void UDPSocket::OnReceived(int32_t result,
                           const std::optional<net::IPEndPoint>& src_addr,
                           std::optional<base::span<const uint8_t>> data) {
  DCHECK(!recv_from_callback_.is_null() || !read_callback_.is_null());

  std::string ip;
  uint16_t port = 0;
  if (result != net::OK) {
    if (!read_callback_.is_null()) {
      std::move(read_callback_)
          .Run(result, nullptr, false /* socket_destroying */);
      return;
    }
    std::move(recv_from_callback_)
        .Run(result, nullptr, false /* socket_destroying */, ip, port);
    return;
  }

  auto io_buffer =
      base::MakeRefCounted<net::IOBufferWithSize>(data.value().size());
  memcpy(io_buffer->data(), data.value().data(), data.value().size());

  if (!read_callback_.is_null()) {
    std::move(read_callback_)
        .Run(data.value().size(), io_buffer, false /* socket_destroying */);
    return;
  }

  IPEndPointToStringAndPort(src_addr.value(), &ip, &port);
  std::move(recv_from_callback_)
      .Run(data.value().size(), io_buffer, false /* socket_destroying */, ip,
           port);
}

void UDPSocket::OnConnectCompleted(
    net::CompletionOnceCallback callback,
    const net::IPEndPoint& remote_addr,
    int result,
    const std::optional<net::IPEndPoint>& local_addr) {
  if (result != net::OK) {
    std::move(callback).Run(result);
    return;
  }
  local_addr_ = local_addr;
  peer_addr_ = remote_addr;
  is_connected_ = true;
  std::move(callback).Run(result);
}

void UDPSocket::OnBindCompleted(
    net::CompletionOnceCallback callback,
    int result,
    const std::optional<net::IPEndPoint>& local_addr) {
  if (result != net::OK) {
    std::move(callback).Run(result);
    return;
  }
  local_addr_ = local_addr;
  is_bound_ = true;
  std::move(callback).Run(result);
}

void UDPSocket::OnSendToCompleted(net::CompletionOnceCallback callback,
                                  size_t byte_count,
                                  int result) {
  if (result == net::OK) {
    std::move(callback).Run(byte_count);
    return;
  }
  std::move(callback).Run(result);
}

void UDPSocket::OnWriteCompleted(net::CompletionOnceCallback callback,
                                 size_t byte_count,
                                 int result) {
  if (result == net::OK) {
    std::move(callback).Run(byte_count);
    return;
  }
  std::move(callback).Run(result);
}

void UDPSocket::OnJoinGroupCompleted(net::CompletionOnceCallback callback,
                                     const std::string& normalized_address,
                                     int result) {
  if (result == net::OK)
    multicast_groups_.push_back(normalized_address);
  std::move(callback).Run(result);
}

void UDPSocket::OnLeaveGroupCompleted(net::CompletionOnceCallback callback,
                                      const std::string& normalized_address,
                                      int result) {
  if (result == net::OK) {
    std::erase(multicast_groups_, normalized_address);
  }

  std::move(callback).Run(result);
}

void UDPSocket::JoinGroup(const std::string& address,
                          net::CompletionOnceCallback callback) {
  net::IPAddress ip;
  if (!ip.AssignFromIPLiteral(address)) {
    std::move(callback).Run(net::ERR_ADDRESS_INVALID);
    return;
  }

  std::string normalized_address = ip.ToString();
  if (base::Contains(multicast_groups_, normalized_address)) {
    std::move(callback).Run(net::ERR_ADDRESS_INVALID);
    return;
  }

  socket_->JoinGroup(
      ip,
      base::BindOnce(&UDPSocket::OnJoinGroupCompleted, base::Unretained(this),
                     std::move(callback), normalized_address));
}

void UDPSocket::LeaveGroup(const std::string& address,
                           net::CompletionOnceCallback callback) {
  net::IPAddress ip;
  if (!ip.AssignFromIPLiteral(address)) {
    std::move(callback).Run(net::ERR_ADDRESS_INVALID);
    return;
  }

  std::string normalized_address = ip.ToString();
  if (!base::Contains(multicast_groups_, normalized_address)) {
    std::move(callback).Run(net::ERR_ADDRESS_INVALID);
    return;
  }

  socket_->LeaveGroup(
      ip,
      base::BindOnce(&UDPSocket::OnLeaveGroupCompleted, base::Unretained(this),
                     std::move(callback), normalized_address));
}

int UDPSocket::SetMulticastTimeToLive(int ttl) {
  if (!socket_options_)
    return net::ERR_SOCKET_IS_CONNECTED;
  if (ttl < 0 || ttl > 255)
    return net::ERR_INVALID_ARGUMENT;
  socket_options_->multicast_time_to_live = ttl;
  return net::OK;
}

int UDPSocket::SetMulticastLoopbackMode(bool loopback) {
  if (!socket_options_)
    return net::ERR_SOCKET_IS_CONNECTED;
  socket_options_->multicast_loopback_mode = loopback;
  return net::OK;
}

void UDPSocket::SetBroadcast(bool enabled,
                             net::CompletionOnceCallback callback) {
  if (!is_bound_) {
    std::move(callback).Run(net::ERR_SOCKET_NOT_CONNECTED);
    return;
  }
  socket_->SetBroadcast(enabled, std::move(callback));
}

const std::vector<std::string>& UDPSocket::GetJoinedGroups() const {
  return multicast_groups_;
}

ResumableUDPSocket::ResumableUDPSocket(
    mojo::PendingRemote<network::mojom::UDPSocket> socket,
    mojo::PendingReceiver<network::mojom::UDPSocketListener> listener_receiver,
    const std::string& owner_extension_id)
    : UDPSocket(std::move(socket),
                std::move(listener_receiver),
                owner_extension_id),
      persistent_(false),
      buffer_size_(0),
      paused_(false) {}

bool ResumableUDPSocket::IsPersistent() const { return persistent(); }

}  // namespace extensions

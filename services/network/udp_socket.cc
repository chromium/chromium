// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/udp_socket.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/ranges.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/socket/udp_socket.h"

namespace network {

namespace {

const uint32_t kMaxReadSize = 64 * 1024;
// The limit on data length for a UDP packet is 65,507 for IPv4 and 65,535 for
// IPv6.
const uint32_t kMaxPacketSize = kMaxReadSize - 1;

int ClampUDPBufferSize(int requested_buffer_size) {
  constexpr int kMinBufferSize = 0;
  constexpr int kMaxBufferSize = 128 * 1024;
  return base::ClampToRange(requested_buffer_size, kMinBufferSize,
                            kMaxBufferSize);
}

class SocketWrapperImpl : public UDPSocket::SocketWrapper {
 public:
  SocketWrapperImpl(net::DatagramSocket::BindType bind_type,
                    net::NetLog* net_log,
                    const net::NetLogSource& source)
      : socket_(bind_type, net_log, source) {}
  ~SocketWrapperImpl() override {}

  int Connect(const net::IPEndPoint& remote_addr,
              mojom::UDPSocketOptionsPtr options,
              net::IPEndPoint* local_addr_out) override {
    int result = socket_.Open(remote_addr.GetFamily());
    if (result == net::OK)
      result = ConfigureOptions(std::move(options));
    if (result == net::OK)
      result = socket_.Connect(remote_addr);
    if (result == net::OK)
      result = socket_.GetLocalAddress(local_addr_out);

    if (result != net::OK)
      socket_.Close();
    return result;
  }
  int Bind(const net::IPEndPoint& local_addr,
           mojom::UDPSocketOptionsPtr options,
           net::IPEndPoint* local_addr_out) override {
    int result = socket_.Open(local_addr.GetFamily());
    if (result == net::OK)
      result = ConfigureOptions(std::move(options));
    if (result == net::OK)
      result = socket_.Bind(local_addr);
    if (result == net::OK)
      result = socket_.GetLocalAddress(local_addr_out);

    if (result != net::OK)
      socket_.Close();
    return result;
  }
  int SendTo(
      net::IOBuffer* buf,
      int buf_len,
      const net::IPEndPoint& dest_addr,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    return socket_.SendTo(buf, buf_len, dest_addr, std::move(callback));
  }
  int SetBroadcast(bool broadcast) override {
    return socket_.SetBroadcast(broadcast);
  }
  int SetSendBufferSize(int send_buffer_size) override {
    return socket_.SetSendBufferSize(ClampUDPBufferSize(send_buffer_size));
  }
  int SetReceiveBufferSize(int receive_buffer_size) override {
    return socket_.SetReceiveBufferSize(
        ClampUDPBufferSize(receive_buffer_size));
  }
  int JoinGroup(const net::IPAddress& group_address) override {
    return socket_.JoinGroup(group_address);
  }
  int LeaveGroup(const net::IPAddress& group_address) override {
    return socket_.LeaveGroup(group_address);
  }
  int Write(
      net::IOBuffer* buf,
      int buf_len,
      net::CompletionOnceCallback callback,
      const net::NetworkTrafficAnnotationTag& traffic_annotation) override {
    return socket_.Write(buf, buf_len, std::move(callback), traffic_annotation);
  }
  int RecvFrom(net::IOBuffer* buf,
               int buf_len,
               net::IPEndPoint* address,
               net::CompletionOnceCallback callback) override {
    return socket_.RecvFrom(buf, buf_len, address, std::move(callback));
  }

 private:
  int ConfigureOptions(mojom::UDPSocketOptionsPtr options) {
    if (!options)
      return net::OK;
    int result = net::OK;
    if (options->allow_address_reuse)
      result = socket_.AllowAddressReuse();
    if (result == net::OK && options->allow_address_sharing_for_multicast)
      result = socket_.AllowAddressSharingForMulticast();
    if (result == net::OK && options->allow_broadcast)
      result = socket_.SetBroadcast(true);
    if (result == net::OK && options->multicast_interface != 0)
      result = socket_.SetMulticastInterface(options->multicast_interface);
    if (result == net::OK && !options->multicast_loopback_mode) {
      result =
          socket_.SetMulticastLoopbackMode(options->multicast_loopback_mode);
    }
    if (result == net::OK && options->multicast_time_to_live != 1) {
      result = socket_.SetMulticastTimeToLive(
          base::saturated_cast<int32_t>(options->multicast_time_to_live));
    }
    if (result == net::OK && options->receive_buffer_size != 0) {
      result = socket_.SetReceiveBufferSize(
          ClampUDPBufferSize(options->receive_buffer_size));
    }
    if (result == net::OK && options->send_buffer_size != 0) {
      result = socket_.SetSendBufferSize(
          ClampUDPBufferSize(options->send_buffer_size));
    }
    return result;
  }

  net::UDPSocket socket_;

  DISALLOW_COPY_AND_ASSIGN(SocketWrapperImpl);
};

}  // namespace

UDPSocket::PendingSendRequest::PendingSendRequest() {}

UDPSocket::PendingSendRequest::~PendingSendRequest() {}

UDPSocket::UDPSocket(mojo::PendingRemote<mojom::UDPSocketListener> listener,
                     net::NetLog* net_log)
    : net_log_(net_log),
      is_bound_(false),
      is_connected_(false),
      listener_(std::move(listener)),
      remaining_recv_slots_(0) {}

UDPSocket::~UDPSocket() {}

void UDPSocket::Connect(const net::IPEndPoint& remote_addr,
                        mojom::UDPSocketOptionsPtr options,
                        ConnectCallback callback) {
  if (IsConnectedOrBound()) {
    std::move(callback).Run(net::ERR_SOCKET_IS_CONNECTED, base::nullopt);
    return;
  }
  DCHECK(!wrapped_socket_);
  wrapped_socket_ = CreateSocketWrapper();
  net::IPEndPoint local_addr_out;
  int result = wrapped_socket_->Connect(remote_addr, std::move(options),
                                        &local_addr_out);
  if (result != net::OK) {
    wrapped_socket_.reset();
    std::move(callback).Run(result, base::nullopt);
    return;
  }
  is_connected_ = true;
  std::move(callback).Run(result, local_addr_out);
}

void UDPSocket::Bind(const net::IPEndPoint& local_addr,
                     mojom::UDPSocketOptionsPtr options,
                     BindCallback callback) {
  if (IsConnectedOrBound()) {
    std::move(callback).Run(net::ERR_SOCKET_IS_CONNECTED, base::nullopt);
    return;
  }
  DCHECK(!wrapped_socket_);
  wrapped_socket_ = CreateSocketWrapper();
  net::IPEndPoint local_addr_out;
  int result =
      wrapped_socket_->Bind(local_addr, std::move(options), &local_addr_out);
  if (result != net::OK) {
    wrapped_socket_.reset();
    std::move(callback).Run(result, base::nullopt);
    return;
  }
  is_bound_ = true;
  std::move(callback).Run(result, local_addr_out);
}

void UDPSocket::SetBroadcast(bool broadcast, SetBroadcastCallback callback) {
  if (!is_bound_) {
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }
  int net_result = wrapped_socket_->SetBroadcast(broadcast);
  std::move(callback).Run(net_result);
}

void UDPSocket::SetSendBufferSize(int32_t send_buffer_size,
                                  SetSendBufferSizeCallback callback) {
  if (!is_bound_) {
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }
  int net_result = wrapped_socket_->SetSendBufferSize(send_buffer_size);
  std::move(callback).Run(net_result);
}

void UDPSocket::SetReceiveBufferSize(int32_t receive_buffer_size,
                                     SetSendBufferSizeCallback callback) {
  if (!is_bound_) {
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }
  int net_result = wrapped_socket_->SetReceiveBufferSize(receive_buffer_size);
  std::move(callback).Run(net_result);
}

void UDPSocket::JoinGroup(const net::IPAddress& group_address,
                          JoinGroupCallback callback) {
  if (!is_bound_) {
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }
  int net_result = wrapped_socket_->JoinGroup(group_address);
  std::move(callback).Run(net_result);
}

void UDPSocket::LeaveGroup(const net::IPAddress& group_address,
                           LeaveGroupCallback callback) {
  if (!is_bound_) {
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }
  int net_result = wrapped_socket_->LeaveGroup(group_address);
  std::move(callback).Run(net_result);
}

void UDPSocket::ReceiveMore(uint32_t num_additional_datagrams) {
  ReceiveMoreWithBufferSize(num_additional_datagrams, kMaxReadSize);
}

void UDPSocket::ReceiveMoreWithBufferSize(uint32_t num_additional_datagrams,
                                          uint32_t buffer_size) {
  if (!listener_)
    return;
  if (!IsConnectedOrBound()) {
    listener_->OnReceived(net::ERR_UNEXPECTED, base::nullopt, base::nullopt);
    return;
  }
  if (num_additional_datagrams == 0)
    return;
  // Check for overflow.
  if (!base::CheckAdd(remaining_recv_slots_, num_additional_datagrams)
           .AssignIfValid(&remaining_recv_slots_)) {
    return;
  }
  if (!recvfrom_buffer_) {
    DCHECK_EQ(num_additional_datagrams, remaining_recv_slots_);
    DoRecvFrom(std::min(buffer_size, kMaxReadSize));
  }
}

void UDPSocket::SendTo(
    const net::IPEndPoint& dest_addr,
    base::span<const uint8_t> data,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    SendToCallback callback) {
  if (!is_bound_) {
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }
  DoSendToOrWrite(
      &dest_addr, data,
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(callback));
}

void UDPSocket::Send(
    base::span<const uint8_t> data,
    const net::MutableNetworkTrafficAnnotationTag& traffic_annotation,
    SendCallback callback) {
  if (!is_connected_) {
    std::move(callback).Run(net::ERR_UNEXPECTED);
    return;
  }
  DoSendToOrWrite(
      nullptr, data,
      static_cast<net::NetworkTrafficAnnotationTag>(traffic_annotation),
      std::move(callback));
}

void UDPSocket::Close() {
  if (!IsConnectedOrBound()) {
    return;
  }
  is_connected_ = false;
  is_bound_ = false;
  recvfrom_buffer_ = nullptr;
  send_callback_.Reset();
  send_buffer_ = nullptr;
  remaining_recv_slots_ = 0;
  wrapped_socket_.reset();
}

std::unique_ptr<UDPSocket::SocketWrapper> UDPSocket::CreateSocketWrapper()
    const {
  return std::make_unique<SocketWrapperImpl>(net::DatagramSocket::RANDOM_BIND,
                                             nullptr, net::NetLogSource());
}

bool UDPSocket::IsConnectedOrBound() const {
  return is_connected_ || is_bound_;
}

void UDPSocket::DoRecvFrom(uint32_t buffer_size) {
  DCHECK(listener_);
  DCHECK(!recvfrom_buffer_);
  DCHECK_GT(remaining_recv_slots_, 0u);
  DCHECK_GE(kMaxReadSize, buffer_size);

  recvfrom_buffer_ =
      base::MakeRefCounted<net::IOBuffer>(static_cast<size_t>(buffer_size));

  // base::Unretained(this) is safe because socket is owned by |this|.
  int net_result = wrapped_socket_->RecvFrom(
      recvfrom_buffer_.get(), buffer_size, &recvfrom_address_,
      base::BindRepeating(&UDPSocket::OnRecvFromCompleted,
                          base::Unretained(this), buffer_size));
  if (net_result != net::ERR_IO_PENDING)
    OnRecvFromCompleted(buffer_size, net_result);
}

void UDPSocket::DoSendToOrWrite(
    const net::IPEndPoint* dest_addr,
    const base::span<const uint8_t>& data,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    SendToCallback callback) {
  if (pending_send_requests_.size() >= kMaxPendingSendRequests) {
    std::move(callback).Run(net::ERR_INSUFFICIENT_RESOURCES);
    return;
  }

  if (data.size() > kMaxPacketSize) {
    std::move(callback).Run(net::ERR_MSG_TOO_BIG);
    return;
  }

  // |data| points to a range of bytes in the received message and will be
  // freed when this method returns, so copy out the bytes now.
  auto buffer = base::MakeRefCounted<net::IOBufferWithSize>(data.size());
  memcpy(buffer.get()->data(), data.data(), data.size());

  if (send_buffer_.get()) {
    auto request = std::make_unique<PendingSendRequest>();
    if (dest_addr)
      request->addr = std::make_unique<net::IPEndPoint>(*dest_addr);
    request->data = buffer;
    request->traffic_annotation =
        net::MutableNetworkTrafficAnnotationTag(traffic_annotation);
    request->callback = std::move(callback);
    pending_send_requests_.push_back(std::move(request));
    return;
  }

  DoSendToOrWriteBuffer(dest_addr, buffer, traffic_annotation,
                        std::move(callback));
}

void UDPSocket::DoSendToOrWriteBuffer(
    const net::IPEndPoint* dest_addr,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    SendToCallback callback) {
  DCHECK(!send_buffer_);
  DCHECK(send_callback_.is_null());

  send_buffer_ = buffer;
  send_callback_ = std::move(callback);
  // base::Unretained(this) is safe because socket is owned by |this|.
  int net_result;
  if (dest_addr) {
    net_result = wrapped_socket_->SendTo(
        buffer.get(), buffer->size(), *dest_addr,
        base::BindRepeating(&UDPSocket::OnSendToCompleted,
                            base::Unretained(this)),
        traffic_annotation);
  } else {
    net_result = wrapped_socket_->Write(
        buffer.get(), buffer->size(),
        base::BindRepeating(&UDPSocket::OnSendToCompleted,
                            base::Unretained(this)),
        traffic_annotation);
  }
  if (net_result != net::ERR_IO_PENDING)
    OnSendToCompleted(net_result);
}

void UDPSocket::OnRecvFromCompleted(uint32_t buffer_size, int net_result) {
  DCHECK(recvfrom_buffer_);

  if (net_result >= 0) {
    listener_->OnReceived(
        net::OK,
        is_bound_ ? base::make_optional(recvfrom_address_) : base::nullopt,
        base::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(recvfrom_buffer_->data()),
            static_cast<size_t>(net_result)));
  } else {
    listener_->OnReceived(net_result, base::nullopt, base::nullopt);
  }
  recvfrom_buffer_ = nullptr;
  DCHECK_GT(remaining_recv_slots_, 0u);
  remaining_recv_slots_--;
  if (remaining_recv_slots_ > 0)
    DoRecvFrom(buffer_size);
}

void UDPSocket::OnSendToCompleted(int net_result) {
  DCHECK(send_buffer_.get());
  DCHECK(!send_callback_.is_null());

  if (net_result > net::OK)
    net_result = net::OK;
  send_buffer_ = nullptr;
  std::move(send_callback_).Run(net_result);

  if (pending_send_requests_.empty())
    return;
  std::unique_ptr<PendingSendRequest> request =
      std::move(pending_send_requests_.front());
  pending_send_requests_.pop_front();
  DoSendToOrWriteBuffer(request->addr.get(), request->data,
                        static_cast<net::NetworkTrafficAnnotationTag>(
                            request->traffic_annotation),
                        std::move(request->callback));
}

}  // namespace network

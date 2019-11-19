// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/mock_mdns_socket_factory.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "net/dns/public/util.h"

using testing::_;
using testing::Invoke;

namespace net {

MockMDnsDatagramServerSocket::MockMDnsDatagramServerSocket(
    AddressFamily address_family) {
  local_address_ = dns_util::GetMdnsReceiveEndPoint(address_family);
}

MockMDnsDatagramServerSocket::~MockMDnsDatagramServerSocket() = default;

int MockMDnsDatagramServerSocket::SendTo(IOBuffer* buf,
                                         int buf_len,
                                         const IPEndPoint& address,
                                         CompletionOnceCallback callback) {
  return SendToInternal(std::string(buf->data(), buf_len), address.ToString(),
                        base::AdaptCallbackForRepeating(std::move(callback)));
}

int MockMDnsDatagramServerSocket::RecvFrom(IOBuffer* buffer,
                                           int size,
                                           IPEndPoint* address,
                                           CompletionOnceCallback callback) {
  return RecvFromInternal(buffer, size, address,
                          base::AdaptCallbackForRepeating(std::move(callback)));
}

int MockMDnsDatagramServerSocket::GetLocalAddress(IPEndPoint* address) const {
  *address = local_address_;
  return OK;
}

void MockMDnsDatagramServerSocket::SetResponsePacket(
    const std::string& response_packet) {
  response_packet_ = response_packet;
}

int MockMDnsDatagramServerSocket::HandleRecvNow(
    IOBuffer* buffer,
    int size,
    IPEndPoint* address,
    CompletionRepeatingCallback callback) {
  int size_returned =
      std::min(response_packet_.size(), static_cast<size_t>(size));
  memcpy(buffer->data(), response_packet_.data(), size_returned);
  return size_returned;
}

int MockMDnsDatagramServerSocket::HandleRecvLater(
    IOBuffer* buffer,
    int size,
    IPEndPoint* address,
    CompletionRepeatingCallback callback) {
  int rv = HandleRecvNow(buffer, size, address, callback);
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::BindOnce(callback, rv));
  return ERR_IO_PENDING;
}

MockMDnsSocketFactory::MockMDnsSocketFactory() = default;

MockMDnsSocketFactory::~MockMDnsSocketFactory() = default;

void MockMDnsSocketFactory::CreateSockets(
    std::vector<std::unique_ptr<DatagramServerSocket>>* sockets) {
  CreateSocket(ADDRESS_FAMILY_IPV4, sockets);
  CreateSocket(ADDRESS_FAMILY_IPV6, sockets);
}

void MockMDnsSocketFactory::CreateSocket(
    AddressFamily address_family,
    std::vector<std::unique_ptr<DatagramServerSocket>>* sockets) {
  std::unique_ptr<testing::NiceMock<MockMDnsDatagramServerSocket>> new_socket(
      new testing::NiceMock<MockMDnsDatagramServerSocket>(address_family));

  ON_CALL(*new_socket, SendToInternal(_, _, _))
      .WillByDefault(Invoke(
          this,
          &MockMDnsSocketFactory::SendToInternal));

  ON_CALL(*new_socket, RecvFromInternal(_, _, _, _))
      .WillByDefault(Invoke(this, &MockMDnsSocketFactory::RecvFromInternal));

  sockets->push_back(std::move(new_socket));
}

void MockMDnsSocketFactory::SimulateReceive(const uint8_t* packet, int size) {
  DCHECK(recv_buffer_size_ >= size);
  DCHECK(recv_buffer_.get());
  DCHECK(!recv_callback_.is_null());

  memcpy(recv_buffer_->data(), packet, size);
  std::move(recv_callback_).Run(size);
}

int MockMDnsSocketFactory::RecvFromInternal(
    IOBuffer* buffer,
    int size,
    IPEndPoint* address,
    CompletionRepeatingCallback callback) {
  recv_buffer_ = buffer;
  recv_buffer_size_ = size;
  recv_callback_ = callback;
  return ERR_IO_PENDING;
}

int MockMDnsSocketFactory::SendToInternal(const std::string& packet,
                                          const std::string& address,
                                          CompletionOnceCallback callback) {
  OnSendTo(packet);
  return packet.size();
}

}  // namespace net

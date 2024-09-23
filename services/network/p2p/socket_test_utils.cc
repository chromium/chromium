// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "services/network/p2p/socket_test_utils.h"

#include <stddef.h>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "net/base/io_buffer.h"
#include "net/base/ip_address.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace network {

const int kStunHeaderSize = 20;
const uint16_t kStunBindingRequest = 0x0001;
const uint16_t kStunBindingResponse = 0x0101;
const uint16_t kStunBindingError = 0x0111;
const uint32_t kStunMagicCookie = 0x2112A442;

FakeP2PSocketDelegate::FakeP2PSocketDelegate() = default;
FakeP2PSocketDelegate::~FakeP2PSocketDelegate() {
  CHECK(sockets_to_be_destroyed_.empty());
}

void FakeP2PSocketDelegate::DestroySocket(P2PSocket* socket) {
  auto it = base::ranges::find(sockets_to_be_destroyed_, socket,
                               &std::unique_ptr<P2PSocket>::get);
  CHECK(it != sockets_to_be_destroyed_.end());
  sockets_to_be_destroyed_.erase(it);
}

void FakeP2PSocketDelegate::DumpPacket(base::span<const uint8_t> data,
                                       bool incoming) {}

void FakeP2PSocketDelegate::AddAcceptedConnection(
    std::unique_ptr<P2PSocket> accepted) {
  accepted_.push_back(std::move(accepted));
}

void FakeP2PSocketDelegate::ExpectDestruction(
    std::unique_ptr<P2PSocket> socket) {
  sockets_to_be_destroyed_.push_back(std::move(socket));
}

std::unique_ptr<P2PSocket> FakeP2PSocketDelegate::pop_accepted_socket() {
  if (accepted_.empty())
    return nullptr;
  auto result = std::move(accepted_.front());
  accepted_.pop_front();
  return result;
}

FakeSocket::FakeSocket(std::string* written_data)
    : read_pending_(false),
      input_pos_(0),
      written_data_(written_data),
      async_write_(false),
      write_pending_(false) {}

FakeSocket::~FakeSocket() {}

void FakeSocket::AppendInputData(const char* data, int data_size) {
  input_data_.insert(input_data_.end(), data, data + data_size);
  // Complete pending read if any.
  if (read_pending_) {
    read_pending_ = false;
    int result = std::min(read_buffer_size_,
                          static_cast<int>(input_data_.size() - input_pos_));
    CHECK(result > 0);
    memcpy(read_buffer_->data(), &input_data_[0] + input_pos_, result);
    input_pos_ += result;
    read_buffer_ = nullptr;
    std::move(read_callback_).Run(result);
  }
}

void FakeSocket::SetPeerAddress(const net::IPEndPoint& peer_address) {
  peer_address_ = peer_address;
}

void FakeSocket::SetLocalAddress(const net::IPEndPoint& local_address) {
  local_address_ = local_address;
}

int FakeSocket::Read(net::IOBuffer* buf,
                     int buf_len,
                     net::CompletionOnceCallback callback) {
  DCHECK(buf);
  if (input_pos_ < static_cast<int>(input_data_.size())) {
    int result =
        std::min(buf_len, static_cast<int>(input_data_.size()) - input_pos_);
    memcpy(buf->data(), &(*input_data_.begin()) + input_pos_, result);
    input_pos_ += result;
    return result;
  } else {
    read_pending_ = true;
    read_buffer_ = buf;
    read_buffer_size_ = buf_len;
    read_callback_ = std::move(callback);
    return net::ERR_IO_PENDING;
  }
}

int FakeSocket::Write(
    net::IOBuffer* buf,
    int buf_len,
    net::CompletionOnceCallback callback,
    const net::NetworkTrafficAnnotationTag& /*traffic_annotation*/) {
  DCHECK(buf);
  DCHECK(!write_pending_);

  if (async_write_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&FakeSocket::DoAsyncWrite, base::Unretained(this),
                       scoped_refptr<net::IOBuffer>(buf), buf_len,
                       std::move(callback)));
    write_pending_ = true;
    return net::ERR_IO_PENDING;
  }

  if (written_data_) {
    written_data_->insert(written_data_->end(), buf->data(),
                          buf->data() + buf_len);
  }
  return buf_len;
}

void FakeSocket::DoAsyncWrite(scoped_refptr<net::IOBuffer> buf,
                              int buf_len,
                              net::CompletionOnceCallback callback) {
  write_pending_ = false;

  if (written_data_) {
    written_data_->insert(written_data_->end(), buf->data(),
                          buf->data() + buf_len);
  }
  std::move(callback).Run(buf_len);
}

int FakeSocket::SetReceiveBufferSize(int32_t size) {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int FakeSocket::SetSendBufferSize(int32_t size) {
  NOTIMPLEMENTED();
  return net::ERR_NOT_IMPLEMENTED;
}

int FakeSocket::Connect(net::CompletionOnceCallback callback) {
  return 0;
}

void FakeSocket::Disconnect() {
  NOTREACHED_IN_MIGRATION();
}

bool FakeSocket::IsConnected() const {
  return true;
}

bool FakeSocket::IsConnectedAndIdle() const {
  return false;
}

int FakeSocket::GetPeerAddress(net::IPEndPoint* address) const {
  *address = peer_address_;
  return net::OK;
}

int FakeSocket::GetLocalAddress(net::IPEndPoint* address) const {
  *address = local_address_;
  return net::OK;
}

const net::NetLogWithSource& FakeSocket::NetLog() const {
  NOTREACHED_IN_MIGRATION();
  return net_log_;
}

bool FakeSocket::WasEverUsed() const {
  return true;
}

net::NextProto FakeSocket::GetNegotiatedProtocol() const {
  return net::kProtoUnknown;
}

bool FakeSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  return false;
}

int64_t FakeSocket::GetTotalReceivedBytes() const {
  NOTIMPLEMENTED();
  return 0;
}

FakeSocketClient::FakeSocketClient(
    mojo::PendingRemote<mojom::P2PSocket> socket,
    mojo::PendingReceiver<mojom::P2PSocketClient> client_receiver)
    : socket_(std::move(socket)), receiver_(this, std::move(client_receiver)) {
  receiver_.set_disconnect_handler(
      base::BindLambdaForTesting([&]() { disconnect_error_ = true; }));
}

FakeSocketClient::~FakeSocketClient() {}

FakeNetworkNotificationClient::FakeNetworkNotificationClient(
    base::OnceClosure closure,
    mojo::PendingReceiver<mojom::P2PNetworkNotificationClient>
        notification_client)
    : notification_client_(this, std::move(notification_client)),
      closure_(std::move(closure)) {}

FakeNetworkNotificationClient::~FakeNetworkNotificationClient() = default;

void FakeNetworkNotificationClient::NetworkListChanged(
    const std::vector<::net::NetworkInterface>& networks,
    const ::net::IPAddress& default_ipv4_local_address,
    const ::net::IPAddress& default_ipv6_local_address) {
  network_list_changed_ = true;
  std::move(closure_).Run();
}

void CreateRandomPacket(std::vector<uint8_t>* packet) {
  size_t size = kStunHeaderSize + rand() % 1000;
  packet->resize(size);
  for (size_t i = 0; i < size; i++) {
    (*packet)[i] = rand() % 256;
  }
  // Always set the first bit to ensure that generated packet is not
  // valid STUN packet.
  (*packet)[0] = (*packet)[0] | 0x80;
}

static void CreateStunPacket(std::vector<uint8_t>* packet, uint16_t type) {
  CreateRandomPacket(packet);
  auto header = base::span(*packet).first<8u>();
  header.subspan<0u, 2u>().copy_from(base::U16ToBigEndian(type));
  header.subspan<2u, 2u>().copy_from(base::U16ToBigEndian(
      base::checked_cast<uint16_t>(packet->size() - kStunHeaderSize)));
  header.subspan<4u, 4u>().copy_from(base::U32ToBigEndian(kStunMagicCookie));
}

void CreateStunRequest(std::vector<uint8_t>* packet) {
  CreateStunPacket(packet, kStunBindingRequest);
}

void CreateStunResponse(std::vector<uint8_t>* packet) {
  CreateStunPacket(packet, kStunBindingResponse);
}

void CreateStunError(std::vector<uint8_t>* packet) {
  CreateStunPacket(packet, kStunBindingError);
}

net::IPEndPoint ParseAddress(const std::string& ip_str, uint16_t port) {
  net::IPAddress ip;
  EXPECT_TRUE(ip.AssignFromIPLiteral(ip_str));
  return net::IPEndPoint(ip, port);
}

}  // namespace network

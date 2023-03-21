// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/p2p/socket_client_impl.h"

#include "base/location.h"
#include "base/time/time.h"
#include "crypto/random.h"
#include "services/network/public/cpp/p2p_param_traits.h"
#include "third_party/blink/renderer/platform/p2p/socket_client_delegate.h"
#include "third_party/blink/renderer/platform/p2p/socket_dispatcher.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace {

uint64_t GetUniqueId(uint32_t random_socket_id, uint32_t packet_id) {
  uint64_t uid = random_socket_id;
  uid <<= 32;
  uid |= packet_id;
  return uid;
}

}  // namespace

namespace blink {

P2PSocketClientImpl::P2PSocketClientImpl(
    P2PSocketDispatcher* dispatcher,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : dispatcher_(dispatcher),
      socket_id_(0),
      delegate_(nullptr),
      state_(kStateUninitialized),
      traffic_annotation_(traffic_annotation),
      random_socket_id_(0),
      next_packet_id_(0) {
  crypto::RandBytes(&random_socket_id_, sizeof(random_socket_id_));
}

P2PSocketClientImpl::~P2PSocketClientImpl() {
  CHECK(state_ == kStateClosed || state_ == kStateUninitialized);
}

void P2PSocketClientImpl::Init(
    network::P2PSocketType type,
    const net::IPEndPoint& local_address,
    uint16_t min_port,
    uint16_t max_port,
    const network::P2PHostAndIPEndPoint& remote_address,
    blink::P2PSocketClientDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate);
  // |delegate_| is only accessesed on |delegate_message_loop_|.
  delegate_ = delegate;

  DCHECK_EQ(state_, kStateUninitialized);
  state_ = kStateOpening;
  auto dispatcher = dispatcher_.Lock();
  CHECK(dispatcher);
  dispatcher->GetP2PSocketManager()->CreateSocket(
      type, local_address, network::P2PPortRange(min_port, max_port),
      remote_address,
      net::MutableNetworkTrafficAnnotationTag(traffic_annotation_),
      receiver_.BindNewPipeAndPassRemote(),
      socket_.BindNewPipeAndPassReceiver());
  receiver_.set_disconnect_handler(WTF::BindOnce(
      &P2PSocketClientImpl::OnConnectionError, WTF::Unretained(this)));
}

uint64_t P2PSocketClientImpl::Send(const net::IPEndPoint& address,
                                   base::span<const uint8_t> data,
                                   const rtc::PacketOptions& options) {
  uint64_t unique_id = GetUniqueId(random_socket_id_, ++next_packet_id_);

  // Can send data only when the socket is open.
  DCHECK(state_ == kStateOpen || state_ == kStateError);
  if (state_ == kStateOpen) {
    SendWithPacketId(address, data, options, unique_id);
  }

  return unique_id;
}

void P2PSocketClientImpl::SendWithPacketId(const net::IPEndPoint& address,
                                           base::span<const uint8_t> data,
                                           const rtc::PacketOptions& options,
                                           uint64_t packet_id) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("p2p", "Send", packet_id);

  socket_->Send(data, network::P2PPacketInfo(address, options, packet_id));
}

void P2PSocketClientImpl::SetOption(network::P2PSocketOption option,
                                    int value) {
  DCHECK(state_ == kStateOpen || state_ == kStateError);
  if (state_ == kStateOpen)
    socket_->SetOption(option, value);
}

void P2PSocketClientImpl::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  delegate_ = nullptr;
  if (socket_)
    socket_.reset();

  state_ = kStateClosed;
}

int P2PSocketClientImpl::GetSocketID() const {
  return socket_id_;
}

void P2PSocketClientImpl::SetDelegate(
    blink::P2PSocketClientDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  delegate_ = delegate;
}

void P2PSocketClientImpl::SocketCreated(const net::IPEndPoint& local_address,
                                        const net::IPEndPoint& remote_address) {
  state_ = kStateOpen;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delegate_)
    delegate_->OnOpen(local_address, remote_address);
}

void P2PSocketClientImpl::SendComplete(
    const network::P2PSendPacketMetrics& send_metrics) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delegate_)
    delegate_->OnSendComplete(send_metrics);
}

void P2PSocketClientImpl::DataReceived(
    WTF::Vector<P2PReceivedPacketPtr> packets) {
  DCHECK(!packets.empty());
  DCHECK_EQ(kStateOpen, state_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delegate_) {
    for (auto& packet : packets) {
      delegate_->OnDataReceived(packet->socket_address, packet->data,
                                packet->timestamp);
    }
  }
}

void P2PSocketClientImpl::OnConnectionError() {
  state_ = kStateError;
  if (delegate_)
    delegate_->OnError();
}

}  // namespace blink

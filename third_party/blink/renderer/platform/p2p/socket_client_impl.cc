// Copyright 2013 The Chromium Authors. All rights reserved.
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
      state_(STATE_UNINITIALIZED),
      traffic_annotation_(traffic_annotation),
      random_socket_id_(0),
      next_packet_id_(0) {
  crypto::RandBytes(&random_socket_id_, sizeof(random_socket_id_));
}

P2PSocketClientImpl::~P2PSocketClientImpl() {
  CHECK(state_ == STATE_CLOSED || state_ == STATE_UNINITIALIZED);
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

  DCHECK_EQ(state_, STATE_UNINITIALIZED);
  state_ = STATE_OPENING;
  dispatcher_->GetP2PSocketManager()->CreateSocket(
      type, local_address, network::P2PPortRange(min_port, max_port),
      remote_address, receiver_.BindNewPipeAndPassRemote(),
      socket_.BindNewPipeAndPassReceiver());
  receiver_.set_disconnect_handler(WTF::Bind(
      &P2PSocketClientImpl::OnConnectionError, WTF::Unretained(this)));
}

uint64_t P2PSocketClientImpl::Send(const net::IPEndPoint& address,
                                   const Vector<int8_t>& data,
                                   const rtc::PacketOptions& options) {
  uint64_t unique_id = GetUniqueId(random_socket_id_, ++next_packet_id_);

  // Can send data only when the socket is open.
  DCHECK(state_ == STATE_OPEN || state_ == STATE_ERROR);
  if (state_ == STATE_OPEN) {
    SendWithPacketId(address, data, options, unique_id);
  }

  return unique_id;
}

void P2PSocketClientImpl::SendWithPacketId(const net::IPEndPoint& address,
                                           const Vector<int8_t>& data,
                                           const rtc::PacketOptions& options,
                                           uint64_t packet_id) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("p2p", "Send", packet_id);

  socket_->Send(data, network::P2PPacketInfo(address, options, packet_id),
                net::MutableNetworkTrafficAnnotationTag(traffic_annotation_));
}

void P2PSocketClientImpl::SetOption(network::P2PSocketOption option,
                                    int value) {
  DCHECK(state_ == STATE_OPEN || state_ == STATE_ERROR);
  if (state_ == STATE_OPEN)
    socket_->SetOption(option, value);
}

void P2PSocketClientImpl::Close() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  delegate_ = nullptr;
  if (socket_)
    socket_.reset();

  state_ = STATE_CLOSED;
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
  state_ = STATE_OPEN;
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

void P2PSocketClientImpl::IncomingTcpConnection(
    const net::IPEndPoint& socket_address,
    mojo::PendingRemote<network::mojom::blink::P2PSocket> socket,
    mojo::PendingReceiver<network::mojom::blink::P2PSocketClient>
        client_receiver) {
  DCHECK_EQ(state_, STATE_OPEN);

  auto new_client =
      std::make_unique<P2PSocketClientImpl>(dispatcher_, traffic_annotation_);
  new_client->state_ = STATE_OPEN;

  new_client->socket_.Bind(std::move(socket));
  new_client->receiver_.Bind(std::move(client_receiver));
  new_client->receiver_.set_disconnect_handler(WTF::Bind(
      &P2PSocketClientImpl::OnConnectionError, WTF::Unretained(this)));

  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delegate_) {
    delegate_->OnIncomingTcpConnection(socket_address, std::move(new_client));
  } else {
    // Just close the socket if there is no delegate to accept it.
    new_client->Close();
  }
}

void P2PSocketClientImpl::DataReceived(const net::IPEndPoint& socket_address,
                                       const Vector<int8_t>& data,
                                       base::TimeTicks timestamp) {
  DCHECK_EQ(STATE_OPEN, state_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delegate_)
    delegate_->OnDataReceived(socket_address, data, timestamp);
}

void P2PSocketClientImpl::OnConnectionError() {
  state_ = STATE_ERROR;
  if (delegate_)
    delegate_->OnError();
}

}  // namespace blink

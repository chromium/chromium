// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/renderer/platform/p2p/socket_client_impl.h"

#include "base/location.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
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

void RecordNumberOfPacketsInBatch(int num_packets) {
  DCHECK_GT(num_packets, 0);
  UMA_HISTOGRAM_COUNTS("WebRTC.P2P.UDP.BatchingNumberOfSentPackets",
                       num_packets);
}

}  // namespace

namespace blink {

P2PSocketClientImpl::P2PSocketClientImpl(bool batch_packets)
    : batch_packets_(batch_packets),
      socket_id_(0),
      delegate_(nullptr),
      state_(kStateUninitialized),
      random_socket_id_(0),
      next_packet_id_(0) {
  crypto::RandBytes(base::byte_span_from_ref(random_socket_id_));
}

P2PSocketClientImpl::~P2PSocketClientImpl() {
  CHECK(state_ == kStateClosed || state_ == kStateUninitialized);
}

void P2PSocketClientImpl::Init(blink::P2PSocketClientDelegate* delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate);
  // |delegate_| is only accessesed on |delegate_message_loop_|.
  delegate_ = delegate;

  DCHECK_EQ(state_, kStateUninitialized);
  state_ = kStateOpening;
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

void P2PSocketClientImpl::FlushBatch() {
  DoSendBatch();
}

void P2PSocketClientImpl::DoSendBatch() {
  TRACE_EVENT1("p2p", __func__, "num_packets", batched_send_packets_.size());
  awaiting_batch_complete_ = false;
  if (!batched_send_packets_.empty()) {
    WTF::Vector<network::mojom::blink::P2PSendPacketPtr> batched_send_packets;
    batched_send_packets_.swap(batched_send_packets);
    RecordNumberOfPacketsInBatch(batched_send_packets.size());
    socket_->SendBatch(std::move(batched_send_packets));
    batched_packets_storage_.clear();
  }
}

void P2PSocketClientImpl::SendWithPacketId(const net::IPEndPoint& address,
                                           base::span<const uint8_t> data,
                                           const rtc::PacketOptions& options,
                                           uint64_t packet_id) {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("p2p", "Send", packet_id);

  // Conditionally start or continue temporarily storing the packets of a batch.
  // We can't allow sending individual packets mid batch since we would receive
  // SendComplete with out-of-order packet IDs. Therefore, we include them in
  // the batch.
  // Additionally, logic below ensures we send single-packet batches to use the
  // Send interface instead of SendBatch to reduce pointless overhead.
  if (batch_packets_ &&
      (awaiting_batch_complete_ ||
       (options.batchable && !options.last_packet_in_batch))) {
    awaiting_batch_complete_ = true;
    batched_packets_storage_.emplace_back(data);
    const auto& storage = batched_packets_storage_.back();
    batched_send_packets_.emplace_back(
        network::mojom::blink::P2PSendPacket::New(
            base::span<const uint8_t>(storage.begin(), storage.end()),
            network::P2PPacketInfo(address, options, packet_id)));
    if (options.last_packet_in_batch) {
      DoSendBatch();
    }
  } else {
    RecordNumberOfPacketsInBatch(1);
    awaiting_batch_complete_ = false;
    socket_->Send(data, network::P2PPacketInfo(address, options, packet_id));
  }
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

void P2PSocketClientImpl::SendBatchComplete(
    const WTF::Vector<::network::P2PSendPacketMetrics>& in_send_metrics_batch) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  TRACE_EVENT1("p2p", __func__, "num_packets", in_send_metrics_batch.size());
  if (delegate_) {
    for (const auto& send_metrics : in_send_metrics_batch) {
      delegate_->OnSendComplete(send_metrics);
    }
  }
}

void P2PSocketClientImpl::DataReceived(
    WTF::Vector<P2PReceivedPacketPtr> packets) {
  DCHECK(!packets.empty());
  DCHECK_EQ(kStateOpen, state_);
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (delegate_) {
    for (auto& packet : packets) {
      delegate_->OnDataReceived(packet->socket_address, packet->data,
                                packet->timestamp, packet->ecn);
    }
  }
}

void P2PSocketClientImpl::OnConnectionError() {
  state_ = kStateError;
  if (delegate_)
    delegate_->OnError();
}

}  // namespace blink

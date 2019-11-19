// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/adapters/quic_packet_transport_adapter.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_logging.h"

namespace blink {

QuicPacketTransportAdapter::QuicPacketTransportAdapter(
    cricket::IceTransportInternal* ice_transport)
    : ice_transport_(ice_transport) {
  DCHECK(ice_transport_);
  ice_transport_->SignalReadPacket.connect(
      this, &QuicPacketTransportAdapter::OnReadPacket);
  ice_transport_->SignalReadyToSend.connect(
      this, &QuicPacketTransportAdapter::OnReadyToSend);
}

QuicPacketTransportAdapter::~QuicPacketTransportAdapter() {
  // Caller is responsible for unsetting the write observer and receive
  // delegate before destroying this.
  DCHECK(!write_observer_);
  DCHECK(!receive_delegate_);
}

int QuicPacketTransportAdapter::WritePacket(const QuicPacket& packet) {
  rtc::PacketOptions options;
  options.packet_id = packet.packet_number;
  int flags = 0;
  return ice_transport_->SendPacket(packet.buffer, packet.buf_len, options,
                                    flags);
}

void QuicPacketTransportAdapter::SetReceiveDelegate(
    ReceiveDelegate* receive_delegate) {
  receive_delegate_ = receive_delegate;
  if (!cached_client_hello_packet_.empty() && receive_delegate_) {
    // If a CHLO was received early, give it to the delegate.
    receive_delegate_->OnPacketDataReceived(cached_client_hello_packet_.c_str(),
                                            cached_client_hello_packet_.size());
    cached_client_hello_packet_.clear();
  }
}

void QuicPacketTransportAdapter::SetWriteObserver(
    WriteObserver* write_observer) {
  write_observer_ = write_observer;
}

bool QuicPacketTransportAdapter::Writable() {
  return ice_transport_->writable();
}

// IceTransportInternal callbacks.
void QuicPacketTransportAdapter::OnReadPacket(
    rtc::PacketTransportInternal* packet_transport,
    const char* buffer,
    size_t buffer_length,
    const int64_t& packet_time,
    int flags) {
  DCHECK_EQ(packet_transport, ice_transport_);
  if (!receive_delegate_) {
    // Cache the early CHLO from the QUIC handshake.
    // The CHLO is stored in a single packet. All packets before the most recent
    // can be discarded because they are no longer relevant, since at this point
    // we have not responded. The packet could also be a connection close packet
    // in the case that QUIC times out waiting for a response.
    cached_client_hello_packet_ = std::string(buffer, buffer_length);
    return;
  }
  receive_delegate_->OnPacketDataReceived(buffer, buffer_length);
}

void QuicPacketTransportAdapter::OnReadyToSend(
    rtc::PacketTransportInternal* packet_transport) {
  DCHECK_EQ(packet_transport, ice_transport_);
  if (!write_observer_) {
    return;
  }
  write_observer_->OnCanWrite();
}

}  // namespace blink

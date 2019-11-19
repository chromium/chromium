// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_PACKET_TRANSPORT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_PACKET_TRANSPORT_H_

#include "net/third_party/quiche/src/quic/core/quic_packet_writer.h"
#include "net/third_party/quiche/src/quic/core/quic_types.h"
#include "net/third_party/quiche/src/quic/platform/api/quic_export.h"

namespace blink {

// This is the interface for the underlying packet transport used by the
// P2PQuicTransport for receiving and writing data. The standard
// implementation of this interface uses an ICE transport.
//
// This object should be run entirely on the webrtc worker thread.
class P2PQuicPacketTransport {
 public:
  // This is subclassed by the P2PQuicTransport so that it can receive incoming
  // data. The standard case is for this to be the P2PQuicTransport.
  // The P2PQuicPacketTransport will outlive the ReceiveDelegate.
  class ReceiveDelegate {
   public:
    virtual ~ReceiveDelegate() = default;
    virtual void OnPacketDataReceived(const char* data, size_t data_len) = 0;
  };

  // This is subclassed by the Writer, so that it is aware when the
  // P2PQuicPacketTransport is ready to write data. The
  // P2PQuicPacketTransport will outlive the WriteObserver.
  class WriteObserver {
   public:
    virtual ~WriteObserver() = default;
    virtual void OnCanWrite() = 0;
  };

  struct QuicPacket {
    // This is taken from the quic::QuicConnection, and 0 means that it is not
    // set. Packet numbers are used to provide metadata to the implementation of
    // the P2PQuicPacketTransport, but this number is not used by the QUIC
    // library itself.
    uint64_t packet_number;
    const char* buffer;
    size_t buf_len;
  };

  virtual ~P2PQuicPacketTransport() = default;

  // Called by the P2PQuicPacketWriter (in quic_transport_factory_impl.cc) when
  // writing QUIC packets to the network. Return the number of written bytes.
  // Return 0 if the write is blocked.
  virtual int WritePacket(const QuicPacket& packet) = 0;
  // Sets the ReceiveDelegate for receiving packets.
  // Since the ReceiveDelegate has a shorter lifetime than the
  // P2PQuicPacketTransport, it must unset itself upon destruction.
  virtual void SetReceiveDelegate(ReceiveDelegate* receive_delegate) = 0;
  // Sets the WriteObserver for obsererving when it can write to the
  // P2PQuicPacketTransport. Since the WriteObserver has a shorter lifetime than
  // the P2PQuicPacketTransport, it must unset itself upon destruction.
  virtual void SetWriteObserver(WriteObserver* write_observer) = 0;
  // Returns true if the P2PQuicPacketTransport can write.
  virtual bool Writable() = 0;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PEERCONNECTION_ADAPTERS_P2P_QUIC_PACKET_TRANSPORT_H_

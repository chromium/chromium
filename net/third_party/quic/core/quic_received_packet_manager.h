// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_QUIC_RECEIVED_PACKET_MANAGER_H_
#define NET_THIRD_PARTY_QUIC_CORE_QUIC_RECEIVED_PACKET_MANAGER_H_

#include "base/macros.h"
#include "net/third_party/quic/core/quic_config.h"
#include "net/third_party/quic/core/quic_framer.h"
#include "net/third_party/quic/core/quic_packets.h"
#include "net/third_party/quic/platform/api/quic_export.h"

namespace quic {

namespace test {
class QuicConnectionPeer;
}  // namespace test

struct QuicConnectionStats;

// Records all received packets by a connection.
class QUIC_EXPORT_PRIVATE QuicReceivedPacketManager {
 public:
  explicit QuicReceivedPacketManager(QuicConnectionStats* stats);
  QuicReceivedPacketManager(const QuicReceivedPacketManager&) = delete;
  QuicReceivedPacketManager& operator=(const QuicReceivedPacketManager&) =
      delete;
  virtual ~QuicReceivedPacketManager();

  // Updates the internal state concerning which packets have been received.
  // header: the packet header.
  // timestamp: the arrival time of the packet.
  virtual void RecordPacketReceived(const QuicPacketHeader& header,
                                    QuicTime receipt_time);

  // Checks whether |packet_number| is missing and less than largest observed.
  virtual bool IsMissing(QuicPacketNumber packet_number);

  // Checks if we're still waiting for the packet with |packet_number|.
  virtual bool IsAwaitingPacket(QuicPacketNumber packet_number);

  // Retrieves a frame containing a QuicAckFrame.  The ack frame may not be
  // changed outside QuicReceivedPacketManager and must be serialized before
  // another packet is received, or it will change.
  const QuicFrame GetUpdatedAckFrame(QuicTime approximate_now);

  // Deletes all missing packets before least unacked. The connection won't
  // process any packets with packet number before |least_unacked| that it
  // received after this call.
  void DontWaitForPacketsBefore(QuicPacketNumber least_unacked);

  // Returns true if there are any missing packets.
  bool HasMissingPackets() const;

  // Returns true when there are new missing packets to be reported within 3
  // packets of the largest observed.
  virtual bool HasNewMissingPackets() const;

  QuicPacketNumber peer_least_packet_awaiting_ack() {
    return peer_least_packet_awaiting_ack_;
  }

  virtual bool ack_frame_updated() const;

  QuicPacketNumber GetLargestObserved() const;

  // For logging purposes.
  const QuicAckFrame& ack_frame() const { return ack_frame_; }

  void set_max_ack_ranges(size_t max_ack_ranges) {
    max_ack_ranges_ = max_ack_ranges;
  }

  void set_save_timestamps(bool save_timestamps) {
    save_timestamps_ = save_timestamps;
  }

 private:
  friend class test::QuicConnectionPeer;

  // Least packet number of the the packet sent by the peer for which it
  // hasn't received an ack.
  QuicPacketNumber peer_least_packet_awaiting_ack_;

  // Received packet information used to produce acks.
  QuicAckFrame ack_frame_;

  // True if |ack_frame_| has been updated since UpdateReceivedPacketInfo was
  // last called.
  bool ack_frame_updated_;

  // Maximum number of ack ranges allowed to be stored in the ack frame.
  size_t max_ack_ranges_;

  // The time we received the largest_observed packet number, or zero if
  // no packet numbers have been received since UpdateReceivedPacketInfo.
  // Needed for calculating ack_delay_time.
  QuicTime time_largest_observed_;

  // If true, save timestamps in the ack_frame_.
  bool save_timestamps_;

  QuicConnectionStats* stats_;
};

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_QUIC_RECEIVED_PACKET_MANAGER_H_

// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_ACK_FRAME_H_
#define NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_ACK_FRAME_H_

#include <ostream>

#include "net/third_party/quic/core/quic_types.h"
#include "net/third_party/quic/platform/api/quic_containers.h"
#include "net/third_party/quic/platform/api/quic_export.h"
#include "net/third_party/quic/platform/api/quic_flags.h"
#include "net/third_party/quic/platform/api/quic_interval.h"

namespace quic {

// A sequence of packet numbers where each number is unique. Intended to be used
// in a sliding window fashion, where smaller old packet numbers are removed and
// larger new packet numbers are added, with the occasional random access.
class QUIC_EXPORT_PRIVATE PacketNumberQueue {
 public:
  PacketNumberQueue();
  PacketNumberQueue(const PacketNumberQueue& other);
  PacketNumberQueue(PacketNumberQueue&& other);
  ~PacketNumberQueue();

  PacketNumberQueue& operator=(const PacketNumberQueue& other);
  PacketNumberQueue& operator=(PacketNumberQueue&& other);

  typedef QuicDeque<QuicInterval<QuicPacketNumber>>::const_iterator
      const_iterator;
  typedef QuicDeque<QuicInterval<QuicPacketNumber>>::const_reverse_iterator
      const_reverse_iterator;

  // Adds |packet_number| to the set of packets in the queue.
  void Add(QuicPacketNumber packet_number);

  // Adds packets between [lower, higher) to the set of packets in the queue. It
  // is undefined behavior to call this with |higher| < |lower|.
  void AddRange(QuicPacketNumber lower, QuicPacketNumber higher);

  // Removes packets with values less than |higher| from the set of packets in
  // the queue. Returns true if packets were removed.
  bool RemoveUpTo(QuicPacketNumber higher);

  // Removes the smallest interval in the queue.
  void RemoveSmallestInterval();

  // Clear this packet number queue.
  void Clear();

  // Returns true if the queue contains |packet_number|.
  bool Contains(QuicPacketNumber packet_number) const;

  // Returns true if the queue is empty.
  bool Empty() const;

  // Returns the minimum packet number stored in the queue. It is undefined
  // behavior to call this if the queue is empty.
  QuicPacketNumber Min() const;

  // Returns the maximum packet number stored in the queue. It is undefined
  // behavior to call this if the queue is empty.
  QuicPacketNumber Max() const;

  // Returns the number of unique packets stored in the queue. Inefficient; only
  // exposed for testing.
  QuicPacketCount NumPacketsSlow() const;

  // Returns the number of disjoint packet number intervals contained in the
  // queue.
  size_t NumIntervals() const;

  // Returns the length of last interval.
  QuicPacketNumber LastIntervalLength() const;

  // Returns iterators over the packet number intervals.
  const_iterator begin() const;
  const_iterator end() const;
  const_reverse_iterator rbegin() const;
  const_reverse_iterator rend() const;

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const PacketNumberQueue& q);

 private:
  QuicDeque<QuicInterval<QuicPacketNumber>> packet_number_deque_;
};

struct QUIC_EXPORT_PRIVATE QuicAckFrame {
  QuicAckFrame();
  QuicAckFrame(const QuicAckFrame& other);
  ~QuicAckFrame();

  void Clear();

  friend QUIC_EXPORT_PRIVATE std::ostream& operator<<(
      std::ostream& os,
      const QuicAckFrame& ack_frame);

  // The highest packet number we've observed from the peer. When |packets| is
  // not empty, it should always be equal to packets.Max(). The |LargestAcked|
  // function ensures this invariant in debug mode.
  QuicPacketNumber largest_acked;

  // Time elapsed since largest_observed() was received until this Ack frame was
  // sent.
  QuicTime::Delta ack_delay_time;

  // Vector of <packet_number, time> for when packets arrived.
  PacketTimeVector received_packet_times;

  // Set of packets.
  PacketNumberQueue packets;

  // ECN counters, used only in version 99's ACK frame and valid only when
  // |ecn_counters_populated| is true.
  bool ecn_counters_populated;
  QuicPacketCount ect_0_count;
  QuicPacketCount ect_1_count;
  QuicPacketCount ecn_ce_count;
};

// The highest acked packet number we've observed from the peer. If no packets
// have been observed, return 0.
inline QUIC_EXPORT_PRIVATE QuicPacketNumber
LargestAcked(const QuicAckFrame& frame) {
  DCHECK(frame.packets.Empty() || frame.packets.Max() == frame.largest_acked);
  return frame.largest_acked;
}

// True if the packet number is greater than largest_observed or is listed
// as missing.
// Always returns false for packet numbers less than least_unacked.
QUIC_EXPORT_PRIVATE bool IsAwaitingPacket(
    const QuicAckFrame& ack_frame,
    QuicPacketNumber packet_number,
    QuicPacketNumber peer_least_packet_awaiting_ack);

}  // namespace quic

#endif  // NET_THIRD_PARTY_QUIC_CORE_FRAMES_QUIC_ACK_FRAME_H_

// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/frames/quic_ack_frame.h"

#include "net/third_party/quic/core/quic_constants.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_flag_utils.h"
#include "net/third_party/quic/platform/api/quic_interval.h"

namespace quic {

namespace {
const QuicPacketNumber kMaxPrintRange = 128;
}  // namespace

bool IsAwaitingPacket(const QuicAckFrame& ack_frame,
                      QuicPacketNumber packet_number,
                      QuicPacketNumber peer_least_packet_awaiting_ack) {
  return packet_number >= peer_least_packet_awaiting_ack &&
         !ack_frame.packets.Contains(packet_number);
}

QuicAckFrame::QuicAckFrame()
    : largest_acked(0),
      ack_delay_time(QuicTime::Delta::Infinite()),
      ecn_counters_populated(false),
      ect_0_count(0),
      ect_1_count(0),
      ecn_ce_count(0) {}

QuicAckFrame::QuicAckFrame(const QuicAckFrame& other) = default;

QuicAckFrame::~QuicAckFrame() {}

std::ostream& operator<<(std::ostream& os, const QuicAckFrame& ack_frame) {
  os << "{ largest_acked: " << LargestAcked(ack_frame)
     << ", ack_delay_time: " << ack_frame.ack_delay_time.ToMicroseconds()
     << ", packets: [ " << ack_frame.packets << " ]"
     << ", received_packets: [ ";
  for (const std::pair<QuicPacketNumber, QuicTime>& p :
       ack_frame.received_packet_times) {
    os << p.first << " at " << p.second.ToDebuggingValue() << " ";
  }
  os << " ]";
  os << ", ecn_counters_populated: " << ack_frame.ecn_counters_populated;
  if (ack_frame.ecn_counters_populated) {
    os << ", ect_0_count: " << ack_frame.ect_0_count
       << ", ect_1_count: " << ack_frame.ect_1_count
       << ", ecn_ce_count: " << ack_frame.ecn_ce_count;
  }

  os << " }\n";
  return os;
}

void QuicAckFrame::Clear() {
  largest_acked = 0;
  ack_delay_time = QuicTime::Delta::Infinite();
  received_packet_times.clear();
  packets.Clear();
}

PacketNumberQueue::PacketNumberQueue() {}
PacketNumberQueue::PacketNumberQueue(const PacketNumberQueue& other) = default;
PacketNumberQueue::PacketNumberQueue(PacketNumberQueue&& other) = default;
PacketNumberQueue::~PacketNumberQueue() {}

PacketNumberQueue& PacketNumberQueue::operator=(
    const PacketNumberQueue& other) = default;
PacketNumberQueue& PacketNumberQueue::operator=(PacketNumberQueue&& other) =
    default;

void PacketNumberQueue::Add(QuicPacketNumber packet_number) {
  // Check if the deque is empty
  if (packet_number_deque_.empty()) {
    packet_number_deque_.push_front(
        QuicInterval<QuicPacketNumber>(packet_number, packet_number + 1));
    return;
  }
  QuicInterval<QuicPacketNumber> back = packet_number_deque_.back();

  // Check for the typical case,
  // when the next packet in order is acked
  if (back.max() == packet_number) {
    packet_number_deque_.back().SetMax(packet_number + 1);
    return;
  }
  // Check if the next packet in order is skipped
  if (back.max() < packet_number) {
    packet_number_deque_.push_back(
        QuicInterval<QuicPacketNumber>(packet_number, packet_number + 1));
    return;
  }

  QuicInterval<QuicPacketNumber> front = packet_number_deque_.front();
  // Check if the packet can be  popped on the front
  if (front.min() > packet_number + 1) {
    packet_number_deque_.push_front(
        QuicInterval<QuicPacketNumber>(packet_number, packet_number + 1));
    return;
  }
  if (front.min() == packet_number + 1) {
    packet_number_deque_.front().SetMin(packet_number);
    return;
  }

  int i = packet_number_deque_.size() - 1;
  // Iterating through the queue backwards
  // to find a proper place for the packet
  while (i >= 0) {
    QuicInterval<QuicPacketNumber> packet_interval = packet_number_deque_[i];
    DCHECK(packet_interval.min() < packet_interval.max());
    // Check if the packet is contained in an interval already
    if (packet_interval.Contains(packet_number)) {
      return;
    }

    // Check if the packet can extend an interval.
    if (packet_interval.max() == packet_number) {
      packet_number_deque_[i].SetMax(packet_number + 1);
      return;
    }
    // Check if the packet can extend an interval
    // and merge two intervals if needed.
    // There is no need to merge an interval in the previous
    // if statement, as all merges will happen here.
    if (packet_interval.min() == packet_number + 1) {
      packet_number_deque_[i].SetMin(packet_number);
      if (i > 0 && packet_number == packet_number_deque_[i - 1].max()) {
        packet_number_deque_[i - 1].SetMax(packet_interval.max());
        packet_number_deque_.erase(packet_number_deque_.begin() + i);
      }
      return;
    }

    // Check if we need to make a new interval for the packet
    if (packet_interval.max() < packet_number + 1) {
      packet_number_deque_.insert(
          packet_number_deque_.begin() + i + 1,
          QuicInterval<QuicPacketNumber>(packet_number, packet_number + 1));
      return;
    }
    i--;
  }
}

void PacketNumberQueue::AddRange(QuicPacketNumber lower,
                                 QuicPacketNumber higher) {
  if (lower >= higher) {
    return;
  }
  if (packet_number_deque_.empty()) {
    packet_number_deque_.push_front(
        QuicInterval<QuicPacketNumber>(lower, higher));
    return;
  }
  QuicInterval<QuicPacketNumber> back = packet_number_deque_.back();

  if (back.max() == lower) {
    // Check for the typical case,
    // when the next packet in order is acked
    packet_number_deque_.back().SetMax(higher);
    return;
  }
  if (back.max() < lower) {
    // Check if the next packet in order is skipped
    packet_number_deque_.push_back(
        QuicInterval<QuicPacketNumber>(lower, higher));
    return;
  }
  QuicInterval<QuicPacketNumber> front = packet_number_deque_.front();
  // Check if the packets are being added in reverse order
  if (front.min() == higher) {
    packet_number_deque_.front().SetMin(lower);
  } else if (front.min() > higher) {
    packet_number_deque_.push_front(
        QuicInterval<QuicPacketNumber>(lower, higher));

  } else {
    // Ranges must be above or below all existing ranges.
    QUIC_BUG << "AddRange only supports adding packets above or below the "
             << "current min:" << Min() << " and max:" << Max()
             << ", but adding [" << lower << "," << higher << ")";
  }
}

bool PacketNumberQueue::RemoveUpTo(QuicPacketNumber higher) {
  if (Empty()) {
    return false;
  }
  const QuicPacketNumber old_min = Min();
  while (!packet_number_deque_.empty()) {
    QuicInterval<QuicPacketNumber> front = packet_number_deque_.front();
    if (front.max() < higher) {
      packet_number_deque_.pop_front();
    } else if (front.min() < higher && front.max() >= higher) {
      packet_number_deque_.front().SetMin(higher);
      if (front.max() == higher) {
        packet_number_deque_.pop_front();
      }
      break;
    } else {
      break;
    }
  }

  return Empty() || old_min != Min();
}

void PacketNumberQueue::RemoveSmallestInterval() {
  QUIC_BUG_IF(packet_number_deque_.size() < 2)
      << (Empty() ? "No intervals to remove."
                  : "Can't remove the last interval.");
  packet_number_deque_.pop_front();
}

void PacketNumberQueue::Clear() {
  packet_number_deque_.clear();
}

bool PacketNumberQueue::Contains(QuicPacketNumber packet_number) const {
  if (packet_number_deque_.empty()) {
    return false;
  }
  if (packet_number_deque_.front().min() > packet_number ||
      packet_number_deque_.back().max() <= packet_number) {
    return false;
  }
  for (QuicInterval<QuicPacketNumber> interval : packet_number_deque_) {
    if (interval.Contains(packet_number)) {
      return true;
    }
  }
  return false;
}

bool PacketNumberQueue::Empty() const {
  return packet_number_deque_.empty();
}

QuicPacketNumber PacketNumberQueue::Min() const {
  DCHECK(!Empty());
  return packet_number_deque_.front().min();
}

QuicPacketNumber PacketNumberQueue::Max() const {
  DCHECK(!Empty());
  return packet_number_deque_.back().max() - 1;
}

QuicPacketCount PacketNumberQueue::NumPacketsSlow() const {
  QuicPacketCount n_packets = 0;
  for (QuicInterval<QuicPacketNumber> interval : packet_number_deque_) {
    n_packets += interval.Length();
  }
  return n_packets;
}

size_t PacketNumberQueue::NumIntervals() const {
  return packet_number_deque_.size();
}

PacketNumberQueue::const_iterator PacketNumberQueue::begin() const {
  return packet_number_deque_.begin();
}

PacketNumberQueue::const_iterator PacketNumberQueue::end() const {
  return packet_number_deque_.end();
}

PacketNumberQueue::const_reverse_iterator PacketNumberQueue::rbegin() const {
  return packet_number_deque_.rbegin();
}

PacketNumberQueue::const_reverse_iterator PacketNumberQueue::rend() const {
  return packet_number_deque_.rend();
}

QuicPacketNumber PacketNumberQueue::LastIntervalLength() const {
  DCHECK(!Empty());
  return packet_number_deque_.back().Length();
}

// Largest min...max range for packet numbers where we print the numbers
// explicitly. If bigger than this, we print as a range  [a,d] rather
// than [a b c d]

std::ostream& operator<<(std::ostream& os, const PacketNumberQueue& q) {
  for (const QuicInterval<QuicPacketNumber>& interval : q) {
    // Print as a range if there is a pathological condition.
    if ((interval.min() >= interval.max()) ||
        (interval.max() - interval.min() > kMaxPrintRange)) {
      // If min>max, it's really a bug, so QUIC_BUG it to
      // catch it in development.
      QUIC_BUG_IF(interval.min() >= interval.max())
          << "Ack Range minimum (" << interval.min() << "Not less than max ("
          << interval.max() << ")";
      // print range as min...max rather than full list.
      // in the event of a bug, the list could be very big.
      os << interval.min() << "..." << (interval.max() - 1) << " ";
    } else {
      for (QuicPacketNumber packet_number = interval.min();
           packet_number < interval.max(); ++packet_number) {
        os << packet_number << " ";
      }
    }
  }
  return os;
}

}  // namespace quic

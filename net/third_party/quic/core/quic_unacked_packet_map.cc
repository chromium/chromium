// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quic/core/quic_unacked_packet_map.h"

#include <limits>
#include <type_traits>

#include "net/third_party/quic/core/quic_connection_stats.h"
#include "net/third_party/quic/core/quic_utils.h"
#include "net/third_party/quic/platform/api/quic_bug_tracker.h"
#include "net/third_party/quic/platform/api/quic_flag_utils.h"

namespace quic {

namespace {
bool WillStreamFrameLengthSumWrapAround(QuicPacketLength lhs,
                                        QuicPacketLength rhs) {
  static_assert(
      std::is_unsigned<QuicPacketLength>::value,
      "This function assumes QuicPacketLength is an unsigned integer type.");
  return std::numeric_limits<QuicPacketLength>::max() - lhs < rhs;
}
}  // namespace

QuicUnackedPacketMap::QuicUnackedPacketMap()
    : largest_sent_packet_(0),
      largest_sent_retransmittable_packet_(0),
      largest_sent_largest_acked_(0),
      largest_acked_(0),
      least_unacked_(1),
      bytes_in_flight_(0),
      pending_crypto_packet_count_(0),
      last_crypto_packet_sent_time_(QuicTime::Zero()),
      session_notifier_(nullptr),
      session_decides_what_to_write_(false),
      fix_is_useful_for_retransmission_(
          GetQuicReloadableFlag(quic_fix_is_useful_for_retrans)) {}

QuicUnackedPacketMap::~QuicUnackedPacketMap() {
  for (QuicTransmissionInfo& transmission_info : unacked_packets_) {
    DeleteFrames(&(transmission_info.retransmittable_frames));
  }
}

void QuicUnackedPacketMap::AddSentPacket(SerializedPacket* packet,
                                         QuicPacketNumber old_packet_number,
                                         TransmissionType transmission_type,
                                         QuicTime sent_time,
                                         bool set_in_flight) {
  QuicPacketNumber packet_number = packet->packet_number;
  QuicPacketLength bytes_sent = packet->encrypted_length;
  QUIC_BUG_IF(largest_sent_packet_ >= packet_number) << packet_number;
  DCHECK_GE(packet_number, least_unacked_ + unacked_packets_.size());
  while (least_unacked_ + unacked_packets_.size() < packet_number) {
    unacked_packets_.push_back(QuicTransmissionInfo());
    unacked_packets_.back().state = NEVER_SENT;
  }

  const bool has_crypto_handshake =
      packet->has_crypto_handshake == IS_HANDSHAKE;
  QuicTransmissionInfo info(
      packet->encryption_level, packet->packet_number_length, transmission_type,
      sent_time, bytes_sent, has_crypto_handshake, packet->num_padding_bytes);
  info.largest_acked = packet->largest_acked;
  largest_sent_largest_acked_ =
      std::max(largest_sent_largest_acked_, packet->largest_acked);
  if (old_packet_number > 0) {
    TransferRetransmissionInfo(old_packet_number, packet_number,
                               transmission_type, &info);
  }

  largest_sent_packet_ = packet_number;
  if (set_in_flight) {
    bytes_in_flight_ += bytes_sent;
    info.in_flight = true;
    largest_sent_retransmittable_packet_ = packet_number;
  }
  unacked_packets_.push_back(info);
  // Swap the retransmittable frames to avoid allocations.
  // TODO(ianswett): Could use emplace_back when Chromium can.
  if (old_packet_number == 0) {
    if (has_crypto_handshake) {
      ++pending_crypto_packet_count_;
      last_crypto_packet_sent_time_ = sent_time;
    }

    packet->retransmittable_frames.swap(
        unacked_packets_.back().retransmittable_frames);
  }
}

void QuicUnackedPacketMap::RemoveObsoletePackets() {
  while (!unacked_packets_.empty()) {
    if (!IsPacketUseless(least_unacked_, unacked_packets_.front())) {
      break;
    }
    if (session_decides_what_to_write_) {
      DeleteFrames(&unacked_packets_.front().retransmittable_frames);
    }
    unacked_packets_.pop_front();
    ++least_unacked_;
  }
}

void QuicUnackedPacketMap::TransferRetransmissionInfo(
    QuicPacketNumber old_packet_number,
    QuicPacketNumber new_packet_number,
    TransmissionType transmission_type,
    QuicTransmissionInfo* info) {
  if (old_packet_number < least_unacked_) {
    // This can happen when a retransmission packet is queued because of write
    // blocked socket, and the original packet gets acked before the
    // retransmission gets sent.
    return;
  }
  if (old_packet_number > largest_sent_packet_) {
    QUIC_BUG << "Old QuicTransmissionInfo never existed for :"
             << old_packet_number << " largest_sent:" << largest_sent_packet_;
    return;
  }
  DCHECK_GE(new_packet_number, least_unacked_ + unacked_packets_.size());
  DCHECK_NE(NOT_RETRANSMISSION, transmission_type);

  QuicTransmissionInfo* transmission_info =
      &unacked_packets_.at(old_packet_number - least_unacked_);
  QuicFrames* frames = &transmission_info->retransmittable_frames;
  if (session_notifier_ != nullptr) {
    for (const QuicFrame& frame : *frames) {
      if (frame.type == STREAM_FRAME) {
        session_notifier_->OnStreamFrameRetransmitted(frame.stream_frame);
      }
    }
  }

  // Swap the frames and preserve num_padding_bytes and has_crypto_handshake.
  frames->swap(info->retransmittable_frames);
  info->has_crypto_handshake = transmission_info->has_crypto_handshake;
  transmission_info->has_crypto_handshake = false;
  info->num_padding_bytes = transmission_info->num_padding_bytes;

  // Don't link old transmissions to new ones when version or
  // encryption changes.
  if (transmission_type == ALL_INITIAL_RETRANSMISSION ||
      transmission_type == ALL_UNACKED_RETRANSMISSION) {
    transmission_info->state = UNACKABLE;
  } else {
    transmission_info->retransmission = new_packet_number;
  }
  // Proactively remove obsolete packets so the least unacked can be raised.
  RemoveObsoletePackets();
}

bool QuicUnackedPacketMap::HasRetransmittableFrames(
    QuicPacketNumber packet_number) const {
  DCHECK_GE(packet_number, least_unacked_);
  DCHECK_LT(packet_number, least_unacked_ + unacked_packets_.size());
  return HasRetransmittableFrames(
      unacked_packets_[packet_number - least_unacked_]);
}

bool QuicUnackedPacketMap::HasRetransmittableFrames(
    const QuicTransmissionInfo& info) const {
  if (!session_decides_what_to_write_) {
    return !info.retransmittable_frames.empty();
  }

  if (!QuicUtils::IsAckable(info.state)) {
    return false;
  }

  for (const auto& frame : info.retransmittable_frames) {
    if (session_notifier_->IsFrameOutstanding(frame)) {
      return true;
    }
  }
  return false;
}

void QuicUnackedPacketMap::RemoveRetransmittability(
    QuicTransmissionInfo* info) {
  if (session_decides_what_to_write_) {
    DeleteFrames(&info->retransmittable_frames);
    if (fix_is_useful_for_retransmission_) {
      info->retransmission = 0;
    }
    return;
  }
  while (info->retransmission != 0) {
    const QuicPacketNumber retransmission = info->retransmission;
    info->retransmission = 0;
    info = &unacked_packets_[retransmission - least_unacked_];
  }

  if (info->has_crypto_handshake) {
    DCHECK(HasRetransmittableFrames(*info));
    DCHECK_LT(0u, pending_crypto_packet_count_);
    --pending_crypto_packet_count_;
    info->has_crypto_handshake = false;
  }
  DeleteFrames(&info->retransmittable_frames);
}

void QuicUnackedPacketMap::RemoveRetransmittability(
    QuicPacketNumber packet_number) {
  DCHECK_GE(packet_number, least_unacked_);
  DCHECK_LT(packet_number, least_unacked_ + unacked_packets_.size());
  QuicTransmissionInfo* info =
      &unacked_packets_[packet_number - least_unacked_];
  RemoveRetransmittability(info);
}

void QuicUnackedPacketMap::IncreaseLargestAcked(
    QuicPacketNumber largest_acked) {
  DCHECK_LE(largest_acked_, largest_acked);
  largest_acked_ = largest_acked;
}

bool QuicUnackedPacketMap::IsPacketUsefulForMeasuringRtt(
    QuicPacketNumber packet_number,
    const QuicTransmissionInfo& info) const {
  // Packet can be used for RTT measurement if it may yet be acked as the
  // largest observed packet by the receiver.
  return QuicUtils::IsAckable(info.state) && packet_number > largest_acked_;
}

bool QuicUnackedPacketMap::IsPacketUsefulForCongestionControl(
    const QuicTransmissionInfo& info) const {
  // Packet contributes to congestion control if it is considered inflight.
  return info.in_flight;
}

bool QuicUnackedPacketMap::IsPacketUsefulForRetransmittableData(
    const QuicTransmissionInfo& info) const {
  if (!session_decides_what_to_write_ || !fix_is_useful_for_retransmission_) {
    // Packet may have retransmittable frames, or the data may have been
    // retransmitted with a new packet number.
    // Allow for an extra 1 RTT before stopping to track old packets.
    return info.retransmission > largest_acked_ ||
           HasRetransmittableFrames(info);
  }

  // Wait for 1 RTT before giving up on the lost packet.
  if (info.retransmission > largest_acked_) {
    QUIC_FLAG_COUNT(quic_reloadable_flag_quic_fix_is_useful_for_retrans);
    return true;
  }
  return false;
}

bool QuicUnackedPacketMap::IsPacketUseless(
    QuicPacketNumber packet_number,
    const QuicTransmissionInfo& info) const {
  return !IsPacketUsefulForMeasuringRtt(packet_number, info) &&
         !IsPacketUsefulForCongestionControl(info) &&
         !IsPacketUsefulForRetransmittableData(info);
}

bool QuicUnackedPacketMap::IsUnacked(QuicPacketNumber packet_number) const {
  if (packet_number < least_unacked_ ||
      packet_number >= least_unacked_ + unacked_packets_.size()) {
    return false;
  }
  return !IsPacketUseless(packet_number,
                          unacked_packets_[packet_number - least_unacked_]);
}

void QuicUnackedPacketMap::RemoveFromInFlight(QuicTransmissionInfo* info) {
  if (info->in_flight) {
    QUIC_BUG_IF(bytes_in_flight_ < info->bytes_sent);
    bytes_in_flight_ -= info->bytes_sent;
    info->in_flight = false;
  }
}

void QuicUnackedPacketMap::RemoveFromInFlight(QuicPacketNumber packet_number) {
  DCHECK_GE(packet_number, least_unacked_);
  DCHECK_LT(packet_number, least_unacked_ + unacked_packets_.size());
  QuicTransmissionInfo* info =
      &unacked_packets_[packet_number - least_unacked_];
  RemoveFromInFlight(info);
}

void QuicUnackedPacketMap::CancelRetransmissionsForStream(
    QuicStreamId stream_id) {
  DCHECK(!session_decides_what_to_write_);
  QuicPacketNumber packet_number = least_unacked_;
  for (auto it = unacked_packets_.begin(); it != unacked_packets_.end();
       ++it, ++packet_number) {
    QuicFrames* frames = &it->retransmittable_frames;
    if (frames->empty()) {
      continue;
    }
    RemoveFramesForStream(frames, stream_id);
    if (frames->empty()) {
      RemoveRetransmittability(packet_number);
    }
  }
}

bool QuicUnackedPacketMap::HasInFlightPackets() const {
  return bytes_in_flight_ > 0;
}

const QuicTransmissionInfo& QuicUnackedPacketMap::GetTransmissionInfo(
    QuicPacketNumber packet_number) const {
  return unacked_packets_[packet_number - least_unacked_];
}

QuicTransmissionInfo* QuicUnackedPacketMap::GetMutableTransmissionInfo(
    QuicPacketNumber packet_number) {
  return &unacked_packets_[packet_number - least_unacked_];
}

QuicTime QuicUnackedPacketMap::GetLastPacketSentTime() const {
  auto it = unacked_packets_.rbegin();
  while (it != unacked_packets_.rend()) {
    if (it->in_flight) {
      QUIC_BUG_IF(it->sent_time == QuicTime::Zero())
          << "Sent time can never be zero for a packet in flight.";
      return it->sent_time;
    }
    ++it;
  }
  QUIC_BUG << "GetLastPacketSentTime requires in flight packets.";
  return QuicTime::Zero();
}

QuicTime QuicUnackedPacketMap::GetLastCryptoPacketSentTime() const {
  return last_crypto_packet_sent_time_;
}

size_t QuicUnackedPacketMap::GetNumUnackedPacketsDebugOnly() const {
  size_t unacked_packet_count = 0;
  QuicPacketNumber packet_number = least_unacked_;
  for (auto it = unacked_packets_.begin(); it != unacked_packets_.end();
       ++it, ++packet_number) {
    if (!IsPacketUseless(packet_number, *it)) {
      ++unacked_packet_count;
    }
  }
  return unacked_packet_count;
}

bool QuicUnackedPacketMap::HasMultipleInFlightPackets() const {
  if (bytes_in_flight_ > kDefaultTCPMSS) {
    return true;
  }
  size_t num_in_flight = 0;
  for (auto it = unacked_packets_.rbegin(); it != unacked_packets_.rend();
       ++it) {
    if (it->in_flight) {
      ++num_in_flight;
    }
    if (num_in_flight > 1) {
      return true;
    }
  }
  return false;
}

bool QuicUnackedPacketMap::HasPendingCryptoPackets() const {
  if (!session_decides_what_to_write_) {
    return pending_crypto_packet_count_ > 0;
  }
  return session_notifier_->HasUnackedCryptoData();
}

bool QuicUnackedPacketMap::HasUnackedRetransmittableFrames() const {
  DCHECK(!GetQuicReloadableFlag(quic_optimize_inflight_check));
  for (auto it = unacked_packets_.rbegin(); it != unacked_packets_.rend();
       ++it) {
    if (it->in_flight && HasRetransmittableFrames(*it)) {
      return true;
    }
  }
  return false;
}

QuicPacketNumber QuicUnackedPacketMap::GetLeastUnacked() const {
  return least_unacked_;
}

void QuicUnackedPacketMap::SetSessionNotifier(
    SessionNotifierInterface* session_notifier) {
  session_notifier_ = session_notifier;
}

bool QuicUnackedPacketMap::NotifyFramesAcked(const QuicTransmissionInfo& info,
                                             QuicTime::Delta ack_delay) {
  if (session_notifier_ == nullptr) {
    return false;
  }
  bool new_data_acked = false;
  for (const QuicFrame& frame : info.retransmittable_frames) {
    if (session_notifier_->OnFrameAcked(frame, ack_delay)) {
      new_data_acked = true;
    }
  }
  return new_data_acked;
}

void QuicUnackedPacketMap::NotifyFramesLost(const QuicTransmissionInfo& info,
                                            TransmissionType type) {
  DCHECK(session_decides_what_to_write_);
  for (const QuicFrame& frame : info.retransmittable_frames) {
    session_notifier_->OnFrameLost(frame);
  }
}

void QuicUnackedPacketMap::RetransmitFrames(const QuicTransmissionInfo& info,
                                            TransmissionType type) {
  DCHECK(session_decides_what_to_write_);
  session_notifier_->RetransmitFrames(info.retransmittable_frames, type);
}

void QuicUnackedPacketMap::MaybeAggregateAckedStreamFrame(
    const QuicTransmissionInfo& info,
    QuicTime::Delta ack_delay) {
  if (session_notifier_ == nullptr) {
    return;
  }
  for (const auto& frame : info.retransmittable_frames) {
    // Determine whether acked stream frame can be aggregated.
    const bool can_aggregate =
        frame.type == STREAM_FRAME &&
        frame.stream_frame.stream_id == aggregated_stream_frame_.stream_id &&
        frame.stream_frame.offset == aggregated_stream_frame_.offset +
                                         aggregated_stream_frame_.data_length &&
        // We would like to increment aggregated_stream_frame_.data_length by
        // frame.stream_frame.data_length, so we need to make sure their sum is
        // representable by QuicPacketLength, which is the type of the former.
        !WillStreamFrameLengthSumWrapAround(
            aggregated_stream_frame_.data_length,
            frame.stream_frame.data_length);

    if (can_aggregate) {
      // Aggregate stream frame.
      aggregated_stream_frame_.data_length += frame.stream_frame.data_length;
      aggregated_stream_frame_.fin = frame.stream_frame.fin;
      if (aggregated_stream_frame_.fin) {
        // Notify session notifier aggregated stream frame gets acked if fin is
        // acked.
        NotifyAggregatedStreamFrameAcked(ack_delay);
      }
      continue;
    }

    NotifyAggregatedStreamFrameAcked(ack_delay);
    if (frame.type != STREAM_FRAME || frame.stream_frame.fin) {
      session_notifier_->OnFrameAcked(frame, ack_delay);
      continue;
    }

    // Delay notifying session notifier stream frame gets acked in case it can
    // be aggregated with following acked ones.
    aggregated_stream_frame_.stream_id = frame.stream_frame.stream_id;
    aggregated_stream_frame_.offset = frame.stream_frame.offset;
    aggregated_stream_frame_.data_length = frame.stream_frame.data_length;
    aggregated_stream_frame_.fin = frame.stream_frame.fin;
  }
}

void QuicUnackedPacketMap::NotifyAggregatedStreamFrameAcked(
    QuicTime::Delta ack_delay) {
  if (aggregated_stream_frame_.stream_id == static_cast<QuicStreamId>(-1) ||
      session_notifier_ == nullptr) {
    // Aggregated stream frame is empty.
    return;
  }
  session_notifier_->OnFrameAcked(QuicFrame(aggregated_stream_frame_),
                                  ack_delay);
  // Clear aggregated stream frame.
  aggregated_stream_frame_.stream_id = -1;
}

void QuicUnackedPacketMap::SetSessionDecideWhatToWrite(
    bool session_decides_what_to_write) {
  if (largest_sent_packet_ > 0) {
    QUIC_BUG << "Cannot change session_decide_what_to_write with packets sent.";
    return;
  }
  session_decides_what_to_write_ = session_decides_what_to_write;
}

}  // namespace quic

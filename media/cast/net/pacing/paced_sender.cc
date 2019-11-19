// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/net/pacing/paced_sender.h"

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/numerics/safe_conversions.h"

namespace media {
namespace cast {

namespace {

static const int64_t kPacingIntervalMs = 10;
// Each frame will be split into no more than kPacingMaxBurstsPerFrame
// bursts of packets.
static const size_t kPacingMaxBurstsPerFrame = 3;
static const size_t kMaxDedupeWindowMs = 500;

}  // namespace

DedupInfo::DedupInfo() : last_byte_acked_for_audio(0) {}

PacketKey::PacketKey() : ssrc(0), packet_id(0) {}

PacketKey::PacketKey(base::TimeTicks capture_time,
                     uint32_t ssrc,
                     FrameId frame_id,
                     uint16_t packet_id)
    : capture_time(capture_time),
      ssrc(ssrc),
      frame_id(frame_id),
      packet_id(packet_id) {}

PacketKey::PacketKey(const PacketKey& other) = default;

PacketKey::~PacketKey() = default;

struct PacedSender::PacketSendRecord {
  PacketSendRecord()
      : last_byte_sent(0), last_byte_sent_for_audio(0), cancel_count(0) {}

  base::TimeTicks time;    // Time when the packet was sent.
  int64_t last_byte_sent;  // Number of bytes sent to network just after this
                           // packet was sent.
  int64_t last_byte_sent_for_audio;  // Number of bytes sent to network from
                                     // audio stream just before this packet.
  int cancel_count;  // Number of times the packet was canceled (debugging).
};

struct PacedSender::RtpSession {
  explicit RtpSession(bool is_audio_stream)
      : last_byte_sent(0), is_audio(is_audio_stream) {}
  RtpSession() = default;

  // Tracks recently-logged RTP timestamps so that it can expand the truncated
  // values found in packets.
  RtpTimeTicks last_logged_rtp_timestamp_;
  int64_t last_byte_sent;
  bool is_audio;
};

PacedSender::PacedSender(
    size_t target_burst_size,
    size_t max_burst_size,
    const base::TickClock* clock,
    std::vector<PacketEvent>* recent_packet_events,
    PacketTransport* transport,
    const scoped_refptr<base::SingleThreadTaskRunner>& transport_task_runner)
    : clock_(clock),
      recent_packet_events_(recent_packet_events),
      transport_(transport),
      transport_task_runner_(transport_task_runner),
      last_byte_sent_for_audio_(0),
      target_burst_size_(target_burst_size),
      max_burst_size_(max_burst_size),
      current_max_burst_size_(target_burst_size_),
      next_max_burst_size_(target_burst_size_),
      next_next_max_burst_size_(target_burst_size_),
      current_burst_size_(0),
      state_(State_Unblocked) {}

PacedSender::~PacedSender() = default;

void PacedSender::RegisterSsrc(uint32_t ssrc, bool is_audio) {
  if (sessions_.find(ssrc) != sessions_.end())
    DVLOG(1) << "Re-register ssrc: " << ssrc;

  sessions_[ssrc] = RtpSession(is_audio);
}

void PacedSender::RegisterPrioritySsrc(uint32_t ssrc) {
  priority_ssrcs_.push_back(ssrc);
}

int64_t PacedSender::GetLastByteSentForPacket(const PacketKey& packet_key) {
  PacketSendHistory::const_iterator it = send_history_.find(packet_key);
  if (it == send_history_.end())
    return 0;
  return it->second.last_byte_sent;
}

int64_t PacedSender::GetLastByteSentForSsrc(uint32_t ssrc) {
  auto it = sessions_.find(ssrc);
  // Return 0 for unknown session.
  if (it == sessions_.end())
    return 0;
  return it->second.last_byte_sent;
}

bool PacedSender::SendPackets(const SendPacketVector& packets) {
  if (packets.empty()) {
    return true;
  }
  const bool high_priority = IsHighPriority(packets.begin()->first);
  for (size_t i = 0; i < packets.size(); i++) {
    if (VLOG_IS_ON(2)) {
      PacketSendHistory::const_iterator history_it =
          send_history_.find(packets[i].first);
      if (history_it != send_history_.end() &&
          history_it->second.cancel_count > 0) {
        VLOG(2) << "PacedSender::SendPackets() called for packet CANCELED "
                << history_it->second.cancel_count << " times: "
                << "ssrc=" << packets[i].first.ssrc
                << ", frame_id=" << packets[i].first.frame_id
                << ", packet_id=" << packets[i].first.packet_id;
      }
    }

    DCHECK(IsHighPriority(packets[i].first) == high_priority);
    if (high_priority) {
      priority_packet_list_[packets[i].first] =
          make_pair(PacketType_Normal, packets[i].second);
    } else {
      packet_list_[packets[i].first] =
          make_pair(PacketType_Normal, packets[i].second);
    }
  }
  if (state_ == State_Unblocked) {
    SendStoredPackets();
  }
  return true;
}

bool PacedSender::ShouldResend(const PacketKey& packet_key,
                               const DedupInfo& dedup_info,
                               const base::TimeTicks& now) {
  PacketSendHistory::const_iterator it = send_history_.find(packet_key);

  // No history of previous transmission. It might be sent too long ago.
  if (it == send_history_.end())
    return true;

  // Suppose there is request to retransmit X and there is an audio
  // packet Y sent just before X. Reject retransmission of X if ACK for
  // Y has not been received.
  // Only do this for video packets.
  //
  // TODO(miu): This sounds wrong.  Audio packets are always transmitted first
  // (because they are put in |priority_packet_list_|, see PopNextPacket()).
  auto session_it = sessions_.find(packet_key.ssrc);
  // The session should always have been registered in |sessions_|.
  DCHECK(session_it != sessions_.end());
  if (!session_it->second.is_audio) {
    if (dedup_info.last_byte_acked_for_audio &&
        it->second.last_byte_sent_for_audio &&
        dedup_info.last_byte_acked_for_audio <
        it->second.last_byte_sent_for_audio) {
      return false;
    }
  }
  // Retransmission interval has to be greater than |resend_interval|.
  if (now - it->second.time < dedup_info.resend_interval)
    return false;
  return true;
}

bool PacedSender::ResendPackets(const SendPacketVector& packets,
                                const DedupInfo& dedup_info) {
  if (packets.empty()) {
    return true;
  }
  const bool high_priority = IsHighPriority(packets.begin()->first);
  const base::TimeTicks now = clock_->NowTicks();
  for (size_t i = 0; i < packets.size(); i++) {
    if (VLOG_IS_ON(2)) {
      PacketSendHistory::const_iterator history_it =
          send_history_.find(packets[i].first);
      if (history_it != send_history_.end() &&
          history_it->second.cancel_count > 0) {
        VLOG(2) << "PacedSender::ReendPackets() called for packet CANCELED "
                << history_it->second.cancel_count << " times: "
                << "ssrc=" << packets[i].first.ssrc
                << ", frame_id=" << packets[i].first.frame_id
                << ", packet_id=" << packets[i].first.packet_id;
      }
    }

    if (!ShouldResend(packets[i].first, dedup_info, now)) {
      LogPacketEvent(packets[i].second->data, PACKET_RTX_REJECTED);
      continue;
    }

    DCHECK(IsHighPriority(packets[i].first) == high_priority);
    if (high_priority) {
      priority_packet_list_[packets[i].first] =
          make_pair(PacketType_Resend, packets[i].second);
    } else {
      packet_list_[packets[i].first] =
          make_pair(PacketType_Resend, packets[i].second);
    }
  }
  if (state_ == State_Unblocked) {
    SendStoredPackets();
  }
  return true;
}

bool PacedSender::SendRtcpPacket(uint32_t ssrc, PacketRef packet) {
  if (state_ == State_TransportBlocked) {
    const PacketKey key(base::TimeTicks(), ssrc, FrameId::first(), 0);
    priority_packet_list_[key] = make_pair(PacketType_RTCP, packet);
  } else {
    // We pass the RTCP packets straight through.
    if (!transport_->SendPacket(
            packet,
            base::Bind(&PacedSender::SendStoredPackets,
                       weak_factory_.GetWeakPtr()))) {
      state_ = State_TransportBlocked;
    }
  }
  return true;
}

void PacedSender::CancelSendingPacket(const PacketKey& packet_key) {
  packet_list_.erase(packet_key);
  priority_packet_list_.erase(packet_key);

  if (VLOG_IS_ON(2)) {
    auto history_it = send_history_.find(packet_key);
    if (history_it != send_history_.end())
      ++history_it->second.cancel_count;
  }
}

PacketRef PacedSender::PopNextPacket(PacketType* packet_type,
                                     PacketKey* packet_key) {
  // Always pop from the priority list first.
  PacketList* list = !priority_packet_list_.empty() ?
      &priority_packet_list_ : &packet_list_;
  DCHECK(!list->empty());

  // Determine which packet in the frame should be popped by examining the
  // |send_history_| for prior transmission attempts.  Packets that have never
  // been transmitted will be popped first.  If all packets have transmitted
  // before, pop the one that has not been re-attempted for the longest time.
  auto it = list->begin();
  PacketKey last_key = it->first;
  last_key.packet_id = UINT16_C(0xffff);
  PacketSendHistory::const_iterator history_it =
      send_history_.lower_bound(it->first);
  base::TimeTicks earliest_send_time =
      base::TimeTicks() + base::TimeDelta::Max();
  auto found_it = it;
  while (true) {
    if (history_it == send_history_.end() || it->first < history_it->first) {
      // There is no send history for this packet, which means it has not been
      // transmitted yet.
      found_it = it;
      break;
    }

    DCHECK(it->first == history_it->first);
    if (history_it->second.time < earliest_send_time) {
      earliest_send_time = history_it->second.time;
      found_it = it;
    }

    // Advance to next packet for the current frame, or break if there are no
    // more.
    ++it;
    if (it == list->end() || last_key < it->first)
      break;

    // Advance to next history entry.  Since there may be "holes" in the packet
    // list (e.g., due to packets canceled for retransmission), it's possible
    // |history_it| will have to be advanced more than once even though |it| was
    // only advanced once.
    do {
      ++history_it;
    } while (history_it != send_history_.end() &&
             history_it->first < it->first);
  }

  *packet_type = found_it->second.first;
  *packet_key = found_it->first;
  PacketRef ret = found_it->second.second;
  list->erase(found_it);
  return ret;
}

bool PacedSender::IsHighPriority(const PacketKey& packet_key) const {
  return std::find(priority_ssrcs_.begin(), priority_ssrcs_.end(),
                   packet_key.ssrc) != priority_ssrcs_.end();
}

bool PacedSender::empty() const {
  return packet_list_.empty() && priority_packet_list_.empty();
}

size_t PacedSender::size() const {
  return packet_list_.size() + priority_packet_list_.size();
}

// This function can be called from three places:
// 1. User called one of the Send* functions and we were in an unblocked state.
// 2. state_ == State_TransportBlocked and the transport is calling us to
//    let us know that it's ok to send again.
// 3. state_ == State_BurstFull and there are still packets to send. In this
//    case we called PostDelayedTask on this function to start a new burst.
void PacedSender::SendStoredPackets() {
  State previous_state = state_;
  state_ = State_Unblocked;
  if (empty()) {
    return;
  }

  base::TimeTicks now = clock_->NowTicks();
  // I don't actually trust that PostDelayTask(x - now) will mean that
  // now >= x when the call happens, so check if the previous state was
  // State_BurstFull too.
  if (now >= burst_end_ || previous_state == State_BurstFull) {
    // Start a new burst.
    current_burst_size_ = 0;
    burst_end_ = now + base::TimeDelta::FromMilliseconds(kPacingIntervalMs);

    // The goal here is to try to send out the queued packets over the next
    // three bursts, while trying to keep the burst size below 10 if possible.
    // We have some evidence that sending more than 12 packets in a row doesn't
    // work very well, but we don't actually know why yet. Sending out packets
    // sooner is better than sending out packets later as that gives us more
    // time to re-send them if needed. So if we have less than 30 packets, just
    // send 10 at a time. If we have less than 60 packets, send n / 3 at a time.
    // if we have more than 60, we send 20 at a time. 20 packets is ~24Mbit/s
    // which is more bandwidth than the cast library should need, and sending
    // out more data per second is unlikely to be helpful.
    size_t max_burst_size = std::min(
        max_burst_size_,
        std::max(target_burst_size_, size() / kPacingMaxBurstsPerFrame));
    current_max_burst_size_ = std::max(next_max_burst_size_, max_burst_size);
    next_max_burst_size_ = std::max(next_next_max_burst_size_, max_burst_size);
    next_next_max_burst_size_ = max_burst_size;
  }

  base::Closure cb = base::Bind(&PacedSender::SendStoredPackets,
                                weak_factory_.GetWeakPtr());
  while (!empty()) {
    if (current_burst_size_ >= current_max_burst_size_) {
      transport_task_runner_->PostDelayedTask(FROM_HERE,
                                              cb,
                                              burst_end_ - now);
      state_ = State_BurstFull;
      return;
    }
    PacketType packet_type;
    PacketKey packet_key;
    PacketRef packet = PopNextPacket(&packet_type, &packet_key);
    PacketSendRecord* const send_record = &(send_history_[packet_key]);
    send_record->time = now;

    if (send_record->cancel_count > 0 && packet_type != PacketType_RTCP) {
      VLOG(2) << "PacedSender is sending a packet known to have been CANCELED "
              << send_record->cancel_count << " times: "
              << "ssrc=" << packet_key.ssrc
              << ", frame_id=" << packet_key.frame_id
              << ", packet_id=" << packet_key.packet_id;
    }

    switch (packet_type) {
      case PacketType_Resend:
        LogPacketEvent(packet->data, PACKET_RETRANSMITTED);
        break;
      case PacketType_Normal:
        LogPacketEvent(packet->data, PACKET_SENT_TO_NETWORK);
        break;
      case PacketType_RTCP:
        break;
    }

    const bool socket_blocked = !transport_->SendPacket(packet, cb);

    // Save the send record.
    send_record->last_byte_sent = transport_->GetBytesSent();
    send_record->last_byte_sent_for_audio = last_byte_sent_for_audio_;
    send_history_buffer_[packet_key] = *send_record;

    auto it = sessions_.find(packet_key.ssrc);
    // The session should always have been registered in |sessions_|.
    DCHECK(it != sessions_.end());
    it->second.last_byte_sent = send_record->last_byte_sent;
    if (it->second.is_audio)
      last_byte_sent_for_audio_ = send_record->last_byte_sent;

    if (socket_blocked) {
      state_ = State_TransportBlocked;
      return;
    }
    current_burst_size_++;
  }

  // Keep ~0.5 seconds of data (1000 packets).
  //
  // TODO(miu): This has no relation to the actual size of the frames, and so
  // there's no way to reason whether 1000 is enough or too much, or whatever.
  if (send_history_buffer_.size() >=
      max_burst_size_ * kMaxDedupeWindowMs / kPacingIntervalMs) {
    send_history_.swap(send_history_buffer_);
    send_history_buffer_.clear();
  }
  DCHECK_LE(send_history_buffer_.size(),
            max_burst_size_ * kMaxDedupeWindowMs / kPacingIntervalMs);
  state_ = State_Unblocked;
}

void PacedSender::LogPacketEvent(const Packet& packet, CastLoggingEvent type) {
  if (!recent_packet_events_)
    return;

  recent_packet_events_->push_back(PacketEvent());
  PacketEvent& event = recent_packet_events_->back();

  // Populate the new PacketEvent by parsing the wire-format |packet|.
  //
  // TODO(miu): This parsing logic belongs in RtpParser.
  event.timestamp = clock_->NowTicks();
  event.type = type;
  base::BigEndianReader reader(reinterpret_cast<const char*>(&packet[0]),
                               packet.size());
  bool success = reader.Skip(4);
  uint32_t truncated_rtp_timestamp;
  success &= reader.ReadU32(&truncated_rtp_timestamp);
  uint32_t ssrc;
  success &= reader.ReadU32(&ssrc);

  auto it = sessions_.find(ssrc);
  // The session should always have been registered in |sessions_|.
  DCHECK(it != sessions_.end());
  event.rtp_timestamp = it->second.last_logged_rtp_timestamp_ =
      it->second.last_logged_rtp_timestamp_.Expand(truncated_rtp_timestamp);
  event.media_type = it->second.is_audio ? AUDIO_EVENT : VIDEO_EVENT;
  success &= reader.Skip(2);
  success &= reader.ReadU16(&event.packet_id);
  success &= reader.ReadU16(&event.max_packet_id);
  event.size = base::checked_cast<uint32_t>(packet.size());
  DCHECK(success);
}

}  // namespace cast
}  // namespace media

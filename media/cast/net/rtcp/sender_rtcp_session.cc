// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <limits>
#include <utility>

#include "base/big_endian.h"
#include "base/time/time.h"
#include "media/cast/constants.h"
#include "media/cast/net/pacing/paced_sender.h"
#include "media/cast/net/rtcp/rtcp_builder.h"
#include "media/cast/net/rtcp/rtcp_defines.h"
#include "media/cast/net/rtcp/rtcp_utility.h"
#include "media/cast/net/rtcp/sender_rtcp_session.h"

namespace media {
namespace cast {

namespace {

enum {
  kStatsHistoryWindowMs = 10000,  // 10 seconds.

  // Reject packets that are 0.5 seconds older than
  // the newest packet we've seen so far. This protects internal
  // states from crazy routers. (Based on RRTR)
  // TODO(isheriff): This should be done better.
  // See https://crbug.com/569261
  kOutOfOrderMaxAgeMs = 500,
};

// Parse a NTP diff value into a base::TimeDelta.
base::TimeDelta ConvertFromNtpDiff(uint32_t ntp_delay) {
  int64_t delay_us =
      (ntp_delay & 0x0000ffff) * base::Time::kMicrosecondsPerSecond;
  delay_us >>= 16;
  delay_us +=
      ((ntp_delay & 0xffff0000) >> 16) * base::Time::kMicrosecondsPerSecond;
  return base::TimeDelta::FromMicroseconds(delay_us);
}

// A receiver frame event is identified by frame RTP timestamp, event timestamp
// and event type.
// A receiver packet event is identified by all of the above plus packet id.
// The key format is as follows:
// First uint64_t:
//   bits 0-11: zeroes (unused).
//   bits 12-15: event type ID.
//   bits 16-31: packet ID if packet event, 0 otherwise.
//   bits 32-63: RTP timestamp.
// Second uint64_t:
//   bits 0-63: event TimeTicks internal value.
std::pair<uint64_t, uint64_t> GetReceiverEventKey(
    RtpTimeTicks frame_rtp_timestamp,
    const base::TimeTicks& event_timestamp,
    uint8_t event_type,
    uint16_t packet_id_or_zero) {
  uint64_t value1 = event_type;
  value1 <<= 16;
  value1 |= packet_id_or_zero;
  value1 <<= 32;
  value1 |= frame_rtp_timestamp.lower_32_bits();
  return std::make_pair(
      value1, static_cast<uint64_t>(event_timestamp.ToInternalValue()));
}

}  // namespace

SenderRtcpSession::SenderRtcpSession(const base::TickClock* clock,
                                     PacedPacketSender* packet_sender,
                                     RtcpObserver* observer,
                                     uint32_t local_ssrc,
                                     uint32_t remote_ssrc)
    : clock_(clock),
      packet_sender_(packet_sender),
      local_ssrc_(local_ssrc),
      remote_ssrc_(remote_ssrc),
      rtcp_observer_(observer),
      largest_seen_timestamp_(base::TimeTicks::FromInternalValue(
          std::numeric_limits<int64_t>::min())),
      parser_(local_ssrc, remote_ssrc) {}

SenderRtcpSession::~SenderRtcpSession() = default;

void SenderRtcpSession::WillSendFrame(FrameId frame_id) {
  if (parser_.max_valid_frame_id().is_null() ||
      frame_id > parser_.max_valid_frame_id()) {
    parser_.SetMaxValidFrameId(frame_id);
  }
}

bool SenderRtcpSession::IncomingRtcpPacket(const uint8_t* data, size_t length) {
  // Check if this is a valid RTCP packet.
  if (!IsRtcpPacket(data, length)) {
    VLOG(1) << "Rtcp@" << this << "::IncomingRtcpPacket() -- "
            << "Received an invalid (non-RTCP?) packet.";
    return false;
  }

  // Check if this packet is to us.
  uint32_t ssrc_of_sender = GetSsrcOfSender(data, length);
  if (ssrc_of_sender != remote_ssrc_) {
    return false;
  }

  // Parse this packet.
  base::BigEndianReader reader(reinterpret_cast<const char*>(data), length);
  if (parser_.Parse(&reader)) {
    if (parser_.has_picture_loss_indicator())
      rtcp_observer_->OnReceivedPli();
    if (parser_.has_receiver_reference_time_report()) {
      base::TimeTicks t = ConvertNtpToTimeTicks(
          parser_.receiver_reference_time_report().ntp_seconds,
          parser_.receiver_reference_time_report().ntp_fraction);
      if (t > largest_seen_timestamp_) {
        largest_seen_timestamp_ = t;
      } else if ((largest_seen_timestamp_ - t).InMilliseconds() >
                 kOutOfOrderMaxAgeMs) {
        // Reject packet, it is too old.
        VLOG(1) << "Rejecting RTCP packet as it is too old ("
                << (largest_seen_timestamp_ - t).InMilliseconds() << " ms)";
        return true;
      }
    }
    if (parser_.has_receiver_log()) {
      if (DedupeReceiverLog(parser_.mutable_receiver_log())) {
        rtcp_observer_->OnReceivedReceiverLog(parser_.receiver_log());
      }
    }
    if (parser_.has_last_report()) {
      OnReceivedDelaySinceLastReport(parser_.last_report(),
                                     parser_.delay_since_last_report());
    }
    if (parser_.has_cast_message()) {
      rtcp_observer_->OnReceivedCastMessage(parser_.cast_message());
    }
  }
  return true;
}

void SenderRtcpSession::OnReceivedDelaySinceLastReport(
    uint32_t last_report,
    uint32_t delay_since_last_report) {
  auto it = last_reports_sent_map_.find(last_report);
  if (it == last_reports_sent_map_.end()) {
    return;  // Feedback on another report.
  }

  const base::TimeDelta sender_delay = clock_->NowTicks() - it->second;
  const base::TimeDelta receiver_delay =
      ConvertFromNtpDiff(delay_since_last_report);
  current_round_trip_time_ = sender_delay - receiver_delay;
  // If the round trip time was computed as less than 1 ms, assume clock
  // imprecision by one or both peers caused a bad value to be calculated.
  // While plenty of networks do easily achieve less than 1 ms round trip time,
  // such a level of precision cannot be measured with our approach; and 1 ms is
  // good enough to represent "under 1 ms" for our use cases.
  current_round_trip_time_ =
      std::max(current_round_trip_time_, base::TimeDelta::FromMilliseconds(1));

  rtcp_observer_->OnReceivedRtt(current_round_trip_time_);
}

void SenderRtcpSession::SaveLastSentNtpTime(const base::TimeTicks& now,
                                            uint32_t last_ntp_seconds,
                                            uint32_t last_ntp_fraction) {
  // Make sure |now| is always greater than the last element in
  // |last_reports_sent_queue_|.
  if (!last_reports_sent_queue_.empty()) {
    DCHECK(now >= last_reports_sent_queue_.back().second);
  }

  uint32_t last_report = ConvertToNtpDiff(last_ntp_seconds, last_ntp_fraction);
  last_reports_sent_map_[last_report] = now;
  last_reports_sent_queue_.push(std::make_pair(last_report, now));

  const base::TimeTicks timeout =
      now - base::TimeDelta::FromMilliseconds(kStatsHistoryWindowMs);

  // Cleanup old statistics older than |timeout|.
  while (!last_reports_sent_queue_.empty()) {
    RtcpSendTimePair oldest_report = last_reports_sent_queue_.front();
    if (oldest_report.second < timeout) {
      last_reports_sent_map_.erase(oldest_report.first);
      last_reports_sent_queue_.pop();
    } else {
      break;
    }
  }
}

bool SenderRtcpSession::DedupeReceiverLog(
    RtcpReceiverLogMessage* receiver_log) {
  auto i = receiver_log->begin();
  while (i != receiver_log->end()) {
    RtcpReceiverEventLogMessages* messages = &i->event_log_messages_;
    auto j = messages->begin();
    while (j != messages->end()) {
      ReceiverEventKey key = GetReceiverEventKey(
          i->rtp_timestamp_, j->event_timestamp, j->type, j->packet_id);
      auto tmp = j;
      ++j;
      if (receiver_event_key_set_.insert(key).second) {
        receiver_event_key_queue_.push(key);
        if (receiver_event_key_queue_.size() > kReceiverRtcpEventHistorySize) {
          receiver_event_key_set_.erase(receiver_event_key_queue_.front());
          receiver_event_key_queue_.pop();
        }
      } else {
        messages->erase(tmp);
      }
    }

    auto tmp = i;
    ++i;
    if (messages->empty()) {
      receiver_log->erase(tmp);
    }
  }
  return !receiver_log->empty();
}

void SenderRtcpSession::SendRtcpReport(
    base::TimeTicks current_time,
    RtpTimeTicks current_time_as_rtp_timestamp,
    uint32_t send_packet_count,
    size_t send_octet_count) {
  uint32_t current_ntp_seconds = 0;
  uint32_t current_ntp_fractions = 0;
  ConvertTimeTicksToNtp(current_time, &current_ntp_seconds,
                        &current_ntp_fractions);
  SaveLastSentNtpTime(current_time, current_ntp_seconds, current_ntp_fractions);

  RtcpSenderInfo sender_info;
  sender_info.ntp_seconds = current_ntp_seconds;
  sender_info.ntp_fraction = current_ntp_fractions;
  sender_info.rtp_timestamp = current_time_as_rtp_timestamp;
  sender_info.send_packet_count = send_packet_count;
  sender_info.send_octet_count = send_octet_count;

  RtcpBuilder rtcp_builder(local_ssrc_);
  packet_sender_->SendRtcpPacket(local_ssrc_,
                                 rtcp_builder.BuildRtcpFromSender(sender_info));
}

}  // namespace cast
}  // namespace media

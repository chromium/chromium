// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/test/receiver/cast_message_builder.h"

#include "base/logging.h"
#include "media/cast/constants.h"
#include "media/cast/test/receiver/framer.h"

namespace media {
namespace cast {

namespace {

// Interval between sending of ACK/NACK Cast Message RTCP packets back to the
// sender.
constexpr base::TimeDelta kCastMessageUpdateInterval = base::Milliseconds(33);

// Interval between repeating a NACK for packets in the same frame.
constexpr base::TimeDelta kNackRepeatInterval = base::Milliseconds(30);

}  // namespace

CastMessageBuilder::CastMessageBuilder(
    const base::TickClock* clock,
    RtpPayloadFeedback* incoming_payload_feedback,
    const Framer* framer,
    uint32_t media_ssrc,
    bool decoder_faster_than_max_frame_rate,
    int max_unacked_frames)
    : clock_(clock),
      cast_feedback_(incoming_payload_feedback),
      framer_(framer),
      media_ssrc_(media_ssrc),
      decoder_faster_than_max_frame_rate_(decoder_faster_than_max_frame_rate),
      max_unacked_frames_(max_unacked_frames),
      cast_msg_(media_ssrc),
      slowing_down_ack_(false),
      acked_last_frame_(true) {
  cast_msg_.ack_frame_id = FrameId::first() - 1;
}

CastMessageBuilder::~CastMessageBuilder() = default;

void CastMessageBuilder::CompleteFrameReceived(FrameId frame_id) {
  DCHECK_GE(frame_id, last_acked_frame_id());
  VLOG(2) << "CompleteFrameReceived: " << frame_id;
  if (last_update_time_.is_null()) {
    // Our first update.
    last_update_time_ = clock_->NowTicks();
  }

  if (!UpdateAckMessage(frame_id)) {
    return;
  }
  BuildPacketList();

  // Send cast message.
  VLOG(2) << "Send cast message Ack:" << frame_id;
  cast_feedback_->CastFeedback(cast_msg_);
}

bool CastMessageBuilder::UpdateAckMessage(FrameId frame_id) {
  if (!decoder_faster_than_max_frame_rate_) {
    int complete_frame_count = framer_->NumberOfCompleteFrames();
    if (complete_frame_count > max_unacked_frames_) {
      // We have too many frames pending in our framer; slow down ACK.
      if (!slowing_down_ack_) {
        slowing_down_ack_ = true;
        ack_queue_.push_back(last_acked_frame_id());
      }
    } else if (complete_frame_count <= 1) {
      // We are down to one or less frames in our framer; ACK normally.
      slowing_down_ack_ = false;
      ack_queue_.clear();
    }
  }

  if (slowing_down_ack_) {
    // We are slowing down acknowledgment by acknowledging every other frame.
    // Note: frame skipping and slowdown ACK is not supported at the same
    // time; and it's not needed since we can skip frames to catch up.
    if (!ack_queue_.empty() && ack_queue_.back() == frame_id) {
      return false;
    }
    ack_queue_.push_back(frame_id);
    if (!acked_last_frame_) {
      ack_queue_.pop_front();
    }
    frame_id = ack_queue_.front();
  }

  // Is it a new frame?
  if (last_acked_frame_id() == frame_id) {
    acked_last_frame_ = false;
    return false;
  }
  acked_last_frame_ = true;
  cast_msg_.ack_frame_id = frame_id;
  cast_msg_.missing_frames_and_packets.clear();
  last_update_time_ = clock_->NowTicks();
  time_last_nacked_map_.erase(
      time_last_nacked_map_.begin(),
      time_last_nacked_map_.upper_bound(last_acked_frame_id()));
  return true;
}

bool CastMessageBuilder::TimeToSendNextCastMessage(
    base::TimeTicks* time_to_send) {
  // We haven't received any packets.
  if (last_update_time_.is_null() && framer_->Empty())
    return false;

  *time_to_send = last_update_time_ + kCastMessageUpdateInterval;
  return true;
}

void CastMessageBuilder::UpdateCastMessage() {
  RtcpCastMessage message(media_ssrc_);
  if (!UpdateCastMessageInternal(&message))
    return;

  // Send cast message.
  cast_feedback_->CastFeedback(message);
}

bool CastMessageBuilder::UpdateCastMessageInternal(RtcpCastMessage* message) {
  if (last_update_time_.is_null()) {
    if (!framer_->Empty()) {
      // We have received packets.
      last_update_time_ = clock_->NowTicks();
    }
    return false;
  }

  // Is it time to update the cast message?
  base::TimeTicks now = clock_->NowTicks();
  if (now - last_update_time_ < kCastMessageUpdateInterval) {
    return false;
  }
  last_update_time_ = now;

  // Needed to cover when a frame is skipped.
  UpdateAckMessage(last_acked_frame_id());
  BuildPacketList();
  *message = cast_msg_;
  return true;
}

void CastMessageBuilder::BuildPacketList() {
  base::TimeTicks now = clock_->NowTicks();

  // Clear message NACK list.
  cast_msg_.missing_frames_and_packets.clear();

  // Are we missing packets?
  if (framer_->Empty())
    return;

  const FrameId newest_frame_id = framer_->newest_frame_id();
  FrameId next_expected_frame_id = last_acked_frame_id() + 1;

  // Iterate over all frames.
  for (; next_expected_frame_id <= newest_frame_id; ++next_expected_frame_id) {
    const auto it = time_last_nacked_map_.find(next_expected_frame_id);
    if (it != time_last_nacked_map_.end()) {
      // We have sent a NACK in this frame before, make sure enough time have
      // passed.
      if (now - it->second < kNackRepeatInterval) {
        continue;
      }
    }

    PacketIdSet missing;
    if (framer_->FrameExists(next_expected_frame_id)) {
      bool last_frame = (newest_frame_id == next_expected_frame_id);
      framer_->GetMissingPackets(next_expected_frame_id, last_frame, &missing);
      if (!missing.empty()) {
        time_last_nacked_map_[next_expected_frame_id] = now;
        cast_msg_.missing_frames_and_packets.insert(
            std::make_pair(next_expected_frame_id, missing));
      }
    } else {
      time_last_nacked_map_[next_expected_frame_id] = now;
      missing.insert(kRtcpCastAllPacketsLost);
      cast_msg_.missing_frames_and_packets[next_expected_frame_id] = missing;
    }
  }
}

}  //  namespace cast
}  //  namespace media

// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/frame_sender.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "media/cast/constants.h"
#include "media/cast/sender/sender_encoded_frame.h"

namespace media {
namespace cast {
namespace {

constexpr int kNumAggressiveReportsSentAtStart = 100;
constexpr base::TimeDelta kMinSchedulingDelay =
    base::TimeDelta::FromMilliseconds(1);
constexpr base::TimeDelta kReceiverProcessTime =
    base::TimeDelta::FromMilliseconds(250);

// The additional number of frames that can be in-flight when input exceeds the
// maximum frame rate.
constexpr int kMaxFrameBurst = 5;

}  // namespace

// Convenience macro used in logging statements throughout this file.
#define SENDER_SSRC (is_audio_ ? "AUDIO[" : "VIDEO[") << ssrc_ << "] "

FrameSender::RtcpClient::RtcpClient(base::WeakPtr<FrameSender> frame_sender)
    : frame_sender_(frame_sender) {}

FrameSender::RtcpClient::~RtcpClient() = default;

void FrameSender::RtcpClient::OnReceivedCastMessage(
    const RtcpCastMessage& cast_message) {
  if (frame_sender_)
    frame_sender_->OnReceivedCastFeedback(cast_message);
}

void FrameSender::RtcpClient::OnReceivedRtt(base::TimeDelta round_trip_time) {
  if (frame_sender_)
    frame_sender_->OnMeasuredRoundTripTime(round_trip_time);
}

void FrameSender::RtcpClient::OnReceivedPli() {
  if (frame_sender_)
    frame_sender_->OnReceivedPli();
}

FrameSender::FrameSender(scoped_refptr<CastEnvironment> cast_environment,
                         CastTransport* const transport_sender,
                         const FrameSenderConfig& config,
                         CongestionControl* congestion_control)
    : cast_environment_(cast_environment),
      transport_sender_(transport_sender),
      ssrc_(config.sender_ssrc),
      min_playout_delay_(config.min_playout_delay.is_zero()
                             ? config.max_playout_delay
                             : config.min_playout_delay),
      max_playout_delay_(config.max_playout_delay),
      animated_playout_delay_(config.animated_playout_delay.is_zero()
                                  ? config.max_playout_delay
                                  : config.animated_playout_delay),
      send_target_playout_delay_(false),
      max_frame_rate_(config.max_frame_rate),
      num_aggressive_rtcp_reports_sent_(0),
      duplicate_ack_counter_(0),
      congestion_control_(congestion_control),
      picture_lost_at_receiver_(false),
      rtp_timebase_(config.rtp_timebase),
      is_audio_(config.rtp_payload_type <= RtpPayloadType::AUDIO_LAST),
      max_ack_delay_(config.max_playout_delay) {
  DCHECK(transport_sender_);
  DCHECK_GT(rtp_timebase_, 0);
  DCHECK(congestion_control_);
  // We assume animated content to begin with since that is the common use
  // case today.
  VLOG(1) << SENDER_SSRC << "min latency "
          << min_playout_delay_.InMilliseconds() << "max latency "
          << max_playout_delay_.InMilliseconds() << "animated latency "
          << animated_playout_delay_.InMilliseconds();
  SetTargetPlayoutDelay(animated_playout_delay_);

  CastTransportRtpConfig transport_config;
  transport_config.ssrc = config.sender_ssrc;
  transport_config.feedback_ssrc = config.receiver_ssrc;
  transport_config.rtp_payload_type = config.rtp_payload_type;
  transport_config.aes_key = config.aes_key;
  transport_config.aes_iv_mask = config.aes_iv_mask;

  transport_sender->InitializeStream(
      transport_config,
      std::make_unique<FrameSender::RtcpClient>(weak_factory_.GetWeakPtr()));
}

FrameSender::~FrameSender() = default;

void FrameSender::ScheduleNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::BindRepeating(&FrameSender::SendRtcpReport,
                          weak_factory_.GetWeakPtr(), true),
      base::TimeDelta::FromMilliseconds(kRtcpReportIntervalMs));
}

void FrameSender::SendRtcpReport(bool schedule_future_reports) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  // Sanity-check: We should have sent at least the first frame by this point.
  DCHECK(!last_send_time_.is_null());

  // Create lip-sync info for the sender report.  The last sent frame's
  // reference time and RTP timestamp are used to estimate an RTP timestamp in
  // terms of "now."  Note that |now| is never likely to be precise to an exact
  // frame boundary; and so the computation here will result in a
  // |now_as_rtp_timestamp| value that is rarely equal to any one emitted by the
  // encoder.
  const base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  const base::TimeDelta time_delta =
      now - GetRecordedReferenceTime(last_sent_frame_id_);
  const RtpTimeDelta rtp_delta =
      RtpTimeDelta::FromTimeDelta(time_delta, rtp_timebase_);
  const RtpTimeTicks now_as_rtp_timestamp =
      GetRecordedRtpTimestamp(last_sent_frame_id_) + rtp_delta;
  transport_sender_->SendSenderReport(ssrc_, now, now_as_rtp_timestamp);

  if (schedule_future_reports)
    ScheduleNextRtcpReport();
}

void FrameSender::OnMeasuredRoundTripTime(base::TimeDelta round_trip_time) {
  DCHECK_GT(round_trip_time, base::TimeDelta());
  current_round_trip_time_ = round_trip_time;
  max_ack_delay_ = 2 * std::max(current_round_trip_time_, base::TimeDelta()) +
                   kReceiverProcessTime;
  max_ack_delay_ = std::min(max_ack_delay_, target_playout_delay_);
}

void FrameSender::SetTargetPlayoutDelay(
    base::TimeDelta new_target_playout_delay) {
  if (send_target_playout_delay_ &&
      target_playout_delay_ == new_target_playout_delay) {
    return;
  }
  new_target_playout_delay = std::max(new_target_playout_delay,
                                      min_playout_delay_);
  new_target_playout_delay = std::min(new_target_playout_delay,
                                      max_playout_delay_);
  VLOG(2) << SENDER_SSRC << "Target playout delay changing from "
          << target_playout_delay_.InMilliseconds() << " ms to "
          << new_target_playout_delay.InMilliseconds() << " ms.";
  target_playout_delay_ = new_target_playout_delay;
  max_ack_delay_ = std::min(max_ack_delay_, target_playout_delay_);
  send_target_playout_delay_ = true;
  congestion_control_->UpdateTargetPlayoutDelay(target_playout_delay_);
}

void FrameSender::ResendCheck() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!last_send_time_.is_null());
  const base::TimeDelta time_since_last_send =
      cast_environment_->Clock()->NowTicks() - last_send_time_;
  if (time_since_last_send > max_ack_delay_) {
    if (latest_acked_frame_id_ == last_sent_frame_id_) {
      // Last frame acked, no point in doing anything
    } else {
      VLOG(1) << SENDER_SSRC << "ACK timeout; last acked frame: "
              << latest_acked_frame_id_;
      ResendForKickstart();
    }
  }
  ScheduleNextResendCheck();
}

void FrameSender::ScheduleNextResendCheck() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!last_send_time_.is_null());
  base::TimeDelta time_to_next =
      last_send_time_ - cast_environment_->Clock()->NowTicks() + max_ack_delay_;
  time_to_next = std::max(time_to_next, kMinSchedulingDelay);
  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::BindRepeating(&FrameSender::ResendCheck,
                          weak_factory_.GetWeakPtr()),
      time_to_next);
}

void FrameSender::ResendForKickstart() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!last_send_time_.is_null());
  VLOG(1) << SENDER_SSRC << "Resending last packet of frame "
          << last_sent_frame_id_ << " to kick-start.";
  last_send_time_ = cast_environment_->Clock()->NowTicks();
  transport_sender_->ResendFrameForKickstart(ssrc_, last_sent_frame_id_);
}

void FrameSender::RecordLatestFrameTimestamps(FrameId frame_id,
                                              base::TimeTicks reference_time,
                                              RtpTimeTicks rtp_timestamp) {
  DCHECK(!reference_time.is_null());
  frame_reference_times_[frame_id.lower_8_bits()] = reference_time;
  frame_rtp_timestamps_[frame_id.lower_8_bits()] = rtp_timestamp;
}

base::TimeTicks FrameSender::GetRecordedReferenceTime(FrameId frame_id) const {
  return frame_reference_times_[frame_id.lower_8_bits()];
}

RtpTimeTicks FrameSender::GetRecordedRtpTimestamp(FrameId frame_id) const {
  return frame_rtp_timestamps_[frame_id.lower_8_bits()];
}

int FrameSender::GetUnacknowledgedFrameCount() const {
  if (last_send_time_.is_null())
    return 0;
  const int count = last_sent_frame_id_ - latest_acked_frame_id_;
  DCHECK_GE(count, 0);
  return count;
}

base::TimeDelta FrameSender::GetAllowedInFlightMediaDuration() const {
  // The total amount allowed in-flight media should equal the amount that fits
  // within the entire playout delay window, plus the amount of time it takes to
  // receive an ACK from the receiver.
  // TODO(miu): Research is needed, but there is likely a better formula.
  return target_playout_delay_ + (current_round_trip_time_ / 2);
}

void FrameSender::SendEncodedFrame(
    int requested_bitrate_before_encode,
    std::unique_ptr<SenderEncodedFrame> encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  VLOG(2) << SENDER_SSRC << "About to send another frame: last_sent="
          << last_sent_frame_id_ << ", latest_acked=" << latest_acked_frame_id_;

  const FrameId frame_id = encoded_frame->frame_id;
  const bool is_first_frame_to_be_sent = last_send_time_.is_null();

  if (picture_lost_at_receiver_ &&
      (encoded_frame->dependency == EncodedFrame::KEY)) {
    picture_lost_at_receiver_ = false;
    DCHECK(frame_id > latest_acked_frame_id_);
    // Cancel sending remaining frames.
    std::vector<FrameId> cancel_sending_frames;
    for (FrameId id = latest_acked_frame_id_ + 1; id < frame_id; ++id) {
      cancel_sending_frames.push_back(id);
    }
    transport_sender_->CancelSendingFrames(ssrc_, cancel_sending_frames);
    OnCancelSendingFrames();
  }

  last_send_time_ = cast_environment_->Clock()->NowTicks();
  last_sent_frame_id_ = frame_id;
  // If this is the first frame about to be sent, fake the value of
  // |latest_acked_frame_id_| to indicate the receiver starts out all caught up.
  // Also, schedule the periodic frame re-send checks.
  if (is_first_frame_to_be_sent) {
    latest_acked_frame_id_ = frame_id - 1;
    ScheduleNextResendCheck();
  }

  VLOG_IF(1, !is_audio_ && encoded_frame->dependency == EncodedFrame::KEY)
      << SENDER_SSRC << "Sending encoded key frame, id=" << frame_id;

  std::unique_ptr<FrameEvent> encode_event(new FrameEvent());
  encode_event->timestamp = encoded_frame->encode_completion_time;
  encode_event->type = FRAME_ENCODED;
  encode_event->media_type = is_audio_ ? AUDIO_EVENT : VIDEO_EVENT;
  encode_event->rtp_timestamp = encoded_frame->rtp_timestamp;
  encode_event->frame_id = frame_id;
  encode_event->size = base::checked_cast<uint32_t>(encoded_frame->data.size());
  encode_event->key_frame = encoded_frame->dependency == EncodedFrame::KEY;
  encode_event->target_bitrate = requested_bitrate_before_encode;
  encode_event->encoder_cpu_utilization = encoded_frame->encoder_utilization;
  encode_event->idealized_bitrate_utilization =
      encoded_frame->lossy_utilization;
  cast_environment_->logger()->DispatchFrameEvent(std::move(encode_event));

  RecordLatestFrameTimestamps(frame_id,
                              encoded_frame->reference_time,
                              encoded_frame->rtp_timestamp);

  if (!is_audio_) {
    // Used by chrome/browser/extension/api/cast_streaming/performance_test.cc
    TRACE_EVENT_INSTANT1(
        "cast_perf_test", "VideoFrameEncoded",
        TRACE_EVENT_SCOPE_THREAD,
        "rtp_timestamp", encoded_frame->rtp_timestamp.lower_32_bits());
  }

  // At the start of the session, it's important to send reports before each
  // frame so that the receiver can properly compute playout times.  The reason
  // more than one report is sent is because transmission is not guaranteed,
  // only best effort, so send enough that one should almost certainly get
  // through.
  if (num_aggressive_rtcp_reports_sent_ < kNumAggressiveReportsSentAtStart) {
    // SendRtcpReport() will schedule future reports to be made if this is the
    // last "aggressive report."
    ++num_aggressive_rtcp_reports_sent_;
    const bool is_last_aggressive_report =
        (num_aggressive_rtcp_reports_sent_ == kNumAggressiveReportsSentAtStart);
    VLOG_IF(1, is_last_aggressive_report)
        << SENDER_SSRC << "Sending last aggressive report.";
    SendRtcpReport(is_last_aggressive_report);
  }

  congestion_control_->SendFrameToTransport(
      frame_id, encoded_frame->data.size() * 8, last_send_time_);

  if (send_target_playout_delay_) {
    encoded_frame->new_playout_delay_ms =
        target_playout_delay_.InMilliseconds();
  }

  TRACE_EVENT_ASYNC_BEGIN1("cast.stream",
                           is_audio_ ? "Audio Transport" : "Video Transport",
                           frame_id.lower_32_bits(), "rtp_timestamp",
                           encoded_frame->rtp_timestamp.lower_32_bits());
  transport_sender_->InsertFrame(ssrc_, *encoded_frame);
}

void FrameSender::OnCancelSendingFrames() {}

void FrameSender::OnReceivedCastFeedback(const RtcpCastMessage& cast_feedback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  const bool have_valid_rtt = current_round_trip_time_ > base::TimeDelta();
  if (have_valid_rtt) {
    congestion_control_->UpdateRtt(current_round_trip_time_);

    // Having the RTT value implies the receiver sent back a receiver report
    // based on it having received a report from here.  Therefore, ensure this
    // sender stops aggressively sending reports.
    if (num_aggressive_rtcp_reports_sent_ < kNumAggressiveReportsSentAtStart) {
      VLOG(1) << SENDER_SSRC
              << "No longer a need to send reports aggressively (sent "
              << num_aggressive_rtcp_reports_sent_ << ").";
      num_aggressive_rtcp_reports_sent_ = kNumAggressiveReportsSentAtStart;
      ScheduleNextRtcpReport();
    }
  }

  if (last_send_time_.is_null())
    return;  // Cannot get an ACK without having first sent a frame.

  if (cast_feedback.missing_frames_and_packets.empty() &&
      cast_feedback.received_later_frames.empty()) {
    if (latest_acked_frame_id_ == cast_feedback.ack_frame_id) {
      VLOG(1) << SENDER_SSRC << "Received duplicate ACK for frame "
              << latest_acked_frame_id_;
      TRACE_EVENT_INSTANT2(
          "cast.stream", "Duplicate ACK", TRACE_EVENT_SCOPE_THREAD,
          "ack_frame_id", cast_feedback.ack_frame_id.lower_32_bits(),
          "last_sent_frame_id", last_sent_frame_id_.lower_32_bits());
    }
    // We only count duplicate ACKs when we have sent newer frames.
    if (latest_acked_frame_id_ == cast_feedback.ack_frame_id &&
        latest_acked_frame_id_ != last_sent_frame_id_) {
      duplicate_ack_counter_++;
    } else {
      duplicate_ack_counter_ = 0;
    }
    // TODO(miu): The values "2" and "3" should be derived from configuration.
    if (duplicate_ack_counter_ >= 2 && duplicate_ack_counter_ % 3 == 2) {
      ResendForKickstart();
    }
  } else {
    // Only count duplicated ACKs if there is no NACK request in between.
    // This is to avoid aggresive resend.
    duplicate_ack_counter_ = 0;
  }

  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  congestion_control_->AckFrame(cast_feedback.ack_frame_id, now);
  if (!cast_feedback.received_later_frames.empty()) {
    // Ack the received frames.
    congestion_control_->AckLaterFrames(cast_feedback.received_later_frames,
                                        now);
  }

  std::unique_ptr<FrameEvent> ack_event(new FrameEvent());
  ack_event->timestamp = now;
  ack_event->type = FRAME_ACK_RECEIVED;
  ack_event->media_type = is_audio_ ? AUDIO_EVENT : VIDEO_EVENT;
  ack_event->rtp_timestamp =
      GetRecordedRtpTimestamp(cast_feedback.ack_frame_id);
  ack_event->frame_id = cast_feedback.ack_frame_id;
  cast_environment_->logger()->DispatchFrameEvent(std::move(ack_event));

  const bool is_acked_out_of_order =
      cast_feedback.ack_frame_id < latest_acked_frame_id_;
  VLOG(2) << SENDER_SSRC
          << "Received ACK" << (is_acked_out_of_order ? " out-of-order" : "")
          << " for frame " << cast_feedback.ack_frame_id;
  if (is_acked_out_of_order) {
    TRACE_EVENT_INSTANT2(
        "cast.stream", "ACK out of order", TRACE_EVENT_SCOPE_THREAD,
        "ack_frame_id", cast_feedback.ack_frame_id.lower_32_bits(),
        "latest_acked_frame_id", latest_acked_frame_id_.lower_32_bits());
  } else if (latest_acked_frame_id_ < cast_feedback.ack_frame_id) {
    // Cancel resends of acked frames.
    std::vector<FrameId> frames_to_cancel;
    frames_to_cancel.reserve(cast_feedback.ack_frame_id -
                             latest_acked_frame_id_);
    do {
      ++latest_acked_frame_id_;
      frames_to_cancel.push_back(latest_acked_frame_id_);
      // This is a good place to match the trace for frame ids
      // since this ensures we not only track frame ids that are
      // implicitly ACKed, but also handles duplicate ACKs
      TRACE_EVENT_ASYNC_END1(
          "cast.stream", is_audio_ ? "Audio Transport" : "Video Transport",
          latest_acked_frame_id_.lower_32_bits(), "RTT_usecs",
          current_round_trip_time_.InMicroseconds());
    } while (latest_acked_frame_id_ < cast_feedback.ack_frame_id);
    transport_sender_->CancelSendingFrames(ssrc_, frames_to_cancel);
    OnCancelSendingFrames();
  }
}

void FrameSender::OnReceivedPli() {
  picture_lost_at_receiver_ = true;
}

bool FrameSender::ShouldDropNextFrame(base::TimeDelta frame_duration) const {
  // Check that accepting the next frame won't cause more frames to become
  // in-flight than the system's design limit.
  const int count_frames_in_flight =
      GetUnacknowledgedFrameCount() + GetNumberOfFramesInEncoder();
  if (count_frames_in_flight >= kMaxUnackedFrames) {
    VLOG(1) << SENDER_SSRC << "Dropping: Too many frames would be in-flight.";
    return true;
  }

  // Check that accepting the next frame won't exceed the configured maximum
  // frame rate, allowing for short-term bursts.
  base::TimeDelta duration_in_flight = GetInFlightMediaDuration();
  const double max_frames_in_flight =
      max_frame_rate_ * duration_in_flight.InSecondsF();
  if (count_frames_in_flight >= max_frames_in_flight + kMaxFrameBurst) {
    VLOG(1) << SENDER_SSRC << "Dropping: Burst threshold would be exceeded.";
    return true;
  }

  // Check that accepting the next frame won't exceed the allowed in-flight
  // media duration.
  const base::TimeDelta duration_would_be_in_flight =
      duration_in_flight + frame_duration;
  const base::TimeDelta allowed_in_flight = GetAllowedInFlightMediaDuration();
  if (VLOG_IS_ON(1)) {
    const int64_t percent =
        allowed_in_flight > base::TimeDelta()
            ? 100 * duration_would_be_in_flight / allowed_in_flight
            : std::numeric_limits<int64_t>::max();
    VLOG_IF(1, percent > 50)
        << SENDER_SSRC
        << duration_in_flight.InMicroseconds() << " usec in-flight + "
        << frame_duration.InMicroseconds() << " usec for next frame --> "
        << percent << "% of allowed in-flight.";
  }
  if (duration_would_be_in_flight > allowed_in_flight) {
    VLOG(1) << SENDER_SSRC << "Dropping: In-flight duration would be too high.";
    return true;
  }

  // Next frame is accepted.
  return false;
}

}  // namespace cast
}  // namespace media

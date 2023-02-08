// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/frame_sender_impl.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/constants.h"
#include "third_party/openscreen/src/cast/streaming/encoded_frame.h"

namespace media::cast {
namespace {

constexpr int kNumAggressiveReportsSentAtStart = 100;
constexpr base::TimeDelta kMinSchedulingDelay = base::Milliseconds(1);
constexpr base::TimeDelta kReceiverProcessTime = base::Milliseconds(250);

// The additional number of frames that can be in-flight when input exceeds the
// maximum frame rate.
constexpr int kMaxFrameBurst = 5;

}  // namespace

// static
std::unique_ptr<FrameSender> FrameSender::Create(
    scoped_refptr<CastEnvironment> cast_environment,
    const FrameSenderConfig& config,
    CastTransport* const transport_sender,
    Client& client) {
  return std::make_unique<FrameSenderImpl>(cast_environment, config,
                                           transport_sender, client);
}

// Convenience macro used in logging statements throughout this file.
#define SENDER_SSRC \
  (is_audio_ ? "AUDIO[" : "VIDEO[") << config_.sender_ssrc << "] "

FrameSenderImpl::Client::~Client() = default;

FrameSenderImpl::RtcpClient::RtcpClient(
    base::WeakPtr<FrameSenderImpl> frame_sender)
    : frame_sender_(frame_sender) {}

FrameSenderImpl::RtcpClient::~RtcpClient() = default;

void FrameSenderImpl::RtcpClient::OnReceivedCastMessage(
    const RtcpCastMessage& cast_message) {
  if (frame_sender_)
    frame_sender_->OnReceivedCastFeedback(cast_message);
}

void FrameSenderImpl::RtcpClient::OnReceivedRtt(
    base::TimeDelta round_trip_time) {
  if (frame_sender_)
    frame_sender_->OnMeasuredRoundTripTime(round_trip_time);
}

void FrameSenderImpl::RtcpClient::OnReceivedPli() {
  if (frame_sender_)
    frame_sender_->OnReceivedPli();
}

FrameSenderImpl::FrameSenderImpl(
    scoped_refptr<CastEnvironment> cast_environment,
    const FrameSenderConfig& config,
    CastTransport* const transport_sender,
    Client& client)
    : cast_environment_(cast_environment),
      config_(config),
      target_playout_delay_(config.max_playout_delay),
      max_frame_rate_(config.max_frame_rate),
      transport_sender_(transport_sender),
      client_(client),
      is_audio_(config.rtp_payload_type <= RtpPayloadType::AUDIO_LAST),
      // We only use the adaptive control for software video encoding.
      congestion_control_(
          (!config.use_hardware_encoder && !is_audio_)
              ? NewAdaptiveCongestionControl(cast_environment->Clock(),
                                             config.max_bitrate,
                                             config.min_bitrate,
                                             max_frame_rate_)
              : NewFixedCongestionControl(
                    (config.min_bitrate + config.max_bitrate) / 2)

              ),
      max_ack_delay_(config_.max_playout_delay) {
  DCHECK(transport_sender_);
  DCHECK_GT(config_.rtp_timebase, 0);
  DCHECK(congestion_control_);

  // We start at the minimum playout delay and extend if necessary later.
  VLOG(1) << SENDER_SSRC << "min latency "
          << config_.min_playout_delay.InMilliseconds() << ", max latency "
          << config_.max_playout_delay.InMilliseconds();
  SetTargetPlayoutDelay(config_.min_playout_delay);

  CastTransportRtpConfig transport_config;
  transport_config.ssrc = config.sender_ssrc;
  transport_config.feedback_ssrc = config.receiver_ssrc;
  transport_config.rtp_payload_type = config.rtp_payload_type;
  transport_config.aes_key = config.aes_key;
  transport_config.aes_iv_mask = config.aes_iv_mask;
  transport_sender_->InitializeStream(
      transport_config, std::make_unique<FrameSenderImpl::RtcpClient>(
                            weak_factory_.GetWeakPtr()));
}

FrameSenderImpl::~FrameSenderImpl() = default;

bool FrameSenderImpl::NeedsKeyFrame() const {
  return picture_lost_at_receiver_;
}

base::TimeTicks FrameSenderImpl::GetRecordedReferenceTime(
    FrameId frame_id) const {
  return frame_reference_times_[frame_id.lower_8_bits()];
}

void FrameSenderImpl::ScheduleNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::BindOnce(&FrameSenderImpl::SendRtcpReport,
                     weak_factory_.GetWeakPtr(), true),
      kRtcpReportInterval);
}

void FrameSenderImpl::SendRtcpReport(bool schedule_future_reports) {
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
      ToRtpTimeDelta(time_delta, config_.rtp_timebase);
  const RtpTimeTicks now_as_rtp_timestamp =
      GetRecordedRtpTimestamp(last_sent_frame_id_) + rtp_delta;
  transport_sender_->SendSenderReport(config_.sender_ssrc, now,
                                      now_as_rtp_timestamp);

  if (schedule_future_reports)
    ScheduleNextRtcpReport();
}

void FrameSenderImpl::OnMeasuredRoundTripTime(base::TimeDelta round_trip_time) {
  DCHECK_GT(round_trip_time, base::TimeDelta());
  current_round_trip_time_ = round_trip_time;
  max_ack_delay_ = 2 * std::max(current_round_trip_time_, base::TimeDelta()) +
                   kReceiverProcessTime;
  max_ack_delay_ = std::min(max_ack_delay_, target_playout_delay_);
}

void FrameSenderImpl::SetTargetPlayoutDelay(
    base::TimeDelta new_target_playout_delay) {
  if (send_target_playout_delay_ &&
      target_playout_delay_ == new_target_playout_delay) {
    return;
  }
  new_target_playout_delay =
      std::max(new_target_playout_delay, config_.min_playout_delay);
  new_target_playout_delay =
      std::min(new_target_playout_delay, config_.max_playout_delay);
  VLOG(2) << SENDER_SSRC << "Target playout delay changing from "
          << target_playout_delay_.InMilliseconds() << " ms to "
          << new_target_playout_delay.InMilliseconds() << " ms.";
  target_playout_delay_ = new_target_playout_delay;
  max_ack_delay_ = std::min(max_ack_delay_, target_playout_delay_);
  send_target_playout_delay_ = true;
  congestion_control_->UpdateTargetPlayoutDelay(target_playout_delay_);
}

base::TimeDelta FrameSenderImpl::GetTargetPlayoutDelay() const {
  return target_playout_delay_;
}

void FrameSenderImpl::ResendCheck() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!last_send_time_.is_null());
  const base::TimeDelta time_since_last_send =
      cast_environment_->Clock()->NowTicks() - last_send_time_;
  if (time_since_last_send > max_ack_delay_) {
    if (latest_acked_frame_id_ == last_sent_frame_id_) {
      // Last frame acked, no point in doing anything
    } else {
      VLOG(1) << SENDER_SSRC
              << "ACK timeout; last acked frame: " << latest_acked_frame_id_;
      ResendForKickstart();
    }
  }
  ScheduleNextResendCheck();
}

void FrameSenderImpl::ScheduleNextResendCheck() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!last_send_time_.is_null());
  base::TimeDelta time_to_next =
      last_send_time_ - cast_environment_->Clock()->NowTicks() + max_ack_delay_;
  time_to_next = std::max(time_to_next, kMinSchedulingDelay);
  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::BindOnce(&FrameSenderImpl::ResendCheck, weak_factory_.GetWeakPtr()),
      time_to_next);
}

void FrameSenderImpl::ResendForKickstart() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(!last_send_time_.is_null());
  VLOG(1) << SENDER_SSRC << "Resending last packet of frame "
          << last_sent_frame_id_ << " to kick-start.";
  last_send_time_ = cast_environment_->Clock()->NowTicks();
  transport_sender_->ResendFrameForKickstart(config_.sender_ssrc,
                                             last_sent_frame_id_);
}

void FrameSenderImpl::RecordLatestFrameTimestamps(
    FrameId frame_id,
    base::TimeTicks reference_time,
    RtpTimeTicks rtp_timestamp) {
  DCHECK(!reference_time.is_null());
  frame_reference_times_[frame_id.lower_8_bits()] = reference_time;
  frame_rtp_timestamps_[frame_id.lower_8_bits()] = rtp_timestamp;
}

base::TimeDelta FrameSenderImpl::GetInFlightMediaDuration() const {
  const base::TimeDelta encoder_duration = client_->GetEncoderBacklogDuration();
  // No frames are in flight, so only look at the encoder duration.
  if (last_sent_frame_id_ == latest_acked_frame_id_) {
    return encoder_duration;
  }

  const RtpTimeTicks oldest_acked_timestamp =
      GetRecordedRtpTimestamp(latest_acked_frame_id_);
  const RtpTimeTicks newest_acked_timestamp =
      GetRecordedRtpTimestamp(last_sent_frame_id_);
  return ToTimeDelta(newest_acked_timestamp - oldest_acked_timestamp,
                     config_.rtp_timebase) +
         encoder_duration;
}

RtpTimeTicks FrameSenderImpl::GetRecordedRtpTimestamp(FrameId frame_id) const {
  return frame_rtp_timestamps_[frame_id.lower_8_bits()];
}

int FrameSenderImpl::GetUnacknowledgedFrameCount() const {
  if (last_send_time_.is_null())
    return 0;
  const int count = last_sent_frame_id_ - latest_acked_frame_id_;
  DCHECK_GE(count, 0);
  return count;
}

int FrameSenderImpl::GetSuggestedBitrate(base::TimeTicks playout_time,
                                         base::TimeDelta playout_delay) {
  return congestion_control_->GetBitrate(playout_time, playout_delay);
}

double FrameSenderImpl::MaxFrameRate() const {
  return max_frame_rate_;
}

void FrameSenderImpl::SetMaxFrameRate(double max_frame_rate) {
  max_frame_rate_ = max_frame_rate;
}

base::TimeDelta FrameSenderImpl::TargetPlayoutDelay() const {
  return target_playout_delay_;
}
base::TimeDelta FrameSenderImpl::CurrentRoundTripTime() const {
  return current_round_trip_time_;
}
base::TimeTicks FrameSenderImpl::LastSendTime() const {
  return last_send_time_;
}
FrameId FrameSenderImpl::LastAckedFrameId() const {
  return latest_acked_frame_id_;
}

base::TimeDelta FrameSenderImpl::GetAllowedInFlightMediaDuration() const {
  // The total amount allowed in-flight media should equal the amount that fits
  // within the entire playout delay window, plus the amount of time it takes to
  // receive an ACK from the receiver.
  return target_playout_delay_ + (current_round_trip_time_ / 2);
}

CastStreamingFrameDropReason FrameSenderImpl::EnqueueFrame(
    std::unique_ptr<SenderEncodedFrame> encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  VLOG(2) << SENDER_SSRC
          << "About to send another frame: last_sent=" << last_sent_frame_id_
          << ", latest_acked=" << latest_acked_frame_id_;

  const FrameId frame_id = encoded_frame->frame_id;
  const bool is_first_frame_to_be_sent = last_send_time_.is_null();

  if (picture_lost_at_receiver_ &&
      (encoded_frame->dependency ==
       openscreen::cast::EncodedFrame::Dependency::kKeyFrame)) {
    picture_lost_at_receiver_ = false;
    DCHECK(frame_id > latest_acked_frame_id_);
    // Cancel sending remaining frames.
    std::vector<FrameId> cancel_sending_frames;
    for (FrameId id = latest_acked_frame_id_ + 1; id < frame_id; ++id) {
      cancel_sending_frames.push_back(id);
      client_->OnFrameCanceled(id);
    }
    transport_sender_->CancelSendingFrames(config_.sender_ssrc,
                                           cancel_sending_frames);
  }

  last_send_time_ = cast_environment_->Clock()->NowTicks();

  DCHECK(frame_id > last_sent_frame_id_) << "enqueued frames out of order.";
  last_sent_frame_id_ = frame_id;
  // If this is the first frame about to be sent, fake the value of
  // |latest_acked_frame_id_| to indicate the receiver starts out all
  // caught up. Also, schedule the periodic frame re-send checks.
  if (is_first_frame_to_be_sent) {
    latest_acked_frame_id_ = frame_id - 1;
    ScheduleNextResendCheck();
  }

  VLOG_IF(1, !is_audio_ &&
                 encoded_frame->dependency ==
                     openscreen::cast::EncodedFrame::Dependency::kKeyFrame)
      << SENDER_SSRC << "Sending encoded key frame, id=" << frame_id;

  std::unique_ptr<FrameEvent> encode_event(new FrameEvent());
  encode_event->timestamp = encoded_frame->encode_completion_time;
  encode_event->type = FRAME_ENCODED;
  encode_event->media_type = is_audio_ ? AUDIO_EVENT : VIDEO_EVENT;
  encode_event->rtp_timestamp = encoded_frame->rtp_timestamp;
  encode_event->frame_id = frame_id;
  encode_event->size = base::checked_cast<uint32_t>(encoded_frame->data.size());
  encode_event->key_frame =
      encoded_frame->dependency ==
      openscreen::cast::EncodedFrame::Dependency::kKeyFrame;
  encode_event->target_bitrate = encoded_frame->encoder_bitrate;
  encode_event->encoder_cpu_utilization = encoded_frame->encoder_utilization;
  encode_event->idealized_bitrate_utilization = encoded_frame->lossiness;
  cast_environment_->logger()->DispatchFrameEvent(std::move(encode_event));

  RecordLatestFrameTimestamps(frame_id, encoded_frame->reference_time,
                              encoded_frame->rtp_timestamp);

  if (!is_audio_) {
    // Used by chrome/browser/media/cast_mirroring_performance_browsertest.cc
    TRACE_EVENT_INSTANT1("cast_perf_test", "VideoFrameEncoded",
                         TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp",
                         encoded_frame->rtp_timestamp.lower_32_bits());
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

  congestion_control_->WillSendFrameToTransport(
      frame_id, encoded_frame->data.size(), last_send_time_);

  if (send_target_playout_delay_) {
    encoded_frame->new_playout_delay_ms =
        target_playout_delay_.InMilliseconds();
  }

  const char* name = is_audio_ ? "Audio Transport" : "Video Transport";
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "cast.stream", name, TRACE_ID_WITH_SCOPE(name, frame_id.lower_32_bits()),
      "rtp_timestamp", encoded_frame->rtp_timestamp.lower_32_bits());
  transport_sender_->InsertFrame(config_.sender_ssrc, *encoded_frame);
  return CastStreamingFrameDropReason::kNotDropped;
}

void FrameSenderImpl::OnReceivedCastFeedback(
    const RtcpCastMessage& cast_feedback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  const bool have_valid_rtt = current_round_trip_time_.is_positive();
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
    if (duplicate_ack_counter_ >= 2 && duplicate_ack_counter_ % 3 == 2) {
      ResendForKickstart();
    }
  } else {
    // Only count duplicated ACKs if there is no NACK request in between.
    // This is to avoid aggressive resend.
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
  VLOG(2) << SENDER_SSRC << "Received ACK"
          << (is_acked_out_of_order ? " out-of-order" : "") << " for frame "
          << cast_feedback.ack_frame_id;
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
      client_->OnFrameCanceled(latest_acked_frame_id_);
      // This is a good place to match the trace for frame ids
      // since this ensures we not only track frame ids that are
      // implicitly ACKed, but also handles duplicate ACKs
      const char* name = is_audio_ ? "Audio Transport" : "Video Transport";
      TRACE_EVENT_NESTABLE_ASYNC_END1(
          "cast.stream", name,
          TRACE_ID_WITH_SCOPE(name, latest_acked_frame_id_.lower_32_bits()),
          "RTT_usecs", current_round_trip_time_.InMicroseconds());
    } while (latest_acked_frame_id_ < cast_feedback.ack_frame_id);
    transport_sender_->CancelSendingFrames(config_.sender_ssrc,
                                           frames_to_cancel);
  }
}

void FrameSenderImpl::OnReceivedPli() {
  picture_lost_at_receiver_ = true;
}

CastStreamingFrameDropReason FrameSenderImpl::ShouldDropNextFrame(
    base::TimeDelta frame_duration) const {
  // Check that accepting the next frame won't cause more frames to become
  // in-flight than the system's design limit.
  const int count_frames_in_flight =
      GetUnacknowledgedFrameCount() + client_->GetNumberOfFramesInEncoder();
  if (count_frames_in_flight >= kMaxUnackedFrames) {
    return CastStreamingFrameDropReason::kTooManyFramesInFlight;
  }

  // Check that accepting the next frame won't exceed the configured maximum
  // frame rate, allowing for short-term bursts.
  const base::TimeDelta duration_in_flight = GetInFlightMediaDuration();
  const double max_frames_in_flight =
      max_frame_rate_ * duration_in_flight.InSecondsF();
  if (count_frames_in_flight >= max_frames_in_flight + kMaxFrameBurst) {
    return CastStreamingFrameDropReason::kBurstThresholdExceeded;
  }

  // Check that accepting the next frame won't exceed the allowed in-flight
  // media duration.
  const base::TimeDelta duration_would_be_in_flight =
      duration_in_flight + frame_duration;
  const base::TimeDelta allowed_in_flight = GetAllowedInFlightMediaDuration();
  if (VLOG_IS_ON(1)) {
    const int64_t percent =
        allowed_in_flight.is_positive()
            ? base::ClampRound<int64_t>(duration_would_be_in_flight /
                                        allowed_in_flight * 100)
            : std::numeric_limits<int64_t>::max();
    VLOG_IF(1, percent > 50)
        << SENDER_SSRC << duration_in_flight.InMicroseconds()
        << " usec in-flight + " << frame_duration.InMicroseconds()
        << " usec for next frame --> " << percent << "% of allowed in-flight.";
  }
  if (duration_would_be_in_flight > allowed_in_flight) {
    return CastStreamingFrameDropReason::kInFlightDurationTooHigh;
  }

  // Next frame is accepted.
  return CastStreamingFrameDropReason::kNotDropped;
}

}  // namespace media::cast

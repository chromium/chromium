// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/cast/sender/openscreen_frame_sender.h"

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
#include "media/base/media_switches.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/constants.h"
#include "third_party/openscreen/src/cast/streaming/public/encoded_frame.h"

namespace media::cast {
namespace {

// The additional number of frames that can be in-flight when input exceeds the
// maximum frame rate.
static constexpr int kMaxFrameBurst = 5;

using EnqueueFrameResult = openscreen::cast::Sender::EnqueueFrameResult;
CastStreamingFrameDropReason ToFrameDropReason(EnqueueFrameResult result) {
  switch (result) {
    case EnqueueFrameResult::OK:
      return CastStreamingFrameDropReason::kNotDropped;

    case EnqueueFrameResult::PAYLOAD_TOO_LARGE:
      return CastStreamingFrameDropReason::kPayloadTooLarge;

    case EnqueueFrameResult::REACHED_ID_SPAN_LIMIT:
      return CastStreamingFrameDropReason::kReachedIdSpanLimit;

    case EnqueueFrameResult::MAX_DURATION_IN_FLIGHT:
      return CastStreamingFrameDropReason::
          kInFlightDurationTooHighAfterEncoding;
  }
}

}  // namespace

std::unique_ptr<FrameSender> FrameSender::Create(
    scoped_refptr<CastEnvironment> cast_environment,
    const FrameSenderConfig& config,
    std::unique_ptr<openscreen::cast::Sender> sender,
    Client& client,
    FrameSender::GetSuggestedVideoBitrateCB get_bitrate_cb) {
  return std::make_unique<OpenscreenFrameSender>(cast_environment, config,
                                                 std::move(sender), client,
                                                 std::move(get_bitrate_cb));
}

// Convenience macro used in logging statements throughout this file.
#define VLOG_WITH_SSRC(N)                      \
  VLOG(N) << (is_audio_ ? "AUDIO[" : "VIDEO[") \
          << sender_->config().sender_ssrc << "] "

OpenscreenFrameSender::OpenscreenFrameSender(
    scoped_refptr<CastEnvironment> cast_environment,
    const FrameSenderConfig& config,
    std::unique_ptr<openscreen::cast::Sender> sender,
    Client& client,
    FrameSender::GetSuggestedVideoBitrateCB get_bitrate_cb)
    : cast_environment_(cast_environment),
      sender_(std::move(sender)),
      client_(client),
      max_frame_rate_(config.max_frame_rate),
      is_audio_(config.rtp_payload_type <= RtpPayloadType::AUDIO_LAST),
      min_playout_delay_(config.min_playout_delay),
      max_playout_delay_(config.max_playout_delay) {
  CHECK_GT(sender_->config().rtp_timebase, 0);
  if (!is_audio_) {
    bitrate_suggester_ = std::make_unique<VideoBitrateSuggester>(
        config, std::move(get_bitrate_cb));
  }

  const std::chrono::milliseconds target_playout_delay =
      sender_->config().target_playout_delay;
  VLOG_WITH_SSRC(1) << "target latency " << target_playout_delay.count()
                    << "ms";
  SetTargetPlayoutDelay(base::Milliseconds(target_playout_delay.count()));

  sender_->SetObserver(this);
}

OpenscreenFrameSender::~OpenscreenFrameSender() {
  sender_->SetObserver(nullptr);
}

bool OpenscreenFrameSender::NeedsKeyFrame() const {
  return sender_->NeedsKeyFrame();
}

void OpenscreenFrameSender::SetTargetPlayoutDelay(
    base::TimeDelta new_target_playout_delay) {
  if (send_target_playout_delay_ &&
      target_playout_delay_ == new_target_playout_delay) {
    return;
  }

  new_target_playout_delay = std::clamp(new_target_playout_delay,
                                        min_playout_delay_, max_playout_delay_);
  VLOG_WITH_SSRC(2) << "Target playout delay changing from "
                    << target_playout_delay_.InMilliseconds() << " ms to "
                    << new_target_playout_delay.InMilliseconds() << " ms.";
  target_playout_delay_ = new_target_playout_delay;
  send_target_playout_delay_ = true;
}

base::TimeDelta OpenscreenFrameSender::GetTargetPlayoutDelay() const {
  return target_playout_delay_;
}

void OpenscreenFrameSender::OnFrameCanceled(
    openscreen::cast::FrameId frame_id) {
  if (frame_id > last_acked_frame_id_) {
    last_acked_frame_id_ = frame_id;
  }
  client_->OnFrameCanceled(frame_id);
}

void OpenscreenFrameSender::OnPictureLost() {}

void OpenscreenFrameSender::RecordLatestFrameTimestamps(
    FrameId frame_id,
    base::TimeTicks reference_time,
    RtpTimeTicks rtp_timestamp) {
  DCHECK(!reference_time.is_null());
  frame_rtp_timestamps_[frame_id.lower_8_bits()] = rtp_timestamp;
}

base::TimeDelta OpenscreenFrameSender::GetInFlightMediaDuration() const {
  // Start by including the encoder backlog duration, defined as the
  // difference between the timestamps of the last frame to enter the encoder
  // and the last frame to exit the encoder.
  base::TimeDelta duration = client_->GetEncoderBacklogDuration();

  // If we have sent at least one frame, then include the duration currently
  // in flight (as recorded by the Open Screen sender).
  if (!last_enqueued_frame_id_.is_null()) {
    const RtpTimeTicks newest_timestamp =
        GetRecordedRtpTimestamp(last_enqueued_frame_id_);
    duration +=
        ToTimeDelta(sender_->GetInFlightMediaDuration(newest_timestamp));
  }
  return duration;
}

RtpTimeTicks OpenscreenFrameSender::GetRecordedRtpTimestamp(
    FrameId frame_id) const {
  if (static_cast<size_t>(std::abs(last_enqueued_frame_id_ - frame_id)) >=
      std::size(frame_rtp_timestamps_)) {
    return {};
  }
  return frame_rtp_timestamps_[frame_id.lower_8_bits()];
}

int OpenscreenFrameSender::GetUnacknowledgedFrameCount() const {
  return sender_->GetInFlightFrameCount();
}

int OpenscreenFrameSender::GetSuggestedBitrate(base::TimeTicks playout_time,
                                               base::TimeDelta playout_delay) {
  // Currently only used by the video sender.
  DCHECK(!is_audio_);
  return bitrate_suggester_->GetSuggestedBitrate();
}

double OpenscreenFrameSender::MaxFrameRate() const {
  return max_frame_rate_;
}

void OpenscreenFrameSender::SetMaxFrameRate(double max_frame_rate) {
  max_frame_rate_ = max_frame_rate;
}

base::TimeDelta OpenscreenFrameSender::TargetPlayoutDelay() const {
  return target_playout_delay_;
}

base::TimeDelta OpenscreenFrameSender::CurrentRoundTripTime() const {
  return base::Microseconds(
      std::chrono::duration_cast<std::chrono::microseconds>(
          sender_->GetCurrentRoundTripTime())
          .count());
}

base::TimeTicks OpenscreenFrameSender::LastSendTime() const {
  return last_send_time_;
}

FrameId OpenscreenFrameSender::LastAckedFrameId() const {
  return last_acked_frame_id_;
}

base::TimeDelta OpenscreenFrameSender::GetAllowedInFlightMediaDuration() const {
  return ToTimeDelta(sender_->GetMaxInFlightMediaDuration());
}

CastStreamingFrameDropReason OpenscreenFrameSender::EnqueueFrame(
    std::unique_ptr<SenderEncodedFrame> encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  CHECK(encoded_frame);
  VLOG_WITH_SSRC(2) << "About to send another frame ("
                    << encoded_frame->frame_id
                    << "). last enqueued=" << last_enqueued_frame_id_;

  CHECK_GE(encoded_frame->frame_id, last_enqueued_frame_id_)
      << "enqueued frames out of order.";
  last_enqueued_frame_id_ = encoded_frame->frame_id;
  last_send_time_ = cast_environment_->Clock()->NowTicks();

  if (!is_audio_ && encoded_frame->dependency ==
                        openscreen::cast::EncodedFrame::Dependency::kKeyFrame) {
    VLOG_WITH_SSRC(1) << "Sending encoded key frame, id="
                      << encoded_frame->frame_id;
    frame_id_map_.clear();
  }

  RecordLatestFrameTimestamps(encoded_frame->frame_id,
                              encoded_frame->reference_time,
                              encoded_frame->rtp_timestamp);

  if (!is_audio_) {
    // Used by chrome/browser/media/cast_mirroring_performance_browsertest.cc
    TRACE_EVENT_INSTANT1("cast_perf_test", "VideoFrameEncoded",
                         TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp",
                         encoded_frame->rtp_timestamp.lower_32_bits());
  }

  if (send_target_playout_delay_) {
    encoded_frame->new_playout_delay_ms =
        target_playout_delay_.InMilliseconds();
    send_target_playout_delay_ = false;
  }

  static const char* name = is_audio_ ? "Audio Transport" : "Video Transport";
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
      "cast.stream", name,
      TRACE_ID_WITH_SCOPE(name, encoded_frame->frame_id.lower_32_bits()),
      "rtp_timestamp", encoded_frame->rtp_timestamp.lower_32_bits());

  // The `FrameId` given to us by child classes such as VideoSender should
  // be at least the result of openscreen::cast::Sender::GetNextFrameId(). If
  // the Open Screen Sender choose to not send a frame, it does not advance the
  // frame identifier.
  const FrameId openscreen_frame_id = sender_->GetNextFrameId();
  CHECK_GE(encoded_frame->frame_id, openscreen_frame_id);

  // We only need to use the frame ID map if the identifiers have diverged and
  // are no longer in sync. The remoting sender should always be in sync, so
  // this optimization improves memory usage significantly for remoting cases as
  // well as high quality mirroring sessions that do not have any dropped
  // frames.
  if (!diverged_frame_id_ && encoded_frame->frame_id != openscreen_frame_id) {
    diverged_frame_id_ = encoded_frame->frame_id;
  }

  // Finally, convert to an Open Screen encoded frame using the equivalent frame
  // identifiers generated by the Open Screen sender.
  auto openscreen_frame = ToOpenscreenEncodedFrame(*encoded_frame);
  if (diverged_frame_id_) {
    frame_id_map_.insert_or_assign(encoded_frame->frame_id,
                                   openscreen_frame_id);
    openscreen_frame.frame_id = openscreen_frame_id;

    // We should have the referenced ID in the map if it was added after we
    // started tracking.
    if (encoded_frame->referenced_frame_id >= *diverged_frame_id_) {
      auto it = frame_id_map_.find(encoded_frame->referenced_frame_id);
      if (it == frame_id_map_.end()) {
        return CastStreamingFrameDropReason::kInvalidReferencedFrameId;
      }
      openscreen_frame.referenced_frame_id = it->second;
    }
  }

  const auto result = sender_->EnqueueFrame(std::move(openscreen_frame));
  return ToFrameDropReason(result);
}

CastStreamingFrameDropReason OpenscreenFrameSender::ShouldDropNextFrame(
    base::TimeDelta frame_duration) {
  // Check that accepting the next frame won't cause more frames to become
  // in-flight than the system's design limit.
  const int count_frames_in_flight =
      GetUnacknowledgedFrameCount() + client_->GetNumberOfFramesInEncoder();
  if (count_frames_in_flight >= kMaxUnackedFrames) {
    RecordShouldDropNextFrame(/*should_drop=*/true);
    return CastStreamingFrameDropReason::kTooManyFramesInFlight;
  }

  // Check that accepting the next frame won't exceed the configured maximum
  // frame rate, allowing for short-term bursts.
  const base::TimeDelta duration_in_flight = GetInFlightMediaDuration();
  const double max_frames_in_flight =
      max_frame_rate_ * duration_in_flight.InSecondsF();
  if (count_frames_in_flight >= max_frames_in_flight + kMaxFrameBurst) {
    RecordShouldDropNextFrame(/*should_drop=*/true);
    return CastStreamingFrameDropReason::kBurstThresholdExceeded;
  }

  // At this point, we know we don't have too many frames, but we still need
  // to ensure we don't have too much duration in flight. This case should
  // not be hit very often, unless some frames have a higher duration than
  // expected.
  const base::TimeDelta duration_would_be_in_flight =
      duration_in_flight + frame_duration;
  const base::TimeDelta allowed_in_flight = GetAllowedInFlightMediaDuration();
  if (VLOG_IS_ON(1)) {
    const int64_t percent =
        allowed_in_flight.is_positive()
            ? base::ClampRound<int64_t>(duration_would_be_in_flight /
                                        allowed_in_flight * 100)
            : std::numeric_limits<int64_t>::max();

    if (percent > 50) {
      VLOG_WITH_SSRC(1) << duration_in_flight.InMicroseconds()
                        << " usec in-flight + "
                        << frame_duration.InMicroseconds()
                        << " usec for next frame --> " << percent
                        << "% of allowed in-flight.";
    }
  }
  if (duration_would_be_in_flight > allowed_in_flight) {
    RecordShouldDropNextFrame(/*should_drop=*/true);
    return CastStreamingFrameDropReason::kInFlightDurationTooHigh;
  }

  // Next frame is accepted.
  RecordShouldDropNextFrame(/*should_drop=*/false);
  return CastStreamingFrameDropReason::kNotDropped;
}

void OpenscreenFrameSender::RecordShouldDropNextFrame(bool should_drop) {
  if (bitrate_suggester_) {
    bitrate_suggester_->RecordShouldDropNextFrame(should_drop);
  }
}
}  // namespace media::cast

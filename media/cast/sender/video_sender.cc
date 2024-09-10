// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/video_sender.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "media/base/media_switches.h"
#include "media/base/video_encoder_metrics_provider.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/common/rtp_time.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/encoding/video_encoder.h"
#include "media/cast/sender/openscreen_frame_sender.h"
#include "media/cast/sender/performance_metrics_overlay.h"
#include "third_party/openscreen/src/cast/streaming/public/encoded_frame.h"
#include "third_party/openscreen/src/cast/streaming/public/sender.h"

namespace media::cast {

namespace {

// The following two constants are used to adjust the target
// playout delay (when allowed). They were calculated using
// a combination of cast_benchmark runs and manual testing.
//
// This is how many round trips we think we need on the network.
constexpr int kRoundTripsNeeded = 4;

// This is an estimate of all the the constant time needed independent of
// network quality (e.g., additional time that accounts for encode and decode
// time).
constexpr int kConstantTimeMs = 75;

// The target maximum utilization of the encoder and network resources.  This is
// used to attenuate the actual measured utilization values in order to provide
// "breathing room" (i.e., to ensure there will be sufficient CPU and bandwidth
// available to handle the occasional more-complex frames).
constexpr int kTargetUtilizationPercentage = 75;

// This is the minimum duration that the sender sends key frame to the encoder
// on receiving Pli messages. This is used to prevent sending multiple requests
// while the sender is waiting for an encoded key frame or receiving multiple
// Pli messages in a short period.
constexpr base::TimeDelta kMinKeyFrameRequestInterval = base::Milliseconds(500);

// This is the minimum amount of frames between issuing key frame requests.
constexpr int kMinKeyFrameRequestFrameInterval = 6;

// UMA histogram name for video bitrate setting.
constexpr char kHistogramBitrate[] = "CastStreaming.Sender.Video.Bitrate";

// UMA histogram for the percentage of dropped video frames.
constexpr char kHistogramDroppedFrames[] =
    "CastStreaming.Sender.Video.PercentDroppedFrames";

// UMA histogram for recording when a frame is dropped.
constexpr char kHistogramFrameDropped[] =
    "CastStreaming.Sender.Video.FrameDropped";

// Extract capture begin/end timestamps from |video_frame|'s metadata and log
// it.
void LogVideoCaptureTimestamps(CastEnvironment* cast_environment,
                               const media::VideoFrame& video_frame,
                               RtpTimeTicks rtp_timestamp) {
  std::unique_ptr<FrameEvent> capture_begin_event(new FrameEvent());
  capture_begin_event->type = FRAME_CAPTURE_BEGIN;
  capture_begin_event->media_type = VIDEO_EVENT;
  capture_begin_event->rtp_timestamp = rtp_timestamp;

  std::unique_ptr<FrameEvent> capture_end_event(new FrameEvent());
  capture_end_event->type = FRAME_CAPTURE_END;
  capture_end_event->media_type = VIDEO_EVENT;
  capture_end_event->rtp_timestamp = rtp_timestamp;
  capture_end_event->width = video_frame.visible_rect().width();
  capture_end_event->height = video_frame.visible_rect().height();

  if (video_frame.metadata().capture_begin_time.has_value() &&
      video_frame.metadata().capture_end_time.has_value()) {
    capture_begin_event->timestamp = *video_frame.metadata().capture_begin_time;
    capture_end_event->timestamp = *video_frame.metadata().capture_end_time;
  } else {
    // The frame capture timestamps were not provided by the video capture
    // source.  Simply log the events as happening right now.
    capture_begin_event->timestamp = capture_end_event->timestamp =
        cast_environment->Clock()->NowTicks();
  }

  cast_environment->logger()->DispatchFrameEvent(
      std::move(capture_begin_event));
  cast_environment->logger()->DispatchFrameEvent(std::move(capture_end_event));
}

}  // namespace

VideoSender::VideoSender(
    scoped_refptr<CastEnvironment> cast_environment,
    const FrameSenderConfig& video_config,
    StatusChangeCallback status_change_cb,
    const CreateVideoEncodeAcceleratorCallback& create_vea_cb,
    std::unique_ptr<openscreen::cast::Sender> sender,
    std::unique_ptr<media::VideoEncoderMetricsProvider>
        encoder_metrics_provider,
    PlayoutDelayChangeCB playout_delay_change_cb,
    media::VideoCaptureFeedbackCB feedback_cb,
    FrameSender::GetSuggestedVideoBitrateCB get_bitrate_cb)
    : frame_sender_(FrameSender::Create(cast_environment,
                                        video_config,
                                        std::move(sender),
                                        *this,
                                        std::move(get_bitrate_cb))),
      cast_environment_(cast_environment),
      min_playout_delay_(video_config.min_playout_delay),
      max_playout_delay_(video_config.max_playout_delay),
      playout_delay_change_cb_(std::move(playout_delay_change_cb)),
      feedback_cb_(feedback_cb) {
  video_encoder_ = VideoEncoder::Create(cast_environment_, video_config,
                                        std::move(encoder_metrics_provider),
                                        status_change_cb, create_vea_cb);
  if (!video_encoder_) {
    cast_environment_->PostTask(
        CastEnvironment::MAIN, FROM_HERE,
        base::BindOnce(std::move(status_change_cb), STATUS_UNSUPPORTED_CODEC));
  }
}

VideoSender::~VideoSender() {
  // Record the number of frames dropped during this session.
  base::UmaHistogramPercentage(kHistogramDroppedFrames,
                               (number_of_frames_dropped_ * 100) /
                                   std::max(1, number_of_frames_inserted_));
}

void VideoSender::InsertRawVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    base::TimeTicks reference_time) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  CHECK(video_encoder_);

  const RtpTimeTicks rtp_timestamp =
      ToRtpTimeTicks(video_frame->timestamp(), kVideoFrequency);
  LogVideoCaptureTimestamps(cast_environment_.get(), *video_frame,
                            rtp_timestamp);

  // Used by chrome/browser/media/cast_mirroring_performance_browsertest.cc
  TRACE_EVENT_INSTANT2("cast_perf_test", "InsertRawVideoFrame",
                       TRACE_EVENT_SCOPE_THREAD, "timestamp",
                       (reference_time - base::TimeTicks()).InMicroseconds(),
                       "rtp_timestamp", rtp_timestamp.lower_32_bits());

  {
    bool new_low_latency_mode = video_frame->metadata().interactive_content;
    if (new_low_latency_mode && !low_latency_mode_) {
      VLOG(1) << "Interactive mode playout time " << min_playout_delay_;
      playout_delay_change_cb_.Run(min_playout_delay_);
    }
    low_latency_mode_ = new_low_latency_mode;
  }

  // Drop the frame if either its RTP or reference timestamp is not an increase
  // over the last frame's.  This protects: 1) the duration calculations that
  // assume timestamps are monotonically non-decreasing, and 2) assumptions made
  // deeper in the implementation where each frame's RTP timestamp needs to be
  // unique.
  if (!last_enqueued_frame_reference_time_.is_null() &&
      (rtp_timestamp <= last_enqueued_frame_rtp_timestamp_ ||
       reference_time <= last_enqueued_frame_reference_time_)) {
    VLOG(1) << "Dropping video frame: RTP or reference time did not increase.";
    TRACE_EVENT_INSTANT2("cast.stream", "Video Frame Drop",
                         TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp",
                         rtp_timestamp.lower_32_bits(), "reason",
                         "time did not increase");
    return;
  }

  // Request a key frame when a Pli message was received, and it has been passed
  // long enough from the last time sending key frame request on receiving a Pli
  // message.
  if (frame_sender_->NeedsKeyFrame()) {
    const base::TimeDelta min_attempt_interval = std::max(
        kMinKeyFrameRequestInterval,
        kMinKeyFrameRequestFrameInterval * frame_sender_->TargetPlayoutDelay());

    if (last_time_attempted_to_resolve_pli_.is_null() ||
        ((reference_time - last_time_attempted_to_resolve_pli_) >
         min_attempt_interval)) {
      video_encoder_->GenerateKeyFrame();
      last_time_attempted_to_resolve_pli_ = reference_time;
    }
  }

  // Two video frames are needed to compute the exact media duration added by
  // the next frame.  If there are no frames in the encoder, compute a guess
  // based on the configured max frame rate.  Any error introduced by this
  // guess will be eliminated when |duration_in_encoder_| is updated in
  // OnEncodedVideoFrame().
  const base::TimeDelta duration_added_by_next_frame =
      frames_in_encoder_ > 0
          ? reference_time - last_enqueued_frame_reference_time_
          : base::Seconds(1.0 / frame_sender_->MaxFrameRate());

  number_of_frames_inserted_++;
  const CastStreamingFrameDropReason reason =
      frame_sender_->ShouldDropNextFrame(duration_added_by_next_frame);
  if (reason != CastStreamingFrameDropReason::kNotDropped) {
    base::TimeDelta new_target_delay =
        std::min(frame_sender_->CurrentRoundTripTime() * kRoundTripsNeeded +
                     base::Milliseconds(kConstantTimeMs),
                 max_playout_delay_);
    // In case of low latency mode, we prefer frame drops over increasing
    // playout time.
    if (!low_latency_mode_ &&
        new_target_delay > frame_sender_->TargetPlayoutDelay()) {
      // In case we detect user is no longer in a low latency mode and there is
      // a need to drop a frame, we ensure the playout delay is at-least the
      // the starting value for playing animated content.
      // This is intended to minimize freeze when moving from an interactive
      // session to watching animating content while being limited by end-to-end
      // delay.
      VLOG(1) << "Ensure playout time is at least " << min_playout_delay_;
      if (new_target_delay < min_playout_delay_)
        new_target_delay = min_playout_delay_;
      VLOG(1) << "New target delay: " << new_target_delay.InMilliseconds();
      playout_delay_change_cb_.Run(new_target_delay);
    }

    // Some encoder implementations have a frame window for analysis. Since we
    // are dropping this frame, unless we instruct the encoder to flush all the
    // frames that have been enqueued for encoding, frames_in_encoder_ and
    // last_enqueued_frame_reference_time_ will never be updated and we will
    // drop every subsequent frame for the rest of the session.
    video_encoder_->EmitFrames();

    number_of_frames_dropped_++;
    base::UmaHistogramEnumeration(kHistogramFrameDropped, reason);
    TRACE_EVENT_INSTANT2("cast.stream", "Video Frame Drop (raw frame)",
                         TRACE_EVENT_SCOPE_THREAD, "duration",
                         duration_added_by_next_frame, "reason", reason);
    return;
  }

  if (video_frame->visible_rect().IsEmpty()) {
    VLOG(1) << "Rejecting empty video frame.";
    return;
  }

  const int bitrate = frame_sender_->GetSuggestedBitrate(
      reference_time + frame_sender_->TargetPlayoutDelay(),
      frame_sender_->TargetPlayoutDelay());
  if (bitrate != last_bitrate_) {
    video_encoder_->SetBitRate(bitrate);
    last_bitrate_ = bitrate;
  }

  // Report the bitrate every 500 frames.
  constexpr int kSampleInterval = 500;
  frames_since_bitrate_reported_ =
      ++frames_since_bitrate_reported_ % kSampleInterval;
  if (frames_since_bitrate_reported_ == 0) {
    base::UmaHistogramMemoryKB(kHistogramBitrate, bitrate / 1000);
  }

  TRACE_COUNTER_ID1("cast.stream", "Video Target Bitrate", this, bitrate);

  if (base::FeatureList::IsEnabled(media::kCastStreamingPerformanceOverlay)) {
    video_frame = RenderPerformanceMetricsOverlay(
        frame_sender_->GetTargetPlayoutDelay(), low_latency_mode_, bitrate,
        frames_in_encoder_ + 1, last_reported_encoder_utilization_,
        last_reported_lossiness_, std::move(video_frame));
  }

  if (video_encoder_->EncodeVideoFrame(
          video_frame, reference_time,
          base::BindOnce(&VideoSender::OnEncodedVideoFrame, AsWeakPtr(),
                         video_frame, reference_time))) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(
        "cast.stream", "Video Encode", TRACE_ID_LOCAL(video_frame.get()),
        "rtp_timestamp", rtp_timestamp.lower_32_bits());
    frames_in_encoder_++;
    duration_in_encoder_ += duration_added_by_next_frame;
    last_enqueued_frame_rtp_timestamp_ = rtp_timestamp;
    last_enqueued_frame_reference_time_ = reference_time;
  } else {
    VLOG(1) << "Encoder rejected a frame.  Skipping...";
    TRACE_EVENT_INSTANT1("cast.stream", "Video Encode Reject",
                         TRACE_EVENT_SCOPE_THREAD,
                         "rtp_timestamp", rtp_timestamp.lower_32_bits());
  }
}

std::unique_ptr<VideoFrameFactory> VideoSender::CreateVideoFrameFactory() {
  return video_encoder_ ? video_encoder_->CreateVideoFrameFactory() : nullptr;
}

void VideoSender::SetTargetPlayoutDelay(
    base::TimeDelta new_target_playout_delay) {
  frame_sender_->SetTargetPlayoutDelay(new_target_playout_delay);
}

base::TimeDelta VideoSender::GetTargetPlayoutDelay() const {
  return frame_sender_->GetTargetPlayoutDelay();
}

base::WeakPtr<VideoSender> VideoSender::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

VideoSender::VideoSender() = default;

int VideoSender::GetNumberOfFramesInEncoder() const {
  return frames_in_encoder_;
}

base::TimeDelta VideoSender::GetEncoderBacklogDuration() const {
  return duration_in_encoder_;
}

void VideoSender::OnEncodedVideoFrame(
    scoped_refptr<media::VideoFrame> video_frame,
    const base::TimeTicks reference_time,
    std::unique_ptr<SenderEncodedFrame> encoded_frame) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  frames_in_encoder_--;
  DCHECK_GE(frames_in_encoder_, 0);

  // Update |duration_in_encoder_| so that |frame_sender_| doesn't regard the
  // encoder is really slow.
  duration_in_encoder_ = last_enqueued_frame_reference_time_ - reference_time;

  TRACE_EVENT_NESTABLE_ASYNC_END2(
      "cast.stream", "Video Encode", TRACE_ID_LOCAL(video_frame.get()),
      "encoder_utilization", last_reported_encoder_utilization_, "lossiness",
      last_reported_lossiness_);
  // The encoder drops a frame.
  if (!encoded_frame || encoded_frame->data.empty()) {
    DVLOG(3) << "Drop frame";
    return;
  }

  last_reported_encoder_utilization_ = encoded_frame->encoder_utilization;
  last_reported_lossiness_ = encoded_frame->lossiness;

  // Report the resource utilization for processing this frame.  Take the
  // greater of the two utilization values and attenuate them such that the
  // target utilization is reported as the maximum sustainable amount.
  const double attenuated_utilization =
      std::max(last_reported_encoder_utilization_, last_reported_lossiness_) /
      (kTargetUtilizationPercentage / 100.0);
  if (attenuated_utilization >= 0.0) {
    // Key frames are artificially capped to 1.0 because their actual
    // utilization is atypical compared to the other frames in the stream, and
    // this can misguide the producer of the input video frames.
    VideoCaptureFeedback feedback;
    feedback.resource_utilization =
        encoded_frame->dependency ==
                openscreen::cast::EncodedFrame::Dependency::kKeyFrame
            ? std::min(1.0, attenuated_utilization)
            : attenuated_utilization;
    if (feedback_cb_)
      feedback_cb_.Run(feedback);
  }

  const RtpTimeTicks rtp_timestamp = encoded_frame->rtp_timestamp;
  const CastStreamingFrameDropReason reason =
      frame_sender_->EnqueueFrame(std::move(encoded_frame));
  if (reason != CastStreamingFrameDropReason::kNotDropped) {
    // Since we have dropped an already encoded frame, which is much worse than
    // dropping a raw frame above, we need to flush the encoder and emit a new
    // keyframe.
    video_encoder_->EmitFrames();
    video_encoder_->GenerateKeyFrame();

    base::UmaHistogramEnumeration(kHistogramFrameDropped, reason);
    TRACE_EVENT_INSTANT2("cast.stream", "Video Frame Drop (already encoded)",
                         TRACE_EVENT_SCOPE_THREAD, "rtp_timestamp",
                         rtp_timestamp.lower_32_bits(), "reason", reason);
  }
}

}  // namespace media::cast

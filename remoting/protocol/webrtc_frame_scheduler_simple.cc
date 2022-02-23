// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_frame_scheduler_simple.h"

#include <algorithm>

#include "base/bind.h"
#include "base/cxx17_backports.h"
#include "base/time/default_tick_clock.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/webrtc_bandwidth_estimator.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {
namespace protocol {

namespace {

constexpr base::TimeDelta kTargetFrameInterval =
    base::Milliseconds(1000 / kTargetFrameRate);

// Minimum interval between frames needed to keep the connection alive. The
// client will request a key-frame if it does not receive any frames for a
// 3-second period. This is effectively a minimum frame-rate, so the value
// should not be too small, otherwise the client may waste CPU cycles on
// processing and rendering lots of identical frames.
constexpr base::TimeDelta kKeepAliveInterval = base::Milliseconds(2000);

// Baseline bandwidth to use for scheduling captures. This is only used
// until the next OnTargetBitrateChanged() notification, typically after
// 2 or 3 capture/encode cycles. Any realistic value should work OK - the
// chosen value is the current upper limit for relay connections.
constexpr int kBaselineBandwidthKbps = 8000;

}  // namespace

// TODO(zijiehe): Use |options| to select bandwidth estimator.
WebrtcFrameSchedulerSimple::WebrtcFrameSchedulerSimple(
    const SessionOptions& options)
    : tick_clock_(base::DefaultTickClock::GetInstance()),
      pacing_bucket_(LeakyBucket::kUnlimitedDepth,
                     kBaselineBandwidthKbps * 1000 / 8),
      bandwidth_estimator_(new WebrtcBandwidthEstimator()) {
  // Set up bandwidth-estimators with an initial rate so that captures can be
  // scheduled when the encoder is ready. With the standard encoding pipeline
  // (has_internal_source == false), WebRTC does not create the encoder until
  // after the first frame is captured and sent to the VideoTrack's output
  // sink. Bandwidth updates cannot be received until after this occurs.
  bandwidth_estimator_->OnBitrateEstimation(kBaselineBandwidthKbps);
  processing_time_estimator_.SetBandwidthKbps(kBaselineBandwidthKbps);
}

WebrtcFrameSchedulerSimple::~WebrtcFrameSchedulerSimple() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebrtcFrameSchedulerSimple::OnKeyFrameRequested() {
  DCHECK(thread_checker_.CalledOnValidThread());
  key_frame_request_ = true;
  ScheduleNextFrame();
}

void WebrtcFrameSchedulerSimple::OnTargetBitrateChanged(int bandwidth_kbps) {
  DCHECK(thread_checker_.CalledOnValidThread());
  bandwidth_estimator_->OnReceivedAck();
  bandwidth_estimator_->OnBitrateEstimation(bandwidth_kbps);
  processing_time_estimator_.SetBandwidthKbps(
      bandwidth_estimator_->GetBitrateKbps());
  pacing_bucket_.UpdateRate(bandwidth_estimator_->GetBitrateKbps() * 1000 / 8,
                            tick_clock_->NowTicks());
  ScheduleNextFrame();
}

void WebrtcFrameSchedulerSimple::Start(
    const base::RepeatingClosure& capture_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  capture_callback_ = capture_callback;
}

void WebrtcFrameSchedulerSimple::Pause(bool pause) {
  DCHECK(thread_checker_.CalledOnValidThread());

  paused_ = pause;
  if (paused_) {
    capture_timer_.Stop();
  } else {
    ScheduleNextFrame();
  }
}

void WebrtcFrameSchedulerSimple::OnFrameCaptured(
    const webrtc::DesktopFrame* frame) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(frame_pending_);
  frame_pending_ = false;

  // Null |frame| indicates a capturer error.
  if (!frame) {
    frame_pending_ = false;
    ScheduleNextFrame();
    return;
  }

  key_frame_request_ = false;

  ScheduleNextFrame();
}

void WebrtcFrameSchedulerSimple::OnFrameEncoded(
    WebrtcVideoEncoder::EncodeResult encode_result,
    const WebrtcVideoEncoder::EncodedFrame* encoded_frame) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::TimeTicks now = tick_clock_->NowTicks();

  // Calculate |send_pending_delay| before refilling |pacing_bucket_|.
  if (encoded_frame && encoded_frame->stats) {
    encoded_frame->stats->send_pending_delay =
        std::max(base::TimeDelta(), pacing_bucket_.GetEmptyTime() - now);
  }

  // TODO(zijiehe): |encoded_frame|->data.empty() is unreasonable, we should try
  // to get rid of it in WebrtcVideoEncoder layer.
  if (encoded_frame && !encoded_frame->data.empty()) {
    pacing_bucket_.RefillOrSpill(encoded_frame->data.size(), now);

    processing_time_estimator_.FinishFrame(*encoded_frame);
  }

  ScheduleNextFrame();

  bandwidth_estimator_->OnSendingFrame(*encoded_frame);
}

void WebrtcFrameSchedulerSimple::OnEncodedFrameSent(
    webrtc::EncodedImageCallback::Result result,
    const WebrtcVideoEncoder::EncodedFrame& frame) {}

void WebrtcFrameSchedulerSimple::SetMaxFramerateFps(int max_framerate_fps) {
  // TODO(http://crbug.com/1268253): Implement this.
  encoder_ready_ = true;
  ScheduleNextFrame();
}

void WebrtcFrameSchedulerSimple::SetTickClockForTest(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

void WebrtcFrameSchedulerSimple::ScheduleNextFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());
  base::TimeTicks now = tick_clock_->NowTicks();

  if (!encoder_ready_ || paused_ || pacing_bucket_.rate() == 0 ||
      capture_callback_.is_null() || frame_pending_) {
    return;
  }

  base::TimeTicks target_capture_time;
  if (!last_capture_started_time_.is_null()) {
    // Try to set the capture time so that (if the estimated processing time is
    // accurate) the new frame is ready to be sent just when the previous frame
    // is finished sending.
    target_capture_time = pacing_bucket_.GetEmptyTime() -
        processing_time_estimator_.EstimatedProcessingTime(key_frame_request_);

    // Ensure that the capture rate is capped by kTargetFrameInterval, to avoid
    // excessive CPU usage by the capturer.
    // Also ensure that the video does not freeze for excessively long periods.
    // This protects against, for example, bugs in the b/w estimator or the
    // LeakyBucket implementation which may result in unbounded wait times.
    // If the network is such that it really takes > 2 or 3 seconds to send one
    // video frame, then this upper-bound cap could result in packet-loss,
    // triggering PLI (key-frame request). But the session would be already
    // unusable under such network conditions. And the client would trigger PLI
    // anyway if it doesn't receive any video for > 3 seconds.
    target_capture_time = base::clamp(
        target_capture_time, last_capture_started_time_ + kTargetFrameInterval,
        last_capture_started_time_ + kKeepAliveInterval);
  }

  target_capture_time = std::max(target_capture_time, now);

  capture_timer_.Start(
      FROM_HERE, target_capture_time - now,
      base::BindOnce(&WebrtcFrameSchedulerSimple::CaptureNextFrame,
                     base::Unretained(this)));
}

void WebrtcFrameSchedulerSimple::CaptureNextFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!frame_pending_);
  last_capture_started_time_ = tick_clock_->NowTicks();
  frame_pending_ = true;
  capture_callback_.Run();
}

}  // namespace protocol
}  // namespace remoting

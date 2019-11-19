// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_frame_scheduler_simple.h"

#include <algorithm>

#include "base/bind.h"
#include "base/numerics/ranges.h"
#include "base/time/default_tick_clock.h"
#include "remoting/base/constants.h"
#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/webrtc_bandwidth_estimator.h"
#include "remoting/protocol/webrtc_dummy_video_encoder.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting {
namespace protocol {

namespace {

// Number of samples used to estimate processing time for the next frame.
const int kStatsWindow = 5;

constexpr base::TimeDelta kTargetFrameInterval =
    base::TimeDelta::FromMilliseconds(1000 / kTargetFrameRate);

// Target quantizer at which stop the encoding top-off.
const int kTargetQuantizerForVp8TopOff = 30;

// Maximum quantizer at which to encode frames. Lowering this value will
// improve image quality (in cases of low-bandwidth or large frames) at the
// cost of latency. Increasing the value will improve latency (in these cases)
// at the cost of image quality, resulting in longer top-off times.
// TODO(lambroslambrou): Move this, and any other encoder-specific parameters
// into the WebrtcVideoEncoderXXX implementations.
const int kMaxQuantizer = 50;

const int64_t kPixelsPerMegapixel = 1000000;

// Threshold in number of updated pixels used to detect "big" frames. These
// frames update significant portion of the screen compared to the preceding
// frames. For these frames min quantizer may need to be adjusted in order to
// ensure that they get delivered to the client as soon as possible, in exchange
// for lower-quality image.
const int kBigFrameThresholdPixels = 300000;

// Estimated size (in bytes per megapixel) of encoded frame at target quantizer
// value (see kTargetQuantizerForVp8TopOff). Compression ratio varies depending
// on the image, so this is just a rough estimate. It's used to predict when
// encoded "big" frame may be too large to be delivered to the client quickly.
const int kEstimatedBytesPerMegapixel = 100000;

// Minimum interval between frames needed to keep the connection alive. The
// client will request a key-frame if it does not receive any frames for a
// 3-second period. This is effectively a minimum frame-rate, so the value
// should not be too small, otherwise the client may waste CPU cycles on
// processing and rendering lots of identical frames.
constexpr base::TimeDelta kKeepAliveInterval =
    base::TimeDelta::FromMilliseconds(2000);

int64_t GetRegionArea(const webrtc::DesktopRegion& region) {
  int64_t result = 0;
  for (webrtc::DesktopRegion::Iterator r(region); !r.IsAtEnd(); r.Advance()) {
    result += r.rect().width() * r.rect().height();
  }
  return result;
}

}  // namespace

// TODO(zijiehe): Use |options| to select bandwidth estimator.
WebrtcFrameSchedulerSimple::WebrtcFrameSchedulerSimple(
    const SessionOptions& options)
    : tick_clock_(base::DefaultTickClock::GetInstance()),
      pacing_bucket_(LeakyBucket::kUnlimitedDepth, 0),
      updated_region_area_(kStatsWindow),
      bandwidth_estimator_(new WebrtcBandwidthEstimator()) {}

WebrtcFrameSchedulerSimple::~WebrtcFrameSchedulerSimple() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void WebrtcFrameSchedulerSimple::OnKeyFrameRequested() {
  DCHECK(thread_checker_.CalledOnValidThread());
  encoder_ready_ = true;
  key_frame_request_ = true;
  ScheduleNextFrame();
}

void WebrtcFrameSchedulerSimple::OnChannelParameters(int packet_loss,
                                                     base::TimeDelta rtt) {
  DCHECK(thread_checker_.CalledOnValidThread());

  bandwidth_estimator_->UpdateRtt(rtt);
  rtt_estimate_ = rtt;
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
    WebrtcDummyVideoEncoderFactory* video_encoder_factory,
    const base::Closure& capture_callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  capture_callback_ = capture_callback;
  video_encoder_factory->SetVideoChannelStateObserver(
      weak_factory_.GetWeakPtr());
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

bool WebrtcFrameSchedulerSimple::OnFrameCaptured(
    const webrtc::DesktopFrame* frame,
    WebrtcVideoEncoder::FrameParams* params_out) {
  DCHECK(thread_checker_.CalledOnValidThread());

  base::TimeTicks now = tick_clock_->NowTicks();

  // Null |frame| indicates a capturer error.
  if (!frame) {
    frame_pending_ = false;
    ScheduleNextFrame();
    return false;
  }

  if (frame->updated_region().is_empty()) {
    // If we've captured an empty frame we still need to encode and send the
    // previous frame when top-off is active or a key-frame was requested. But
    // it makes sense only when we have a frame to send, i.e. there is nothing
    // to send if first capture request failed.
    // Also send previous frame if there haven't been any frame updates for a
    // while, to keep the video stream alive. Otherwise, the client will
    // think the video stream is frozen and will attempt to recover it by
    // requesting a key-frame every few seconds, wasting network resources.
    bool send_frame =
        top_off_is_active_ || key_frame_request_ ||
        (now - latest_frame_encode_start_time_ > kKeepAliveInterval);
    if (!send_frame) {
      frame_pending_ = false;
      ScheduleNextFrame();
      return false;
    }
  }

  // Encoder uses frame duration to calculate portion of the target bitrate it
  // can use for this frame. Higher values normally will cause bigger encoded
  // frames that will take longer to be delivered to the client. To keep
  // end-to-end latency low always pass the target frame duration. The actual
  // interval between frames can be longer than the target value, depending on
  // the size of the encoded frames.
  params_out->duration = kTargetFrameInterval;
  params_out->fps = processing_time_estimator_.EstimatedFrameRate();

  latest_frame_encode_start_time_ = now;

  params_out->bitrate_kbps = pacing_bucket_.rate() * 8 / 1000;
  params_out->key_frame = key_frame_request_;
  key_frame_request_ = false;

  params_out->vpx_min_quantizer = 10;

  int64_t updated_area = params_out->key_frame
                             ? frame->size().width() * frame->size().height()
                             : GetRegionArea(frame->updated_region());

  // TODO(zijiehe): This logic should be removed if a codec without top-off
  // supported is used.
  // If bandwidth is being underutilized then libvpx is likely to choose the
  // minimum allowed quantizer value, which means that encoded frame size may
  // be significantly bigger than the bandwidth allows. Detect this case and set
  // vpx_min_quantizer to the maximum value. The quality will be topped off
  // later.
  if (updated_area - updated_region_area_.Max() > kBigFrameThresholdPixels) {
    int expected_frame_size =
        updated_area * kEstimatedBytesPerMegapixel / kPixelsPerMegapixel;
    base::TimeDelta expected_send_delay =
        pacing_bucket_.rate() ? base::TimeDelta::FromMicroseconds(
                                    base::Time::kMicrosecondsPerSecond *
                                    expected_frame_size / pacing_bucket_.rate())
                              : base::TimeDelta::Max();
    if (expected_send_delay > kTargetFrameInterval) {
      params_out->vpx_min_quantizer = kMaxQuantizer;
    }
  }

  updated_region_area_.Record(updated_area);

  params_out->vpx_max_quantizer = kMaxQuantizer;

  params_out->clear_active_map = !top_off_is_active_;

  return true;
}

void WebrtcFrameSchedulerSimple::OnFrameEncoded(
    const WebrtcVideoEncoder::EncodedFrame* encoded_frame,
    HostFrameStats* frame_stats) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(frame_pending_);
  frame_pending_ = false;

  base::TimeTicks now = tick_clock_->NowTicks();

  if (frame_stats) {
    // Calculate |send_pending_delay| before refilling |pacing_bucket_|.
    frame_stats->send_pending_delay =
        std::max(base::TimeDelta(), pacing_bucket_.GetEmptyTime() - now);
  }

  // TODO(zijiehe): |encoded_frame|->data.empty() is unreasonable, we should try
  // to get rid of it in WebrtcVideoEncoder layer.
  if (!encoded_frame || encoded_frame->data.empty()) {
    top_off_is_active_ = false;
  } else {
    pacing_bucket_.RefillOrSpill(encoded_frame->data.size(), now);

    processing_time_estimator_.FinishFrame(*encoded_frame);

    // Top-off until the target quantizer value is reached.
    top_off_is_active_ =
        encoded_frame->quantizer > kTargetQuantizerForVp8TopOff;
  }

  ScheduleNextFrame();

  if (frame_stats) {
    frame_stats->rtt_estimate = rtt_estimate_;
    frame_stats->bandwidth_estimate_kbps =
        bandwidth_estimator_->GetBitrateKbps();
  }

  bandwidth_estimator_->OnSendingFrame(*encoded_frame);
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
    target_capture_time = base::ClampToRange(
        target_capture_time, last_capture_started_time_ + kTargetFrameInterval,
        last_capture_started_time_ + kKeepAliveInterval);
  }

  target_capture_time = std::max(target_capture_time, now);

  capture_timer_.Start(FROM_HERE, target_capture_time - now,
                       base::Bind(&WebrtcFrameSchedulerSimple::CaptureNextFrame,
                                  base::Unretained(this)));
}

void WebrtcFrameSchedulerSimple::CaptureNextFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!frame_pending_);
  last_capture_started_time_ = tick_clock_->NowTicks();
  processing_time_estimator_.StartFrame();
  frame_pending_ = true;
  capture_callback_.Run();
}

}  // namespace protocol
}  // namespace remoting

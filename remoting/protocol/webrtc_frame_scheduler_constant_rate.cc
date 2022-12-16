// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_frame_scheduler_constant_rate.h"

#include <algorithm>

#include "base/logging.h"
#include "base/time/time.h"

namespace remoting::protocol {

WebrtcFrameSchedulerConstantRate::WebrtcFrameSchedulerConstantRate() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

WebrtcFrameSchedulerConstantRate::~WebrtcFrameSchedulerConstantRate() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void WebrtcFrameSchedulerConstantRate::Start(
    const base::RepeatingClosure& capture_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  capture_callback_ = capture_callback;
}

void WebrtcFrameSchedulerConstantRate::Pause(bool pause) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  paused_ = pause;
  if (paused_) {
    capture_timer_.Stop();
  } else {
    ScheduleNextFrame();
  }
}

void WebrtcFrameSchedulerConstantRate::OnFrameCaptured(
    const webrtc::DesktopFrame* frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(frame_pending_);

  frame_pending_ = false;
  ScheduleNextFrame();
}

void WebrtcFrameSchedulerConstantRate::SetMaxFramerateFps(int max_framerate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  max_framerate_fps_ = max_framerate;
  ScheduleNextFrame();
}

void WebrtcFrameSchedulerConstantRate::BoostCaptureRate(
    base::TimeDelta capture_interval,
    base::TimeDelta duration) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Clamp |boost_capture_interval_| as the capture pipeline starts acting weird
  // when we try to capture at sub-millisecond intervals.
  boost_capture_interval_ = std::max(capture_interval, base::Milliseconds(1));
  boost_window_ = base::TimeTicks::Now() + duration;

  ScheduleNextFrame();
}

void WebrtcFrameSchedulerConstantRate::ScheduleNextFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::TimeTicks now = base::TimeTicks::Now();

  if (paused_) {
    VLOG(0) << "Not scheduling capture because stream is paused.";
    return;
  }

  if (!capture_callback_) {
    VLOG(0) << "Not scheduling capture because callback is not provided.";
    return;
  }

  if (frame_pending_) {
    // This might be logged every time the capture takes more time than the
    // polling period. To avoid spamming, only log if the capture has been
    // pending for an unreasonable length of time.
    DCHECK(!last_capture_started_time_.is_null());
    if (now - last_capture_started_time_ > base::Seconds(1)) {
      // Log this as an error, because a capture should never be pending for
      // this length of time.
      LOG(ERROR) << "Not scheduling capture because a capture is pending.";
    }
    return;
  }

  if (max_framerate_fps_ == 0) {
    VLOG(0) << "Not scheduling capture because framerate is set to 0.";
    return;
  }

  // Captures should be scheduled at least 1ms apart, otherwise WebRTC's video
  // stream encoder complains about non-increasing frame timestamps, which can
  // affect some unittests.
  base::TimeDelta capture_interval =
      std::max(base::Seconds(1) / max_framerate_fps_, base::Milliseconds(1));

  // Use the boosted capture interval if we are within |boost_window_|.
  if (!boost_window_.is_null()) {
    if (boost_window_ > now) {
      capture_interval = boost_capture_interval_;
    } else {
      boost_window_ = {};
    }
  }

  base::TimeDelta delay;
  if (!last_capture_started_time_.is_null()) {
    base::TimeTicks target_capture_time =
        std::max(last_capture_started_time_ + capture_interval, now);
    delay = std::max(target_capture_time - now, base::Milliseconds(1));
  }

  capture_timer_.Start(FROM_HERE, delay, this,
                       &WebrtcFrameSchedulerConstantRate::CaptureNextFrame);
}

void WebrtcFrameSchedulerConstantRate::CaptureNextFrame() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!frame_pending_);

  last_capture_started_time_ = base::TimeTicks::Now();
  frame_pending_ = true;
  capture_callback_.Run();
}

}  // namespace remoting::protocol

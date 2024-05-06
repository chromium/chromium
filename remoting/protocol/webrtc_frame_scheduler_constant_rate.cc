// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/webrtc_frame_scheduler_constant_rate.h"

#include <algorithm>

#include "base/logging.h"
#include "base/system/sys_info.h"
#include "base/time/time.h"

namespace remoting::protocol {

namespace {
base::TimeDelta GetPostTaskAdjustment() {
  int proccessor_count = base::SysInfo::NumberOfProcessors();
  if (proccessor_count < 16) {
    // Don't change the scheduler timing on these machines as we don't want to
    // overload the machine.
    return base::Milliseconds(0);
  }

  // We've observed the encoding rate in the client as being a couple of frames
  // lower than the target. By adjusting the capture rate by ~2ms, the host will
  // generate frames at, or slightly above, the target frame rate. If a value of
  // 1ms is used, then the host will generate frames at, or slightly below, the
  // target. The higher of the two was chosen for better performance on high-CPU
  // machines.
  return base::Milliseconds(2);
}
}  // namespace

WebrtcFrameSchedulerConstantRate::WebrtcFrameSchedulerConstantRate()
    : post_task_adjustment_(GetPostTaskAdjustment()) {
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
  DCHECK_GE(max_framerate, 0);

  max_framerate_fps_ = std::min(max_framerate, 1000);
  if (max_framerate_fps_ > 0) {
    // Calculate the interval used to schedule each frame capture and account
    // for the time used when posting tasks (~1-2ms per frame). We also set a
    // floor at 1ms as some of our dependencies have problems when frames are
    // delivered with the same timestamp.
    capture_interval_ =
        std::max(base::Hertz(max_framerate_fps_) - post_task_adjustment_,
                 base::Milliseconds(1));
  } else {
    capture_interval_ = {};
  }

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

  base::TimeDelta delay;
  if (!last_capture_started_time_.is_null()) {
    auto capture_interval = capture_interval_;

    // Use the boosted capture interval if we are within |boost_window_|.
    if (!boost_window_.is_null()) {
      if (boost_window_ > now) {
        capture_interval = boost_capture_interval_;
      } else {
        boost_window_ = {};
      }
    }

    base::TimeTicks target_capture_time =
        last_capture_started_time_ + capture_interval;

    // Captures should be scheduled at least 1ms apart, otherwise WebRTC's video
    // stream logic will complain about non-increasing frame timestamps, which
    // can affect some unittests.
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

void WebrtcFrameSchedulerConstantRate::SetPostTaskAdjustmentForTest(
    base::TimeDelta post_task_adjustment) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  post_task_adjustment_ = post_task_adjustment;
}

}  // namespace remoting::protocol

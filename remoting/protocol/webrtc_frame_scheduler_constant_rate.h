// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_CONSTANT_RATE_H_
#define REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_CONSTANT_RATE_H_

#include "remoting/protocol/webrtc_frame_scheduler.h"

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/timer.h"

namespace remoting::protocol {

// WebrtcFrameSchedulerConstantRate is an implementation of WebrtcFrameScheduler
// that captures frames at a fixed rate. It uses the maximum frame rate provided
// by SetMaxFramerateFps().
class WebrtcFrameSchedulerConstantRate : public WebrtcFrameScheduler {
 public:
  WebrtcFrameSchedulerConstantRate();

  WebrtcFrameSchedulerConstantRate(const WebrtcFrameSchedulerConstantRate&) =
      delete;
  WebrtcFrameSchedulerConstantRate& operator=(
      const WebrtcFrameSchedulerConstantRate&) = delete;

  ~WebrtcFrameSchedulerConstantRate() override;

  // WebrtcFrameScheduler implementation.
  void Start(const base::RepeatingClosure& capture_callback) override;
  void Pause(bool pause) override;
  void OnFrameCaptured(const webrtc::DesktopFrame* frame) override;
  void SetMaxFramerateFps(int max_framerate_fps) override;

  // Temporarily adjusts the capture rate to |capture_interval| for the next
  // |duration|.
  void BoostCaptureRate(base::TimeDelta capture_interval,
                        base::TimeDelta duration) override;

  void SetPostTaskAdjustmentForTest(base::TimeDelta post_task_adjustment);

 private:
  void ScheduleNextFrame();
  void CaptureNextFrame();

  base::RepeatingClosure capture_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool paused_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  base::OneShotTimer capture_timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::TimeTicks last_capture_started_time_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Set to true when a frame is being captured. Used to avoid scheduling more
  // than one capture in parallel.
  bool frame_pending_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Framerate for scheduling frames. Initially 0 to prevent scheduling before
  // the output sink has been added.
  int max_framerate_fps_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  base::TimeDelta capture_interval_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::TimeDelta post_task_adjustment_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::TimeDelta boost_capture_interval_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::TimeTicks boost_window_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_CONSTANT_RATE_H_

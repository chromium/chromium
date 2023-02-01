// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/no_op_webrtc_frame_scheduler.h"

#include "base/notreached.h"

namespace remoting::protocol {

NoOpWebrtcFrameScheduler::NoOpWebrtcFrameScheduler(DesktopCapturer* capturer)
    : capturer_(capturer) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

NoOpWebrtcFrameScheduler::~NoOpWebrtcFrameScheduler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NoOpWebrtcFrameScheduler::Start(
    const base::RepeatingClosure& capture_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NoOpWebrtcFrameScheduler::Pause(bool pause) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (pause) {
    capturer_->SetMaxFrameRate(0u);
  } else {
    capturer_->SetMaxFrameRate(last_frame_rate_);
  }
}

void NoOpWebrtcFrameScheduler::OnFrameCaptured(
    const webrtc::DesktopFrame* frame) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NoOpWebrtcFrameScheduler::SetMaxFramerateFps(int max_framerate_fps) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  last_frame_rate_ = max_framerate_fps;
  capturer_->SetMaxFrameRate(last_frame_rate_);
}

void NoOpWebrtcFrameScheduler::BoostCaptureRate(
    base::TimeDelta capture_interval,
    base::TimeDelta duration) {
  NOTIMPLEMENTED() << "Boosting frame rate is not supported for wayland";
}

}  // namespace remoting::protocol

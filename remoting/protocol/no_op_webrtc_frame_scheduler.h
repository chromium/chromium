// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_NO_OP_WEBRTC_FRAME_SCHEDULER_H_
#define REMOTING_PROTOCOL_NO_OP_WEBRTC_FRAME_SCHEDULER_H_

#include "remoting/protocol/webrtc_frame_scheduler.h"

#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "remoting/protocol/desktop_capturer.h"

namespace remoting::protocol {

// This no-op scheduler is used in place of constant frame rate scheduler
// when we have a capturer that supports sending us events whenever a frame is
// captured.
class NoOpWebrtcFrameScheduler : public WebrtcFrameScheduler {
 public:
  explicit NoOpWebrtcFrameScheduler(DesktopCapturer* capturer);
  ~NoOpWebrtcFrameScheduler() override;

  // WebrtcFrameScheduler Interface.
  void Start(const base::RepeatingClosure& capture_callback) override;
  void Pause(bool pause) override;
  void OnFrameCaptured(const webrtc::DesktopFrame* frame) override;
  void SetMaxFramerateFps(int max_framerate_fps) override;
  void BoostCaptureRate(base::TimeDelta capture_interval,
                        base::TimeDelta duration) override;

 private:
  raw_ptr<DesktopCapturer> capturer_ GUARDED_BY_CONTEXT(sequence_checker_);
  uint32_t last_frame_rate_ GUARDED_BY_CONTEXT(sequence_checker_) = 60;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_NO_OP_WEBRTC_FRAME_SCHEDULER_H_

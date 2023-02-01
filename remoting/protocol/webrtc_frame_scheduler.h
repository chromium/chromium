// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_H_
#define REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_H_

#include "base/functional/callback_forward.h"

#include "base/time/time.h"

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting::protocol {

// An abstract interface for frame schedulers, which are responsible for
// scheduling when video frames are captured and for defining encoding
// parameters for each frame.
class WebrtcFrameScheduler {
 public:
  WebrtcFrameScheduler() = default;
  virtual ~WebrtcFrameScheduler() = default;

  // Starts the scheduler. |capture_callback| will be called whenever a new
  // frame should be captured.
  virtual void Start(const base::RepeatingClosure& capture_callback) = 0;

  // Pause and resumes the scheduler.
  virtual void Pause(bool pause) = 0;

  // Called after |frame| has been captured. |frame| may be set to nullptr if
  // the capture request failed.
  virtual void OnFrameCaptured(const webrtc::DesktopFrame* frame) = 0;

  // Called when WebRTC requests the VideoTrackSource to provide frames at a
  // maximum framerate.
  virtual void SetMaxFramerateFps(int max_framerate_fps) = 0;

  // Temporarily adjusts the capture rate to |capture_interval| for the next
  // |duration|.
  virtual void BoostCaptureRate(base::TimeDelta capture_interval,
                                base::TimeDelta duration) = 0;
};

}  // namespace remoting::protocol

#endif  // REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_H_

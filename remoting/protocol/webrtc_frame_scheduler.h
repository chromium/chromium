// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_H_
#define REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_H_

#include "base/callback_forward.h"
#include "remoting/protocol/video_channel_state_observer.h"

namespace webrtc {
class DesktopFrame;
}  // namespace webrtc

namespace remoting {
namespace protocol {

// An abstract interface for frame schedulers, which are responsible for
// scheduling when video frames are captured and for defining encoding
// parameters for each frame.
class WebrtcFrameScheduler : public VideoChannelStateObserver {
 public:
  WebrtcFrameScheduler() = default;
  ~WebrtcFrameScheduler() override = default;

  // Starts the scheduler. |capture_callback| will be called whenever a new
  // frame should be captured.
  virtual void Start(const base::RepeatingClosure& capture_callback) = 0;

  // Pause and resumes the scheduler.
  virtual void Pause(bool pause) = 0;

  // Called after |frame| has been captured. |frame| may be set to nullptr
  // if the capture request failed.
  virtual void OnFrameCaptured(const webrtc::DesktopFrame* frame) = 0;

  // Called when WebRTC requests the VideoTrackSource to provide frames
  // at a maximum framerate.
  virtual void SetMaxFramerateFps(int max_framerate_fps) = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_H_

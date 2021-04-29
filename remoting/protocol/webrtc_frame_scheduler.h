// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_H_
#define REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "remoting/codec/webrtc_video_encoder.h"
#include "remoting/protocol/video_channel_state_observer.h"

namespace remoting {
namespace protocol {

struct HostFrameStats;

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

  // Called after |frame| has been captured to get encoding parameters for the
  // frame. Returns false if the frame should be dropped (e.g. when there are no
  // changes), true otherwise. |frame| may be set to nullptr if the capture
  // request failed.
  virtual bool OnFrameCaptured(const webrtc::DesktopFrame* frame,
                               WebrtcVideoEncoder::FrameParams* params_out) = 0;

  // Writes the following bandwidth-related statistics to |frame_stats_out|:
  // * bandwidth_estimate_kbps
  // * rtt_estimate
  // * send_pending_delay - an estimate of the delay (due to WebRTC's pacing
  //   buffer) before the recently-encoded frame will be sent.
  // This should be called just after OnFrameEncoded().
  virtual void GetSchedulerStats(HostFrameStats& frame_stats_out) const = 0;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_H_

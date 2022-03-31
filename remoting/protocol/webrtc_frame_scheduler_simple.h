// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_SIMPLE_H_
#define REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_SIMPLE_H_

#include "base/memory/raw_ptr.h"
#include "remoting/protocol/webrtc_frame_scheduler.h"

#include <memory>

#include "base/containers/queue.h"
#include "base/threading/thread_checker.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "remoting/base/leaky_bucket.h"
#include "remoting/base/running_samples.h"
#include "remoting/base/session_options.h"
#include "remoting/codec/frame_processing_time_estimator.h"

namespace remoting {
namespace protocol {

class BandwidthEstimator;

// WebrtcFrameSchedulerSimple is a simple implementation of WebrtcFrameScheduler
// that always keeps only one frame in the pipeline. It schedules each frame
// such that it is encoded and ready to be sent by the time previous one is
// expected to finish sending.
class WebrtcFrameSchedulerSimple : public WebrtcFrameScheduler {
 public:
  explicit WebrtcFrameSchedulerSimple(const SessionOptions& options);
  ~WebrtcFrameSchedulerSimple() override;

  // VideoChannelStateObserver implementation.
  void OnKeyFrameRequested() override;
  void OnTargetBitrateChanged(int bitrate_kbps) override;
  void OnFrameEncoded(
      WebrtcVideoEncoder::EncodeResult encode_result,
      const WebrtcVideoEncoder::EncodedFrame* encoded_frame) override;
  void OnEncodedFrameSent(
      webrtc::EncodedImageCallback::Result result,
      const WebrtcVideoEncoder::EncodedFrame& frame) override;

  // WebrtcFrameScheduler implementation.
  void Start(const base::RepeatingClosure& capture_callback) override;
  void Pause(bool pause) override;
  void OnFrameCaptured(const webrtc::DesktopFrame* frame) override;
  void SetMaxFramerateFps(int max_framerate_fps) override;

  // Allows unit-tests to provide a mock clock.
  void SetTickClockForTest(const base::TickClock* tick_clock);

 private:
  void ScheduleNextFrame();
  void CaptureNextFrame();

  // A TimeTicks provider which defaults to using a real system clock, but can
  // be replaced for unittests.
  raw_ptr<const base::TickClock> tick_clock_;

  base::RepeatingClosure capture_callback_;
  bool paused_ = false;

  // Set to true when the encoder is ready to receive frames (which is when the
  // output sink gets added to the VideoTrackSource). The sink's requested
  // framerate will then be passed to SetMaxFramerateFps(), which will set this
  // flag.
  bool encoder_ready_ = false;

  // Set to true when a key frame was requested.
  bool key_frame_request_ = false;

  base::TimeTicks last_capture_started_time_;

  LeakyBucket pacing_bucket_;

  // Set to true when a frame is being captured.
  bool frame_pending_ = false;

  // Accumulator for capture and encoder delay history, as well as the transit
  // time.
  FrameProcessingTimeEstimator processing_time_estimator_;

  base::OneShotTimer capture_timer_;

  // Estimates the bandwidth.
  const std::unique_ptr<BandwidthEstimator> bandwidth_estimator_;

  base::ThreadChecker thread_checker_;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_SIMPLE_H_

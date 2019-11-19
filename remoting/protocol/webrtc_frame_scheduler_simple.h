// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_SIMPLE_H_
#define REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_SIMPLE_H_

#include "remoting/protocol/webrtc_frame_scheduler.h"

#include <memory>

#include "base/containers/queue.h"
#include "base/threading/thread_checker.h"
#include "base/time/tick_clock.h"
#include "base/timer/timer.h"
#include "remoting/base/leaky_bucket.h"
#include "remoting/base/running_samples.h"
#include "remoting/base/session_options.h"
#include "remoting/codec/frame_processing_time_estimator.h"
#include "remoting/protocol/video_channel_state_observer.h"

namespace remoting {
namespace protocol {

class BandwidthEstimator;

// WebrtcFrameSchedulerSimple is a simple implementation of WebrtcFrameScheduler
// that always keeps only one frame in the pipeline. It schedules each frame
// such that it is encoded and ready to be sent by the time previous one is
// expected to finish sending.
class WebrtcFrameSchedulerSimple : public VideoChannelStateObserver,
                                   public WebrtcFrameScheduler {
 public:
  explicit WebrtcFrameSchedulerSimple(const SessionOptions& options);
  ~WebrtcFrameSchedulerSimple() override;

  // VideoChannelStateObserver implementation.
  void OnKeyFrameRequested() override;
  void OnChannelParameters(int packet_loss, base::TimeDelta rtt) override;
  void OnTargetBitrateChanged(int bitrate_kbps) override;

  // WebrtcFrameScheduler implementation.
  void Start(WebrtcDummyVideoEncoderFactory* video_encoder_factory,
             const base::Closure& capture_callback) override;
  void Pause(bool pause) override;
  bool OnFrameCaptured(const webrtc::DesktopFrame* frame,
                       WebrtcVideoEncoder::FrameParams* params_out) override;
  void OnFrameEncoded(const WebrtcVideoEncoder::EncodedFrame* encoded_frame,
                      HostFrameStats* frame_stats) override;

  // Allows unit-tests to provide a mock clock.
  void SetTickClockForTest(const base::TickClock* tick_clock);

 private:
  void ScheduleNextFrame();
  void CaptureNextFrame();

  // A TimeTicks provider which defaults to using a real system clock, but can
  // be replaced for unittests.
  const base::TickClock* tick_clock_;

  base::Closure capture_callback_;
  bool paused_ = false;

  // Set to true after the first key frame is requested.
  bool encoder_ready_ = false;

  // Set to true when a key frame was requested.
  bool key_frame_request_ = false;

  base::TimeTicks last_capture_started_time_;

  // Set in OnFrameCaptured() whenever a (non-null) frame (possibly with an
  // empty updated region) is sent to the encoder. Empty frames are still sent,
  // but at a throttled rate.
  base::TimeTicks latest_frame_encode_start_time_;

  LeakyBucket pacing_bucket_;

  // Set to true when a frame is being captured or encoded.
  bool frame_pending_ = false;

  base::TimeDelta rtt_estimate_;

  // Set to true when encoding unchanged frames for top-off.
  bool top_off_is_active_ = false;

  // Accumulator for capture and encoder delay history, as well as the transit
  // time.
  FrameProcessingTimeEstimator processing_time_estimator_;

  // Accumulator for updated region area in the previously encoded frames.
  RunningSamples updated_region_area_;

  base::OneShotTimer capture_timer_;

  // Estimates the bandwidth.
  const std::unique_ptr<BandwidthEstimator> bandwidth_estimator_;

  base::ThreadChecker thread_checker_;
  base::WeakPtrFactory<WebrtcFrameSchedulerSimple> weak_factory_{this};
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_WEBRTC_FRAME_SCHEDULER_SIMPLE_H_

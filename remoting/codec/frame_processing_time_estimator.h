// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CODEC_FRAME_PROCESSING_TIME_ESTIMATOR_H_
#define REMOTING_CODEC_FRAME_PROCESSING_TIME_ESTIMATOR_H_

#include "base/containers/circular_deque.h"
#include "base/time/time.h"
#include "remoting/base/running_samples.h"
#include "remoting/codec/webrtc_video_encoder.h"

namespace remoting {

// Estimator of frame processing time. It considers both frame size and
// bandwidth, and provides an estimation of the time required from capturing to
// reaching the client.
class FrameProcessingTimeEstimator {
 public:
  FrameProcessingTimeEstimator();
  virtual ~FrameProcessingTimeEstimator();

  // Marks the finish of encoding a frame. The frame must have a FrameStats
  // object, otherwise the frame will be ignored.
  void FinishFrame(const WebrtcVideoEncoder::EncodedFrame& frame);

  // Sets the estimated network bandwidth. Negative |bandwidth_kbps| will be
  // ignored.
  void SetBandwidthKbps(int bandwidth_kbps);

  // Returns the estimated processing time of a frame. The TimeDelta includes
  // both capturing and encoding time.
  base::TimeDelta EstimatedProcessingTime(bool key_frame) const;

  // Returns the estimated transit time of a frame. Returns a fairly large value
  // if no bandwidth estimations received.
  base::TimeDelta EstimatedTransitTime(bool key_frame) const;

  // Returns the average bandwidth in kbps. Never returns negative value.
  // TODO(zijiehe): This should be removed once we have a reliable bandwidth
  // estimator. Do not use this function explicitly except for unit-testing.
  int AverageBandwidthKbps() const;

  // Returns the estimated size of the next frame without needing to know
  // whether it will be a key-frame or not: this function considers the ratio of
  // key-frames to non-key-frames. Returns 0 if there are no records. Never
  // returns negative values.
  int EstimatedFrameSize() const;

  // Returns the estimated processing time of the next frame without needing to
  // know whether it will be a key-frame or not: this function considers the
  // ratio of the key-frames to non-key-frames. Returns 0 if there are no
  // records.
  base::TimeDelta EstimatedProcessingTime() const;

  // Returns the estimated transit time of the next frame without needing to
  // know whether it will be a key-frame or not: this function considers the
  // ratio of the key-frames to non-key-frames. Returns 0 if there are no
  // records. Returns a fairly large value if no bandwidth estimations received.
  base::TimeDelta EstimatedTransitTime() const;

  // Returns the average interval between two frames based on recent frame
  // samples. Returns 0 if there are less than two records.
  base::TimeDelta RecentAverageFrameInterval() const;

  // Returns the frame rate based on recent frame samples. Never returns zero
  // or negative value.
  int RecentFrameRate() const;

  // Returns the predicted frame rate rounding in ceiling during the coming
  // second (FPS). Never returns zero or negative value.
  int PredictedFrameRate() const;

  // Min(RecentFrameRate(), PredictedFrameRate()).
  int EstimatedFrameRate() const;

 private:
  // A virtual function to replace base::TimeTicks::Now() for testing.
  virtual base::TimeTicks Now() const;

  RunningSamples delta_frame_processing_us_;
  RunningSamples delta_frame_size_;
  RunningSamples key_frame_processing_us_;
  RunningSamples key_frame_size_;

  base::circular_deque<base::TimeTicks> frame_finish_ticks_;

  int64_t delta_frame_count_ = 0;
  int64_t key_frame_count_ = 0;

  // TODO(zijiehe): This should be removed once we have a reliable bandwidth
  // estimator.
  RunningSamples bandwidth_kbps_;
};

}  // namespace remoting

#endif  // REMOTING_CODEC_FRAME_PROCESSING_TIME_ESTIMATOR_H_

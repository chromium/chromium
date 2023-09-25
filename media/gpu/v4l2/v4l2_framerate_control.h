// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_FRAMERATE_CONTROL_H_
#define MEDIA_GPU_V4L2_V4L2_FRAMERATE_CONTROL_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/moving_window.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/base/video_frame.h"
#include "media/gpu/v4l2/v4l2_device.h"

namespace media {

// Some hardware decoder implementations (Qualcomm) need to know the framerate
// of the content being decoded.  On these platforms the hardware uses the
// framerate to allocate resources correctly.  The number of simultaneous
// contexts supported is dependent on hardware usage, i.e. only 1 4KP60 decode
// may be supported but if the framerate is reduce to 30 fps, 2 4KP30 clips
// can be decoded simultaneously.
class V4L2FrameRateControl {
 public:
  V4L2FrameRateControl(scoped_refptr<V4L2Device> device,
                       scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~V4L2FrameRateControl();

  // Trampoline method for VideoFrame destructor callbacks to be directed
  // to this class' task runner.
  static void RecordFrameDurationThunk(
      base::WeakPtr<V4L2FrameRateControl> weak_this,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Called from the VideoFrame destructor.  Stores the duration between
  // subsequent video frames into the moving average.
  void RecordFrameDuration();

  // Register this class as a VideoFrame destructor observer.
  void AttachToVideoFrame(scoped_refptr<VideoFrame>& video_frame);

 private:
  void UpdateFrameRate();

  scoped_refptr<V4L2Device> device_;
  const bool framerate_control_present_;
  int64_t current_frame_duration_avg_ms_;
  base::TimeTicks last_frame_display_time_;
  base::MovingAverage<base::TimeDelta, base::TimeDelta>
      frame_duration_moving_average_;

  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<V4L2FrameRateControl> weak_this_factory_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_FRAMERATE_CONTROL_H_

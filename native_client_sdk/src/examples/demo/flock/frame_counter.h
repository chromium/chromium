// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXAMPLES_DEMO_FLOCK_FRAME_COUNTER_H_
#define EXAMPLES_DEMO_FLOCK_FRAME_COUNTER_H_

class FrameCounter {
 public:
  FrameCounter()
      : frame_duration_accumulator_(0),
        frame_count_(0),
        frames_per_second_(0) {}
  ~FrameCounter() {}

  // Record the current time, which is used to compute the frame duration
  // when EndFrame() is called.
  void BeginFrame();

  // Compute the delta since the last call to BeginFrame() and increment the
  // frame count.  Update the frame rate whenever the prescribed number of
  // frames have been counted, or at least one second of simulator time has
  // passed, whichever is less.
  void EndFrame();

  // Reset the frame counters back to 0.
  void Reset();

  // The current frame rate.  Note that this is 0 for the first second in
  // the accumulator, and is updated every 100 frames (and at least once
  // every second of simulation time or so).
  double frames_per_second() const {
    return frames_per_second_;
  }

 private:
  static const double kMicroSecondsPerSecond = 1000000.0;
  static const int32_t kFrameRateRefreshCount = 100;

  double frame_duration_accumulator_;  // Measured in microseconds.
  int32_t frame_count_;
  double frame_start_;
  double frames_per_second_;
};

#endif  // EXAMPLES_DEMO_FLOCK_FRAME_COUNTER_H_

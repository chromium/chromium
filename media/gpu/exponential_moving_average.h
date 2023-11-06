// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_EXPONENTIAL_MOVING_AVERAGE_H_
#define MEDIA_GPU_EXPONENTIAL_MOVING_AVERAGE_H_

#include <algorithm>
#include <memory>

#include "base/time/time.h"
#include "media/gpu/media_gpu_export.h"

namespace media {

// An Exponential Moving Average filter.
// It is an implementation of exponential moving average filter with a time
// constant. The effective window size equals to the time elapsed since the
// first sample was added, until the maximum window size is reached. This makes
// a difference to the standard exponential moving average filter
// implementation. Alpha, the time constant, is calculated as a ratio between
// the time period passed from the last added sample and the effective window
// size.
//     mean += alpha * (value - mean)
//     mean_square += alpha * (value^2 - mean_square)
//     std_dev = sqrt(mean_square - mean^2)
//     alpha = elapsed_time / curr_window_size
class MEDIA_GPU_EXPORT ExponentialMovingAverage {
 public:
  explicit ExponentialMovingAverage(base::TimeDelta max_window_size);
  ~ExponentialMovingAverage();

  ExponentialMovingAverage(const ExponentialMovingAverage& other) = delete;
  ExponentialMovingAverage& operator=(const ExponentialMovingAverage& other) =
      delete;

  base::TimeDelta curr_window_size() const { return curr_window_size_; }
  base::TimeDelta max_window_size() const { return max_window_size_; }

  float mean() const { return mean_; }

  void update_max_window_size(base::TimeDelta max_window_size) {
    max_window_size_ = max_window_size;
  }

  // Adds a new value to the filter. The T type is casted to the float value.
  // Elapsed time is the period between the current and previous sample.
  template <typename T>
  void AddValue(T value, base::TimeDelta elapsed_time) {
    float float_value = static_cast<float>(value);
    // The minimum window size is 1ms. This is to avoid division by zero.
    curr_window_size_ = std::clamp(curr_window_size_ + elapsed_time,
                                   base::Milliseconds(1), max_window_size_);
    float alpha = static_cast<float>(std::min(
        elapsed_time.InMillisecondsF() / curr_window_size_.InMillisecondsF(),
        1.0));
    mean_ += alpha * (float_value - mean_);
    mean_square_ += alpha * (float_value * float_value - mean_square_);
  }

  float GetStdDeviation() const;

 private:
  // Mean of values.
  float mean_ = 0.0f;
  // Mean of squared values.
  float mean_square_ = 0.0f;

  // Effective window size.
  base::TimeDelta curr_window_size_;
  // Max window size.
  base::TimeDelta max_window_size_;
};

}  // namespace media

#endif  // MEDIA_GPU_EXPONENTIAL_MOVING_AVERAGE_H_

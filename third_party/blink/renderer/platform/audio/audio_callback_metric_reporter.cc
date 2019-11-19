// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/audio/audio_callback_metric_reporter.h"

#include "third_party/blink/renderer/platform/audio/audio_utilities.h"

namespace blink {

void AudioCallbackMetricReporter::Initialize(
    int callback_buffer_size, float sample_rate) {
  DCHECK_GT(callback_buffer_size, 0);
  DCHECK_GT(sample_rate, 0);

  metric_.expected_callback_interval =
      callback_buffer_size / static_cast<double>(sample_rate);

  // Prime the mean interval with the expected one.
  metric_.mean_callback_interval = metric_.expected_callback_interval;

  // Calculates |alpha_| based on the specified time constant. Instead of
  // the sample rate, we use "callbacks per second".
  alpha_ = audio_utilities::DiscreteTimeConstantForSampleRate(
      time_constant_,
      1.0 / metric_.expected_callback_interval);
}

void AudioCallbackMetricReporter::BeginTrace() {
  callback_start_time_ = base::TimeTicks::Now();

  // If this is the first callback, the previous timestamps are not valid.
  if (metric_.number_of_callbacks == 0) {
    previous_callback_start_time_ =
        callback_start_time_ -
        base::TimeDelta::FromSecondsD(metric_.expected_callback_interval);

    // Let's assume that the previous render duration is zero.
    previous_render_end_time_ = previous_callback_start_time_;
  }

  UpdateMetric();
}

void AudioCallbackMetricReporter::EndTrace() {
  previous_render_end_time_ = base::TimeTicks::Now();
  previous_callback_start_time_ = callback_start_time_;
}

void AudioCallbackMetricReporter::UpdateMetric() {
  metric_.number_of_callbacks++;

  // Calculate the callback interval between callback(n-1) and callback(n) and
  // the render duration of previous render quantum.
  callback_interval_ =
      (callback_start_time_ - previous_callback_start_time_).InSecondsF();
  render_duration_ =
      (previous_render_end_time_ - previous_callback_start_time_)
          .InSecondsF();

  // Calculates the instantaneous render capacity.
  metric_.render_capacity = render_duration_ / callback_interval_;

  // The algorithm for exponentially-weighted mean and variance:
  // http://people.ds.cam.ac.uk/fanf2/hermes/doc/antiforgery/stats.pdf (p. 8)
  double diff = callback_interval_ - metric_.mean_callback_interval;
  double increment = alpha_ * diff;
  metric_.mean_callback_interval += increment;
  metric_.variance_callback_interval =
      (1 - alpha_) * (metric_.variance_callback_interval + diff * increment);
}

}  // namespace blink

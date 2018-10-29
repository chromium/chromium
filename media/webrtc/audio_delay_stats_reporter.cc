// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/audio_delay_stats_reporter.h"

#include <numeric>

#include "base/metrics/histogram_macros.h"

namespace media {

namespace {

// No input data or overflow checks are made here. Input values should be small
// compared to the int range, and number of values should not be too large.
int CalculateVariance(const std::vector<int>& values) {
  DCHECK_GT(values.size(), 1ul);
  if (values.size() <= 1)
    return 0;

  const int mean =
      std::accumulate(values.begin(), values.end(), 0) / values.size();

  int mean_diff_square_sum = 0;
  for (auto value : values) {
    const int mean_diff = value - mean;
    mean_diff_square_sum += mean_diff * mean_diff;
  }

  return mean_diff_square_sum / (values.size() - 1);
}

}  // namespace

AudioDelayStatsReporter::AudioDelayStatsReporter(int variance_window_size)
    : variance_window_size_(variance_window_size),
      delay_histogram_min_(base::TimeDelta::FromMilliseconds(1)),
      delay_histogram_max_(base::TimeDelta::FromMilliseconds(500)) {
  DCHECK_GT(variance_window_size_, 1);
  capture_delays_ms_.reserve(variance_window_size_);
  render_delays_ms_.reserve(variance_window_size_);
  total_delays_ms_.reserve(variance_window_size_);
  DETACH_FROM_THREAD(thread_checker_);
}

AudioDelayStatsReporter::~AudioDelayStatsReporter() {}

void AudioDelayStatsReporter::ReportDelay(base::TimeDelta capture_delay,
                                          base::TimeDelta render_delay) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  const base::TimeDelta total_delay = capture_delay + render_delay;

  UMA_HISTOGRAM_CUSTOM_TIMES("Media.Audio.Processing.CaptureDelayMs",
                             capture_delay, delay_histogram_min_,
                             delay_histogram_max_, 50);
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.Audio.Processing.RenderDelayMs",
                             render_delay, delay_histogram_min_,
                             delay_histogram_max_, 50);
  UMA_HISTOGRAM_CUSTOM_TIMES("Media.Audio.Processing.TotalDelayMs", total_delay,
                             delay_histogram_min_, delay_histogram_max_, 50);

  capture_delays_ms_.push_back(capture_delay.InMilliseconds());
  render_delays_ms_.push_back(render_delay.InMilliseconds());
  total_delays_ms_.push_back(total_delay.InMilliseconds());

  if (capture_delays_ms_.size() ==
      static_cast<unsigned long>(variance_window_size_)) {
    DCHECK_EQ(render_delays_ms_.size(), capture_delays_ms_.size());
    DCHECK_EQ(total_delays_ms_.size(), capture_delays_ms_.size());

    int delay_variance = CalculateVariance(capture_delays_ms_);
    UMA_HISTOGRAM_CUSTOM_COUNTS("Media.Audio.Processing.CaptureDelayVarianceMs",
                                delay_variance, 1, 500, 50);
    capture_delays_ms_.clear();

    delay_variance = CalculateVariance(render_delays_ms_);
    UMA_HISTOGRAM_CUSTOM_COUNTS("Media.Audio.Processing.RenderDelayVarianceMs",
                                delay_variance, 1, 500, 50);
    render_delays_ms_.clear();

    delay_variance = CalculateVariance(total_delays_ms_);
    UMA_HISTOGRAM_CUSTOM_COUNTS("Media.Audio.Processing.TotalDelayVarianceMs",
                                delay_variance, 1, 500, 50);
    total_delays_ms_.clear();
  }
}

}  // namespace media

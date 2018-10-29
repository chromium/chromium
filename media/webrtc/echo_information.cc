// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/webrtc/echo_information.h"

#include "base/metrics/histogram_macros.h"
#include "third_party/webrtc/modules/audio_processing/include/audio_processing.h"

namespace media {

EchoInformation::EchoInformation()
    : divergent_filter_stats_time_ms_(0),
      num_divergent_filter_fraction_(0),
      num_non_zero_divergent_filter_fraction_(0) {}

EchoInformation::~EchoInformation() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ReportAndResetAecDivergentFilterStats();
}

void EchoInformation::UpdateAecStats(
    const webrtc::AudioProcessingStats& audio_processing_stats) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (!audio_processing_stats.divergent_filter_fraction) {
    return;
  }

  divergent_filter_stats_time_ms_ += webrtc::AudioProcessing::kChunkSizeMs;
  if (divergent_filter_stats_time_ms_ <
      100 * webrtc::AudioProcessing::kChunkSizeMs) {  // 1 second
    return;
  }

  double divergent_filter_fraction =
      *audio_processing_stats.divergent_filter_fraction;
  // If not yet calculated, |metrics.divergent_filter_fraction| is -1.0. After
  // being calculated the first time, it is updated periodically.
  if (divergent_filter_fraction < 0.0f) {
    DCHECK_EQ(num_divergent_filter_fraction_, 0);
    return;
  }
  if (divergent_filter_fraction > 0.0f) {
    ++num_non_zero_divergent_filter_fraction_;
  }
  ++num_divergent_filter_fraction_;
  divergent_filter_stats_time_ms_ = 0;
}

void EchoInformation::ReportAndResetAecDivergentFilterStats() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (num_divergent_filter_fraction_ == 0)
    return;

  int non_zero_percent = 100 * num_non_zero_divergent_filter_fraction_ /
                         num_divergent_filter_fraction_;
  UMA_HISTOGRAM_PERCENTAGE("WebRTC.AecFilterHasDivergence", non_zero_percent);

  divergent_filter_stats_time_ms_ = 0;
  num_non_zero_divergent_filter_fraction_ = 0;
  num_divergent_filter_fraction_ = 0;
}

}  // namespace media

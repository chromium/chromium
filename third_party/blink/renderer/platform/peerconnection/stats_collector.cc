// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/stats_collector.h"

#include <algorithm>

#include "base/check.h"
#include "base/logging.h"
#include "media/base/video_codecs.h"

namespace blink {
namespace {
// Histogram parameters.
constexpr float kProcessingTimeHistogramMinValue_ms = 1.0;
constexpr float kProcessingTimeHistogramMaxValue_ms = 35;
constexpr wtf_size_t kProcessingTimeHistogramBuckets = 80;
constexpr float kProcessingTimePercentileToReport = 0.99;

// Report intermediate results every 15 seconds.
constexpr base::TimeDelta kProcessingStatsReportingPeriod = base::Seconds(15);

}  // namespace

StatsCollector::StatsCollector(bool is_decode,
                               media::VideoCodecProfile codec_profile,
                               StoreProcessingStatsCB stats_callback)
    : is_decode_(is_decode),
      codec_profile_(codec_profile),
      stats_callback_(stats_callback) {
  DVLOG(3) << __func__ << " (IsDecode: " << is_decode_ << ", "
           << media::GetProfileName(codec_profile_) << ")";
  ClearStatsCollection();
}

void StatsCollector::StartStatsCollection() {
  DVLOG(3) << __func__;
  processing_time_ms_histogram_ = std::make_unique<LinearHistogram>(
      kProcessingTimeHistogramMinValue_ms, kProcessingTimeHistogramMaxValue_ms,
      kProcessingTimeHistogramBuckets);
  last_report_ = base::TimeTicks();
}

void StatsCollector::ClearStatsCollection() {
  DVLOG(3) << __func__;
  processing_time_ms_histogram_.reset();
  number_of_keyframes_ = 0;
  current_stats_key_ = {is_decode_, codec_profile_, 0,
                        /*hw_accelerated=*/false};
}

void StatsCollector::ReportStats() const {
  DCHECK(processing_time_ms_histogram_);
  VideoStats stats = {
      static_cast<int>(processing_time_ms_histogram_->NumValues()),
      static_cast<int>(number_of_keyframes_),
      processing_time_ms_histogram_->GetPercentile(
          kProcessingTimePercentileToReport)};
  DVLOG(3) << __func__ << " IsDecode: " << current_stats_key_.is_decode
           << ", Pixel size: " << current_stats_key_.pixel_size
           << ", HW: " << current_stats_key_.hw_accelerated
           << ", P99: " << stats.p99_processing_time_ms
           << " ms, frames: " << stats.frame_count
           << ", key frames:: " << stats.key_frame_count;

  stats_callback_.Run(current_stats_key_, stats);
}

void StatsCollector::AddProcessingTime(int pixel_size,
                                       bool is_hardware_accelerated,
                                       const float processing_time_ms,
                                       size_t new_keyframes,
                                       const base::TimeTicks& now) {
  DCHECK(processing_time_ms_histogram_);
  if (pixel_size == current_stats_key_.pixel_size &&
      is_hardware_accelerated == current_stats_key_.hw_accelerated) {
    // Store data.
    processing_time_ms_histogram_->Add(processing_time_ms);
    number_of_keyframes_ += new_keyframes;
  } else {
    // New config.
    if (samples_collected() >= kMinSamplesThreshold) {
      // Report data if enough samples have been collected.
      ReportStats();
    }
    if (samples_collected() > 0) {
      // No need to start over unless some samples have been collected.
      StartStatsCollection();
    }
    current_stats_key_.pixel_size = pixel_size;
    current_stats_key_.hw_accelerated = is_hardware_accelerated;
  }

  // Report data regularly if enough samples have been collected.
  if (samples_collected() >= kMinSamplesThreshold &&
      (now - last_report_) > kProcessingStatsReportingPeriod) {
    // Report intermediate values.
    last_report_ = now;
    ReportStats();

    if (samples_collected() >= kMaxSamplesThreshold) {
      // Stop collecting more stats if we've reached the max samples threshold.
      DVLOG(3) << "Enough samples collected, stop stats collection.";
      processing_time_ms_histogram_.reset();
      stats_collection_finished_ = true;
    }
  }
}

}  // namespace blink

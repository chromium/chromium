// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/video_bitrate_suggester.h"

#include <algorithm>
#include <limits>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "media/base/media_switches.h"
#include "media/cast/constants.h"

namespace media::cast {

VideoBitrateSuggester::VideoBitrateSuggester(
    const FrameSenderConfig& config,
    GetVideoNetworkBandwidthCB get_bitrate_cb)
    : get_bandwidth_cb_(std::move(get_bitrate_cb)),
      min_bitrate_(config.min_bitrate),
      max_bitrate_(config.max_bitrate),
      max_frame_rate_(config.max_frame_rate),
      suggested_bitrate_(max_bitrate_) {
  CHECK_GE(max_bitrate_, min_bitrate_);
}

VideoBitrateSuggester::~VideoBitrateSuggester() = default;

int VideoBitrateSuggester::GetSuggestedBitrate() {
  // The bitrate retrieved from the callback is based on network usage, however
  // we also need to consider how well this device is handling encoding at
  // this bitrate overall.
  const int suggested_bitrate =
      std::min(get_bandwidth_cb_.Run(), suggested_bitrate_);

  // Honor the config boundaries.
  return std::clamp(suggested_bitrate, min_bitrate_, max_bitrate_);
}

void VideoBitrateSuggester::RecordShouldDropNextFrame(bool should_drop) {
  ++frames_requested_;
  if (should_drop) {
    ++frames_dropped_;
  }

  if (base::FeatureList::IsEnabled(
          media::kCastStreamingExponentialVideoBitrateAlgorithm)) {
    UpdateSuggestionUsingExponentialAlgorithm();
  } else {
    UpdateSuggestionUsingLinearAlgorithm();
  }
}

void VideoBitrateSuggester::UpdateSuggestionUsingExponentialAlgorithm() {
  // This is the V2 implementation of the exponential algorithm.
  int window_size =
      media::kCastStreamingExponentialVideoBitrateAlgorithmWindowSize.Get();
  const double multiplier =
      media::
          kCastStreamingExponentialVideoBitrateAlgorithmDynamicWindowMultiplier
              .Get();
  if (multiplier > 0.0) {
    window_size = std::max(1, static_cast<int>(max_frame_rate_ * multiplier));
  }

  if (frames_requested_ >= window_size) {
    const int drop_threshold =
        media::kCastStreamingExponentialVideoBitrateAlgorithmDropThreshold
            .Get();
    const double increase_factor =
        media::kCastStreamingExponentialVideoBitrateAlgorithmIncreaseFactor
            .Get();
    const double decrease_factor =
        media::kCastStreamingExponentialVideoBitrateAlgorithmDecreaseFactor
            .Get();

    suggested_bitrate_ =
        (frames_dropped_ > drop_threshold)
            ? std::max<int>(min_bitrate_, suggested_bitrate_ * decrease_factor)
            : std::min<int>(max_bitrate_, suggested_bitrate_ * increase_factor);

    // Reset the frame counts to start a new window.
    frames_requested_ = 0;
    frames_dropped_ = 0;
  }
}

void VideoBitrateSuggester::UpdateSuggestionUsingLinearAlgorithm() {
  static constexpr int kWindowSize = 100;
  if (frames_requested_ == kWindowSize) {
    // If more than 2% of frames were dropped, decrease the bitrate.
    // 1% is common on even good WiFi, so 2% is a better threshold for
    // sustained performance issues.
    static constexpr int kDropThreshold = 2;

    static constexpr int kBitrateSteps = 8;
    const int adjustment = (max_bitrate_ - min_bitrate_) / kBitrateSteps;

    suggested_bitrate_ =
        (frames_dropped_ > kDropThreshold)
            ? std::max(min_bitrate_, suggested_bitrate_ - adjustment)
            : std::min(max_bitrate_, suggested_bitrate_ + adjustment);

    // Reset the frame counts to start a new window.
    frames_requested_ = 0;
    frames_dropped_ = 0;
  }
}

}  // namespace media::cast

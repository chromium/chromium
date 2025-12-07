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
  static constexpr int kWindowSize = 30;
  if (frames_requested_ == kWindowSize) {
    // Be more conservative about increasing than decreasing the bitrate.
    constexpr double kIncreaseFactor = 1.1;
    constexpr double kDecreaseFactor = 0.7;

    // Dropping any frames is a bad sign.
    suggested_bitrate_ =
        (frames_dropped_ > 0)
            ? std::max<int>(min_bitrate_, suggested_bitrate_ * kDecreaseFactor)
            : std::min<int>(max_bitrate_, suggested_bitrate_ * kIncreaseFactor);

    // Reset the frame counts to start a new window.
    frames_requested_ = 0;
    frames_dropped_ = 0;
  }
}

void VideoBitrateSuggester::UpdateSuggestionUsingLinearAlgorithm() {
  static constexpr int kWindowSize = 100;
  if (frames_requested_ == kWindowSize) {
    static constexpr int kBitrateSteps = 8;
    const int adjustment = (max_bitrate_ - min_bitrate_) / kBitrateSteps;

    // Dropping any frames is a bad sign.
    suggested_bitrate_ =
        (frames_dropped_ > 0)
            ? std::max(min_bitrate_, suggested_bitrate_ - adjustment)
            : std::min(max_bitrate_, suggested_bitrate_ + adjustment);

    // Reset the frame counts to start a new window.
    frames_requested_ = 0;
    frames_dropped_ = 0;
  }
}

}  // namespace media::cast

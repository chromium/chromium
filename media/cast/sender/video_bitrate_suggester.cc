// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/sender/video_bitrate_suggester.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "media/base/media_switches.h"
#include "media/cast/common/openscreen_conversion_helpers.h"
#include "media/cast/common/sender_encoded_frame.h"
#include "media/cast/constants.h"

namespace media::cast {

VideoBitrateSuggester::VideoBitrateSuggester(
    const FrameSenderConfig& config,
    FrameSender::GetSuggestedVideoBitrateCB get_bitrate_cb)
    : get_bitrate_cb_(std::move(get_bitrate_cb)),
      min_bitrate_(config.min_bitrate),
      max_bitrate_(config.max_bitrate),
      suggested_max_bitrate_(max_bitrate_) {}

VideoBitrateSuggester::~VideoBitrateSuggester() = default;

int VideoBitrateSuggester::GetSuggestedBitrate() {
  // Skip the more complicated calculation if the feature is not enabled.
  if (!base::FeatureList::IsEnabled(
          media::kOpenscreenVideoBitrateFactorInFrameDrops)) {
    return get_bitrate_cb_.Run();
  }

  // The bitrate retrieved from the callback is based on network usage, however
  // we also need to consider how well this device is handling encoding at
  // this bitrate overall.
  const int suggested_bitrate =
      std::min(get_bitrate_cb_.Run(), suggested_max_bitrate_);

  // Honor the config boundaries.
  return std::clamp(suggested_bitrate, min_bitrate_, max_bitrate_);
}

void VideoBitrateSuggester::RecordShouldDropNextFrame(bool should_drop) {
  // Nothing to do if frame drop logic is disabled.
  if (!base::FeatureList::IsEnabled(
          media::kOpenscreenVideoBitrateFactorInFrameDrops)) {
    return;
  }

  ++number_of_frames_requested_;
  if (should_drop) {
    ++number_of_frames_dropped_;
  }

  // We don't want to change the bitrate too frequently in order to give
  // things time to adjust, so only adjust every 100 frames (about 3 seconds
  // at 30FPS).
  constexpr int kWindowSize = 100;
  if (number_of_frames_requested_ == kWindowSize) {
    constexpr int kBitrateSteps = 8;
    DCHECK_GE(max_bitrate_, min_bitrate_);
    const int adjustment = (max_bitrate_ - min_bitrate_) / kBitrateSteps;

    // Generally speaking we shouldn't be dropping any frames, so even one is
    // a bad sign.
    if (number_of_frames_dropped_ > 0) {
      suggested_max_bitrate_ =
          std::max(min_bitrate_, suggested_max_bitrate_ - adjustment);
    } else {
      suggested_max_bitrate_ =
          std::min(max_bitrate_, suggested_max_bitrate_ + adjustment);
    }

    // Reset the recorded frame drops to start a new window.
    number_of_frames_requested_ = 0;
    number_of_frames_dropped_ = 0;
  }
}
}  // namespace media::cast

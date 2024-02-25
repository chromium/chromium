// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/mediastream/video_track_adapter_settings.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/check_op.h"

namespace blink {

VideoTrackAdapterSettings::VideoTrackAdapterSettings()
    : VideoTrackAdapterSettings(std::nullopt,
                                0.0,
                                std::numeric_limits<double>::max(),
                                std::nullopt) {}

VideoTrackAdapterSettings::VideoTrackAdapterSettings(
    const gfx::Size& target_size,
    std::optional<double> max_frame_rate)
    : VideoTrackAdapterSettings(target_size, 0.0, HUGE_VAL, max_frame_rate) {}

VideoTrackAdapterSettings::VideoTrackAdapterSettings(
    std::optional<gfx::Size> target_size,
    double min_aspect_ratio,
    double max_aspect_ratio,
    std::optional<double> max_frame_rate)
    : target_size_(std::move(target_size)),
      min_aspect_ratio_(min_aspect_ratio),
      max_aspect_ratio_(max_aspect_ratio),
      max_frame_rate_(max_frame_rate) {
  DCHECK(!target_size_ ||
         (target_size_->width() >= 0 && target_size_->height() >= 0));
  DCHECK(!std::isnan(min_aspect_ratio_));
  DCHECK_GE(min_aspect_ratio_, 0.0);
  DCHECK(!std::isnan(max_aspect_ratio_));
  DCHECK_GE(max_aspect_ratio_, min_aspect_ratio_);
  DCHECK(!max_frame_rate_ || !std::isnan(*max_frame_rate_));
  DCHECK(!max_frame_rate_ || *max_frame_rate_ >= 0.0);
}

VideoTrackAdapterSettings::VideoTrackAdapterSettings(
    const VideoTrackAdapterSettings& other) = default;
VideoTrackAdapterSettings& VideoTrackAdapterSettings::operator=(
    const VideoTrackAdapterSettings& other) = default;

bool VideoTrackAdapterSettings::operator==(
    const VideoTrackAdapterSettings& other) const {
  return target_size_ == other.target_size_ &&
         min_aspect_ratio_ == other.min_aspect_ratio_ &&
         max_aspect_ratio_ == other.max_aspect_ratio_ &&
         max_frame_rate_ == other.max_frame_rate_;
}

}  // namespace blink

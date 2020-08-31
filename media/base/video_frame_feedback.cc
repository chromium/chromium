// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/video_frame_feedback.h"

#include <cmath>

namespace media {

VideoFrameFeedback::VideoFrameFeedback() = default;
VideoFrameFeedback::VideoFrameFeedback(const VideoFrameFeedback& other) =
    default;

VideoFrameFeedback::VideoFrameFeedback(double resource_utilization,
                                       float max_framerate_fps,
                                       int max_pixels)
    : resource_utilization(resource_utilization),
      max_framerate_fps(max_framerate_fps),
      max_pixels(max_pixels) {}

void VideoFrameFeedback::Combine(const VideoFrameFeedback& other) {
  // Take maximum of non-negative and finite |resource_utilization| values.
  if (other.resource_utilization >= 0 &&
      std::isfinite(other.resource_utilization)) {
    if (!std::isfinite(resource_utilization) ||
        resource_utilization < other.resource_utilization) {
      resource_utilization = other.resource_utilization;
    }
  }

  // Take minimum non-negative max_pixels value to satisfy both constraints.
  if (other.max_pixels > 0 &&
      (max_pixels <= 0 || max_pixels > other.max_pixels)) {
    max_pixels = other.max_pixels;
  }

  // Take minimum of non-negative max_framerate_fps.
  if (other.max_framerate_fps >= 0.0 &&
      (max_framerate_fps < 0.0 || max_framerate_fps > other.max_framerate_fps))
    max_framerate_fps = other.max_framerate_fps;
}

bool VideoFrameFeedback::Empty() const {
  return !std::isfinite(max_framerate_fps) &&
         max_pixels == std::numeric_limits<int>::max() &&
         (resource_utilization < 0.0);
}

}  // namespace media
